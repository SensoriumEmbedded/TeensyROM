// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Mos6502 } from '../lib/mos6502.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');

const UPDATE_MOUSE = 6;
const MOUSE_ON = 0x30;
const POINTER_ON = 0x80;
const MOUSE_X = 0x3a;
const MOUSE_Y = 0x3c;
const CALLER = 0x0200;
const BESIDE_GEORAM = 0xdf00;
const BESIDE_REU = 0xde00;
const REU_REGISTERS = 0x0b;
const REU_GHOSTS = 0x1f;
const START = 100;

const id = (text) => [...text].map((c) => c.charCodeAt(0));

function readDriver(header, prefix, base) {
  const text = fs.readFileSync(path.join(root, 'Source/Teensy/GEOS', header), 'utf8');
  const code = [...text.match(new RegExp(`${prefix}Code\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16));
  return { base, code };
}

const DRIVERS = [
  ['GEOS 64', readDriver('TRMouse64.h', 'TRMouse64', 0xfe80)],
  ['GEOS 128', readDriver('TRMouse128.h', 'TRMouse128', 0xfd00)],
];

// The TR's page as USBMouse.c and the block device serve it: X and Y movement, the buttons, "TRM", and "TRB" at $10
function trPage(dx, dy) {
  const page = new Uint8Array(256);
  page.set([dx & 0xff, dy & 0xff, 0, ...id('TRM')]);
  page.set(id('TRB'), 0x10);
  return (offset) => page[offset];
}

// An REU's registers at $DF00, holding what a program left in them, ghosted every 32 bytes, the unused ones reading $FF
const reuRegisters = (regs) => (offset) => ((offset & REU_GHOSTS) < REU_REGISTERS ? regs[offset & REU_GHOSTS] : 0xff);

function update(driver, pages) {
  const ram = new Uint8Array(0x10000);
  ram.set(driver.code, driver.base);
  ram.set([0x20, (driver.base + UPDATE_MOUSE) & 0xff, (driver.base + UPDATE_MOUSE) >> 8], CALLER);
  ram[MOUSE_ON] = POINTER_ON;
  ram[MOUSE_X] = START;
  ram[MOUSE_Y] = START;
  const cpu = new Mos6502({
    read: (a) => (pages[a & 0xff00] ? pages[a & 0xff00](a & 0xff) : ram[a]),
    write: (a, v) => {
      if (pages[a & 0xff00]) throw new Error(`the driver wrote $${a.toString(16)}`);
      ram[a] = v;
    },
  });
  cpu.pc = CALLER;
  cpu.run((c) => c.pc === CALLER + 3);
  return { x: ram[MOUSE_X] - START, y: ram[MOUSE_Y] - START };
}

for (const [name, driver] of DRIVERS) {
  test(`${name}: the mouse moves by the registers beside GeoRAM, not by a GeoRAM window that shows the TR's IDs`, () => {
    const window = new Uint8Array(256);
    window.set([9, 9, 0, ...id('TRM')]);
    window.set(id('TRB'), 0x10);
    assert.deepEqual(update(driver, { [BESIDE_GEORAM]: trPage(3, 4), [BESIDE_REU]: (o) => window[o] }), { x: 3, y: 4 });
  });

  test(`${name}: the mouse moves by the registers beside an REU, even when the REU's address registers hold "TRM"`, () => {
    const reu = reuRegisters([0x10, 0, 9, ...id('TRM'), 0, 0, 0, 0, 0]);
    assert.deepEqual(update(driver, { [BESIDE_GEORAM]: reu, [BESIDE_REU]: trPage(5, 6) }), { x: 5, y: 6 });
  });

  test(`${name}: with no TR registers on either page the mouse stays put`, () => {
    const reu = reuRegisters([0x10, 0, 9, ...id('TRM'), 0, 0, 0, 0, 0]);
    assert.deepEqual(update(driver, { [BESIDE_GEORAM]: reu, [BESIDE_REU]: () => 7 }), { x: 0, y: 0 });
  });
}
