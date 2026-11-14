// SPDX-License-Identifier: MIT
// Runs the menu's Minesweeper from its committed header (build-c64.test.mjs keeps that header in step
// with the source) and checks the game against a reference written here.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Mos6502 } from '../lib/mos6502.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');

const header = fs.readFileSync(path.join(root, 'Source/Teensy/TRMenuFiles/ROMs/Minesweeper.prg.h'), 'utf8');
const prg = [...header.slice(header.indexOf('{')).matchAll(/0x([0-9a-f]{2})/gi)].map((m) => parseInt(m[1], 16));
const LOAD = prg[0] | (prg[1] << 8);

// The source's "name = $hex" lines: its entry points, variables and constants
const S = Object.fromEntries(
  [...fs.readFileSync(path.join(root, 'Source/C64/Minesweeper/source/Minesweeper.asm'), 'utf8').matchAll(/^(\w+)\s*=\s*\$([0-9a-f]+)/gim)]
    .map((m) => [m[1], parseInt(m[2], 16)]),
);
const STATE = { READY: 0, PLAYING: 1, WON: 2, LOST: 3 };
const LEVELS = [
  { rows: 9, cols: 9, mines: 10, left: 15 },
  { rows: 16, cols: 16, mines: 40, left: 12 },
  { rows: 16, cols: 30, mines: 99, left: 5 },
];
const TOP = 5;
const CALLER = 0x0200;
const id = (text) => [...text].map((c) => c.charCodeAt(0));

// The TR's page as USBMouse.c and the block device serve it
function trPage(mouse) {
  return (offset) => {
    if (offset === 0) return mouse.dx & 0xff;
    if (offset === 1) return mouse.dy & 0xff;
    if (offset === 2) return mouse.buttons;
    if (offset >= 3 && offset < 6) return id('TRM')[offset - 3];
    if (offset >= 0x10 && offset < 0x13) return id('TRB')[offset - 0x10];
    return 0;
  };
}

class Game {
  constructor({ level = 0, seed = 0x1234, pages = {} } = {}) {
    this.ram = new Uint8Array(0x10000);
    this.ram.set(prg.slice(2), LOAD);
    this.pages = pages;
    this.keys = new Set();  // [column, row] of the C64 keyboard matrix, as "c,r"
    this.port1 = 0;         // bits port 1 holds low on $DC01: 0x10 fire (a 1351's left), 0x01 up (its right)
    this.pot = [0, 0];      // SID's POTX, POTY
    this.cpu = new Mos6502({
      read: (a) => {
        if (a === 0xdc01) return this.portB();
        if (a === 0xd419 || a === 0xd41a) return this.pot[a - 0xd419];
        return this.pages[a & 0xff00] ? this.pages[a & 0xff00](a & 0xff) : this.ram[a];
      },
      write: (a, v) => { if (!this.pages[a & 0xff00]) this.ram[a] = v; },
    });
    this.ram[S.level] = level;
    this.ram[S.seed] = seed & 0xff;
    this.ram[S.seed + 1] = seed >> 8;
    this.call(S.API_NEWGAME);
  }

  // CIA 1's port B: a held key pulls its row low while $DC00 drives its column low; port 1 pulls rows regardless
  portB() {
    const columns = this.ram[0xdc00];
    let rows = 0xff;
    for (const key of this.keys) {
      const [c, r] = key.split(',').map(Number);
      if (!(columns & (1 << c))) rows &= ~(1 << r);
    }
    return rows & ~this.port1;
  }

  call(address) {
    this.ram.set([0x20, address & 0xff, address >> 8], CALLER);
    this.cpu.pc = CALLER;
    this.cpu.s = 0xff;
    this.cpu.run((c) => c.pc === CALLER + 3, 20_000_000);
  }

  at(entry, r, c) {
    this.ram[S.row] = r;
    this.ram[S.col] = c;
    this.call(entry);
  }

  get rows() { return this.ram[S.rows]; }
  get cols() { return this.ram[S.cols]; }
  get state() { return this.ram[S.state]; }
  get safeLeft() { return this.ram[S.safeLeft] | (this.ram[S.safeLeft + 1] << 8); }
  get flags() { return this.ram[S.flags] | (this.ram[S.flags + 1] << 8); }
  cell(r, c) { return this.ram[S.BOARD + r * 32 + c]; }
  isMine(r, c) { return (this.cell(r, c) & 0x80) !== 0; }
  isOpen(r, c) { return (this.cell(r, c) & 0x40) !== 0; }
  isFlag(r, c) { return (this.cell(r, c) & 0x20) !== 0; }
  count(r, c) { return this.cell(r, c) & 0x0f; }

  *cells() {
    for (let r = 0; r < this.rows; r++) for (let c = 0; c < this.cols; c++) yield [r, c];
  }

  *neighbors(r, c) {
    for (let dr = -1; dr <= 1; dr++) {
      for (let dc = -1; dc <= 1; dc++) {
        const nr = r + dr, nc = c + dc;
        if ((dr || dc) && nr >= 0 && nr < this.rows && nc >= 0 && nc < this.cols) yield [nr, nc];
      }
    }
  }

  // What opening r, c should open, by the rules, from the mines the game placed
  expectedOpen(r, c, open = new Set()) {
    const todo = [[r, c]];
    while (todo.length) {
      const [tr, tc] = todo.pop();
      const key = `${tr},${tc}`;
      if (open.has(key) || this.isFlag(tr, tc)) continue;
      open.add(key);
      if ([...this.neighbors(tr, tc)].every(([nr, nc]) => !this.isMine(nr, nc))) todo.push(...this.neighbors(tr, tc));
    }
    return open;
  }

  openSet() {
    return new Set([...this.cells()].filter(([r, c]) => this.isOpen(r, c)).map(([r, c]) => `${r},${c}`));
  }

  screen(row, col) { return this.ram[S.SCREEN + row * 40 + col]; }
}

for (const [level, spec] of LEVELS.entries()) {
  for (const seed of [0x0001, 0x1234, 0xbeef]) {
    test(`level ${level}, seed ${seed.toString(16)}: the first click places every mine away from it and opens what the rules open`, () => {
      const g = new Game({ level, seed });
      assert.equal(g.rows, spec.rows);
      assert.equal(g.cols, spec.cols);
      assert.equal(g.state, STATE.READY);
      const r = spec.rows >> 1, c = spec.cols >> 1;
      g.at(S.API_LEFT, r, c);
      assert.equal(g.state, STATE.PLAYING);

      const mines = [...g.cells()].filter(([mr, mc]) => g.isMine(mr, mc));
      assert.equal(mines.length, spec.mines);
      for (const [mr, mc] of mines) assert.ok(Math.abs(mr - r) > 1 || Math.abs(mc - c) > 1, `a mine at ${mr},${mc} beside the first click`);
      for (let x = spec.cols; x < 32; x++) for (let y = 0; y < 16; y++) assert.equal(g.ram[S.BOARD + y * 32 + x], 0, 'a cell past the board was written');
      for (const [cr, cc] of g.cells()) {
        assert.equal(g.count(cr, cc), [...g.neighbors(cr, cc)].filter(([nr, nc]) => g.isMine(nr, nc)).length, `count at ${cr},${cc}`);
      }
      assert.deepEqual(g.openSet(), g.expectedOpen(r, c));
      assert.equal(g.safeLeft, spec.rows * spec.cols - spec.mines - g.openSet().size);
    });
  }

  test(`level ${level}: opening every safe cell wins and flags every mine`, () => {
    const g = new Game({ level, seed: 0x4321 + level });
    g.at(S.API_LEFT, 0, 0);
    for (const [r, c] of g.cells()) {
      if (!g.isMine(r, c) && !g.isOpen(r, c)) g.at(S.API_LEFT, r, c);
    }
    assert.equal(g.state, STATE.WON);
    assert.equal(g.safeLeft, 0);
    assert.equal(g.flags, spec.mines);
    for (const [r, c] of g.cells()) assert.equal(g.isFlag(r, c), g.isMine(r, c));
  });
}

test('opening a mine loses, marks it, and stops play', () => {
  const g = new Game();
  g.at(S.API_LEFT, 4, 4);
  const [mr, mc] = [...g.cells()].find(([r, c]) => g.isMine(r, c));
  g.at(S.API_LEFT, mr, mc);
  assert.equal(g.state, STATE.LOST);
  assert.equal(g.cell(mr, mc) & 0x10, 0x10);
  const [sr, sc] = [...g.cells()].find(([r, c]) => !g.isMine(r, c) && !g.isOpen(r, c));
  g.at(S.API_LEFT, sr, sc);
  g.at(S.API_RIGHT, sr, sc);
  assert.equal(g.isOpen(sr, sc) || g.isFlag(sr, sc), false, 'a lost game still took a move');
});

test('a flag counts, blocks opening, and comes off again; an open cell takes no flag', () => {
  const g = new Game();
  g.at(S.API_RIGHT, 0, 0);
  assert.ok(g.isFlag(0, 0));
  assert.equal(g.flags, 1);
  g.at(S.API_LEFT, 0, 0);
  assert.equal(g.isOpen(0, 0), false);
  assert.equal(g.state, STATE.READY, 'clicking a flag placed the mines');
  g.at(S.API_RIGHT, 0, 0);
  assert.equal(g.flags, 0);
  g.at(S.API_LEFT, 4, 4);
  g.at(S.API_RIGHT, 4, 4);
  assert.equal(g.isFlag(4, 4), false);
  assert.equal(g.flags, 0);
});

test('more flags than mines show a negative count', () => {
  const g = new Game();
  for (let c = 0; c < 9; c++) for (const r of [0, 1]) g.at(S.API_RIGHT, r, c);
  assert.equal(g.flags, 18);
  // "-008" from column 1 of the status row
  assert.deepEqual([1, 2, 3, 4].map((col) => g.screen(2, col)), [0x2d, 0x30, 0x30, 0x38]);
});

// A number whose flags match opens its other neighbors; with a flag on the wrong cell it opens a mine.
function numberBesideCovered(g) {
  return [...g.cells()].find(([r, c]) => g.isOpen(r, c) && g.count(r, c) > 0
    && [...g.neighbors(r, c)].some(([nr, nc]) => !g.isOpen(nr, nc) && !g.isMine(nr, nc)));
}

test('opening around a number with its mines flagged opens the rest, by left or middle click', () => {
  for (const entry of [S.API_LEFT, S.API_MIDDLE]) {
    const g = new Game({ level: 1, seed: 0x7777 });
    g.at(S.API_LEFT, 8, 8);
    const [r, c] = numberBesideCovered(g);
    const before = g.openSet();
    for (const [nr, nc] of g.neighbors(r, c)) if (g.isMine(nr, nc)) g.at(S.API_RIGHT, nr, nc);
    g.at(entry, r, c);
    let expected = new Set(before);
    for (const [nr, nc] of g.neighbors(r, c)) if (!g.isMine(nr, nc)) expected = g.expectedOpen(nr, nc, expected);
    assert.deepEqual(g.openSet(), expected);
    assert.notEqual(g.state, STATE.LOST);
  }
});

test('opening around a number does nothing until its flags match, and loses with a wrong flag', () => {
  const g = new Game({ level: 1, seed: 0x7777 });
  g.at(S.API_LEFT, 8, 8);
  const [r, c] = numberBesideCovered(g);
  const before = g.openSet();
  g.at(S.API_MIDDLE, r, c);
  assert.deepEqual(g.openSet(), before, 'it opened with no flags');

  const mines = [...g.neighbors(r, c)].filter(([nr, nc]) => g.isMine(nr, nc));
  const safe = [...g.neighbors(r, c)].filter(([nr, nc]) => !g.isMine(nr, nc) && !g.isOpen(nr, nc));
  g.at(S.API_RIGHT, ...safe[0]);
  for (const m of mines.slice(1)) g.at(S.API_RIGHT, ...m);
  g.at(S.API_MIDDLE, r, c);
  assert.equal(g.state, STATE.LOST);
});

test('the mouse is found beside GeoRAM, beside an REU, or not at all', () => {
  const mouse = { dx: 0, dy: 0, buttons: 0 };
  const page = trPage(mouse);
  const reu = (o) => ((o & 0x1f) < 0x0b ? [0x10, 0, 0, ...id('TRM'), 0, 0, 0, 0, 0][o & 0x1f] : 0xff);
  const cases = [
    [{ 0xdf00: page }, 0xdf],
    [{ 0xdf00: reu, 0xde00: page }, 0xde],
    [{ 0xdf00: reu, 0xde00: () => 7 }, 0],
    [{}, 0],
  ];
  for (const [pages, expected] of cases) {
    const g = new Game();
    g.pages = pages;
    g.call(S.API_FINDMOUSE);
    assert.equal(g.ram[S.mregs + 1], expected);
  }
});

function mouseGame(level = 0) {
  const mouse = { dx: 0, dy: 0, buttons: 0 };
  const g = new Game({ level, pages: { 0xdf00: trPage(mouse) } });
  g.call(S.API_FINDMOUSE);
  return { g, mouse };
}

const pos = (g) => [g.ram[S.posLo] | (g.ram[S.posHi] << 8), g.ram[S.posLo + 1] | (g.ram[S.posHi + 1] << 8)];

function setPos(g, x, y) {
  g.ram[S.posLo] = x & 0xff;
  g.ram[S.posHi] = x >> 8;
  g.ram[S.posLo + 1] = y;
  g.ram[S.posHi + 1] = 0;
}

test('the mouse moves the pointer by signed steps and keeps it on the screen', () => {
  const { g, mouse } = mouseGame();
  setPos(g, 160, 100);
  Object.assign(mouse, { dx: 100, dy: -50 });
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [260, 50]);
  Object.assign(mouse, { dx: 127, dy: 127 });
  g.call(S.API_READMOUSE);
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [319, 199]);
  for (let i = 0; i < 4; i++) {
    Object.assign(mouse, { dx: -127, dy: -127 });
    g.call(S.API_READMOUSE);
  }
  assert.deepEqual(pos(g), [0, 0]);
});

test('a mouse button acts once when pressed, on the cell under the pointer', () => {
  const { g, mouse } = mouseGame();
  const spec = LEVELS[0];
  setPos(g, (spec.left + 2) * 8 + 1, (TOP + 3) * 8 + 6);
  mouse.buttons = 2;
  g.call(S.API_READMOUSE);
  assert.ok(g.isFlag(3, 2));
  g.call(S.API_READMOUSE);
  assert.ok(g.isFlag(3, 2), 'a held button flagged again');
  mouse.buttons = 0;
  g.call(S.API_READMOUSE);
  setPos(g, (spec.left + 6) * 8, (TOP + 6) * 8 + 7);
  mouse.buttons = 1;
  g.call(S.API_READMOUSE);
  assert.equal(g.state, STATE.PLAYING);
  assert.ok(g.isOpen(6, 6));
});

test('clicking a level label starts that level, and the face starts a new game', () => {
  const { g, mouse } = mouseGame();
  const click = (col, row) => {
    setPos(g, col * 8 + 2, row * 8 + 2);
    mouse.buttons = 1;
    g.call(S.API_READMOUSE);
    mouse.buttons = 0;
    g.call(S.API_READMOUSE);
  };
  click(35, 24);
  assert.deepEqual([g.rows, g.cols], [16, 30]);
  click(20, 24);
  assert.deepEqual([g.rows, g.cols], [16, 16]);
  click(4, 24);
  assert.deepEqual([g.rows, g.cols], [9, 9]);
  click(15 + 4, TOP + 4);
  assert.equal(g.state, STATE.PLAYING);
  click(20, 3);
  assert.equal(g.state, STATE.READY);
});

// The keyboard matrix as column, row
const KEY = { F1: '0,4', F3: '0,5', F5: '0,6', RETURN: '0,1', N: '4,7', SPACE: '7,4' };

function press(g, key) {
  g.keys.add(key);
  g.call(S.API_READKEYS);
  g.keys.delete(key);
  g.call(S.API_READKEYS);
}

test('the keys pick a level, start a new game, and flag', () => {
  const g = new Game();
  press(g, KEY.F5);
  assert.deepEqual([g.rows, g.cols], [16, 30]);
  press(g, KEY.F3);
  assert.deepEqual([g.rows, g.cols], [16, 16]);
  press(g, KEY.F1);
  assert.deepEqual([g.rows, g.cols], [9, 9]);
  for (const k of ['N', 'RETURN']) {
    g.at(S.API_LEFT, 4, 4);
    assert.equal(g.state, STATE.PLAYING);
    press(g, KEY[k]);
    assert.equal(g.state, STATE.READY, `${k} did not start a new game`);
  }
  setPos(g, (LEVELS[0].left + 1) * 8 + 3, (TOP + 2) * 8 + 3);
  press(g, KEY.SPACE);
  assert.ok(g.isFlag(2, 1));
});

test('a key acts once while held, and $DC00 is left selecting port 1', () => {
  const g = new Game();
  g.at(S.API_LEFT, 4, 4);
  g.keys.add(KEY.N);
  g.call(S.API_READKEYS);
  g.at(S.API_LEFT, 4, 4);
  g.call(S.API_READKEYS);
  assert.equal(g.state, STATE.PLAYING, 'a held N started a second new game');
  assert.equal(g.ram[0xdc00], 0x7f);
});

test("a 1351's buttons in port 1 are not read as F1 or Space", () => {
  const g = new Game({ level: 1 });
  g.at(S.API_LEFT, 8, 8);
  g.port1 = 0x11;
  g.call(S.API_READKEYS);
  g.port1 = 0;
  g.call(S.API_READKEYS);
  assert.deepEqual([g.rows, g.state], [16, STATE.PLAYING]);
});

test('a 1351 in port 1 moves the pointer by its POTs, modulo 64, Y counting up', () => {
  const g = new Game();
  setPos(g, 160, 100);
  g.pot = [20, 20];
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [170, 90]);
  g.pot = [10, 30];
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [165, 85]);
  g.pot = [10 + 126, 30];  // bits 6-1 wrap: +63 steps is -1
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [164, 85]);
  g.pot = [11 + 126, 31];  // bit 0 is noise
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [164, 85]);
  g.pot = [9 + 126, 29];   // in either direction
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [164, 85]);
  g.pot = [(136 + 130) & 0xff, 31];  // 65 steps is +1
  g.call(S.API_READMOUSE);
  assert.deepEqual(pos(g), [165, 85]);
});

test("a 1351's fire line clicks and its up line flags, once each press", () => {
  const g = new Game();
  setPos(g, (LEVELS[0].left + 2) * 8 + 3, (TOP + 3) * 8 + 3);
  g.port1 = 0x01;
  g.call(S.API_READMOUSE);
  g.call(S.API_READMOUSE);
  assert.ok(g.isFlag(3, 2));
  g.port1 = 0;
  g.call(S.API_READMOUSE);
  setPos(g, (LEVELS[0].left + 6) * 8 + 3, (TOP + 6) * 8 + 3);
  g.port1 = 0x10;
  g.call(S.API_READMOUSE);
  assert.ok(g.isOpen(6, 6));
});

test("a button held on either mouse is held: the other's press of it does not act again", () => {
  const { g, mouse } = mouseGame();
  setPos(g, (LEVELS[0].left + 2) * 8 + 3, (TOP + 3) * 8 + 3);
  g.port1 = 0x01;
  g.call(S.API_READMOUSE);
  assert.ok(g.isFlag(3, 2));
  for (const b of [2, 0, 2]) {
    mouse.buttons = b;
    g.call(S.API_READMOUSE);
  }
  assert.ok(g.isFlag(3, 2), "the USB mouse's right press unflagged while the 1351's was held");
});

// Screen codes as ACME's !scr writes lowercase text, read from a screen row
const scr = (text) => [...text].map((ch) => (ch >= 'a' && ch <= 'z' ? ch.charCodeAt(0) - 96 : ch.charCodeAt(0)));
const BG3 = 0xc0;
const BANNER = ' n for new game, q to quit ';
const bannerAt = (g) => [...BANNER].map((_, i) => g.screen(12, 6 + i));

test('the top row names both mice', () => {
  const g = new Game();
  assert.deepEqual([...'usb or 1351 mouse'].map((_, i) => g.screen(0, 22 + i)), scr('usb or 1351 mouse'));
});

test('the end of a game puts the new-game banner across the middle, and a new game clears it', () => {
  const g = new Game();
  g.at(S.API_LEFT, 4, 4);
  assert.notDeepEqual(bannerAt(g), scr(BANNER).map((c) => c | BG3), 'the banner is up mid-game');
  const [mr, mc] = [...g.cells()].find(([r, c]) => g.isMine(r, c));
  g.at(S.API_LEFT, mr, mc);
  assert.deepEqual(bannerAt(g), scr(BANNER).map((c) => c | BG3));
  g.call(S.API_NEWGAME);
  assert.notDeepEqual(bannerAt(g), scr(BANNER).map((c) => c | BG3));

  const w = new Game({ seed: 0x4321 });
  w.at(S.API_LEFT, 0, 0);
  for (const [r, c] of w.cells()) if (!w.isMine(r, c) && !w.isOpen(r, c)) w.at(S.API_LEFT, r, c);
  assert.equal(w.state, STATE.WON);
  assert.deepEqual(bannerAt(w), scr(BANNER).map((c) => c | BG3));
});

test('Q turns the pointer off, restores the kernal and the screen, and cold-starts BASIC', () => {
  const g = new Game();
  const COLD = 0x0300;
  const calls = [];
  g.ram[0xd015] = 3;
  for (const kernal of [0xff8a, 0xff81]) g.ram[kernal] = 0x60;  // RTS, recorded below
  g.ram.set([COLD & 0xff, COLD >> 8], 0xa000);
  g.keys.add('7,6');
  g.ram.set([0x20, S.API_READKEYS & 0xff, S.API_READKEYS >> 8], CALLER);
  g.cpu.pc = CALLER;
  g.cpu.s = 0xff;
  // The kernal's key scan enters a held key whose code differs from LSTX ($C5), which this game's IRQ
  //  never updates: BASIC would start with a Q typed after READY.
  let steps = 0;
  g.cpu.run((c) => {
    if (c.pc === 0xff8a || c.pc === 0xff81) calls.push(c.pc);
    return c.pc === COLD || ++steps === 10_000;
  }, 1_000_000);
  assert.deepEqual(calls, [], 'BASIC started while Q was still held');
  assert.equal(g.ram[0xd015], 0);
  g.keys.delete('7,6');
  g.cpu.run((c) => {
    if (c.pc === 0xff8a || c.pc === 0xff81) calls.push(c.pc);
    return c.pc === COLD;
  }, 1_000_000);
  assert.deepEqual(calls, [0xff8a, 0xff81]);
});

// The kernal's NMI entry ($FE43) is SEI, JMP ($0318): the handler starts with the 6502's own frame on the stack.
function nmi(g, column) {
  const COLD = 0x0300;
  const calls = [];
  for (const kernal of [0xff8a, 0xff81]) g.ram[kernal] = 0x60;
  g.ram.set([COLD & 0xff, COLD >> 8], 0xa000);
  g.ram.set([S.API_NMI & 0xff, S.API_NMI >> 8], 0xfffa);
  g.ram[CALLER] = 0xea;
  g.ram[0xdc00] = column;
  g.cpu.pc = CALLER;
  g.cpu.s = 0xff;
  g.cpu.a = 0x5a;
  g.cpu.nmi();
  return { COLD, calls };
}

test('RESTORE alone returns, with A and $DC00 as the interrupted code had them', () => {
  const g = new Game();
  nmi(g, 0xfe);
  g.cpu.run((c) => c.pc === CALLER, 1000);
  assert.equal(g.cpu.a, 0x5a);
  assert.equal(g.cpu.s, 0xff);
  assert.equal(g.ram[0xdc00], 0xfe);
});

test('RUN/STOP+RESTORE quits as Q does, once RUN/STOP is up', () => {
  const g = new Game();
  g.ram[0xd015] = 3;
  g.keys.add('7,7');
  const { COLD, calls } = nmi(g, 0xfe);
  let steps = 0;
  const track = (c) => { if (c.pc === 0xff8a || c.pc === 0xff81) calls.push(c.pc); };
  g.cpu.run((c) => { track(c); return c.pc === COLD || c.pc === CALLER || ++steps === 10_000; }, 1_000_000);
  assert.deepEqual(calls, [], 'BASIC started while RUN/STOP was still held');
  assert.equal(g.ram[0xd015], 0);
  g.keys.delete('7,7');
  g.cpu.run((c) => { track(c); return c.pc === COLD || c.pc === CALLER; }, 1_000_000);
  assert.equal(g.cpu.pc, COLD);
  assert.deepEqual(calls, [0xff8a, 0xff81]);
});
