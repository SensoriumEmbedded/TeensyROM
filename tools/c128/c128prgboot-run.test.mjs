// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Mos6502 } from '../lib/mos6502.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'Source/Teensy/TRMenuFiles/ROMs/C128PrgBoot.bin.h');
const launcher = path.join(root, 'Source/Teensy/C128Prg.ino');

const MMU_CR = 0xff00;
const FUNCTION_ROM_AT_8000 = 0x08;
const SYSTEM_BANK = 0x00;
const COLD_START = 0x8000;
const BASIC_START = 0x1c01;
const BASIC_WARM_START = 0x4003;
const BASIC_RESTART = 0x0a00;
const TEXT_TOP = 0x1210;
const KEYBOARD_BUFFER = 0x034a;
const KEYBOARD_COUNT = 0xd0;
const IO1 = 0xde00;
const REG = { STREAM_DATA: 0x03, CONTROL: 0x04, SWAP_POLL: 0xfe };
const CTL = { VANISH_ROM: 0, RUNNING_PRG: 5 };
const SWAP = { BUSY: 0x00, READY: 0xff };

function firmwareRom(prgSize) {
  const text = fs.readFileSync(header, 'utf8');
  const rom = Uint8Array.from([...text.match(/C128PrgBoot_bin\[\] = \{([^}]*)\}/)[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16)));
  const define = (name) => parseInt(fs.readFileSync(launcher, 'utf8').match(new RegExp(`#define ${name}\\s+(0x[0-9a-f]+)`))[1], 16);
  const end = BASIC_START + prgSize - 2;
  rom[define('C128PRG_END_PARAM')] = end & 0xff;
  rom[define('C128PRG_END_PARAM') + 1] = end >> 8;
  return { roml: rom.subarray(0, define('C128PRG_ROML_SIZE')), romh: rom.subarray(define('C128PRG_ROML_SIZE')) };
}

function program(bodySize) {
  const prg = new Uint8Array(bodySize + 2);
  prg[0] = BASIC_START & 0xff;
  prg[1] = BASIC_START >> 8;
  for (let i = 2; i < prg.length; i++) prg[i] = (i * 13 + (i >> 8)) & 0xff;
  return prg;
}

function c128(prg, { swapBusyReads = 3 } = {}) {
  const cart = firmwareRom(prg.length);
  const ram = new Uint8Array(0x10000);
  ram[BASIC_RESTART] = BASIC_WARM_START & 0xff;
  ram[BASIC_RESTART + 1] = BASIC_WARM_START >> 8;
  const m = { ram, cr: FUNCTION_ROM_AT_8000, streamed: 0, controls: [], ioWrites: [], streamedAfterControl: false, swapReady: false, systemBankBeforeReady: false };
  let busy = swapBusyReads;

  const ioMapped = (a) => a >= 0xd000 && a < 0xe000 && !(m.cr & 0x01);
  const rom = (a) => {
    if (a >= 0x4000 && a < 0x8000 && !(m.cr & 0x02)) return 'system';
    if (a >= 0x8000 && a < 0xc000) return [ 'system', 'internal', cart.roml, null ][(m.cr >> 2) & 3];
    if (a >= 0xc000) return [ 'system', 'internal', cart.romh, null ][(m.cr >> 4) & 3];
    return null;
  };
  const io1Read = (offset) => {
    if (offset === REG.STREAM_DATA) {
      if (m.controls.length) m.streamedAfterControl = true;
      return m.streamed < prg.length ? prg[m.streamed++] : 0;
    }
    if (offset === REG.SWAP_POLL) {
      if (!m.controls.includes(CTL.RUNNING_PRG)) return SWAP.BUSY;
      if (busy-- > 0) return SWAP.BUSY;
      m.swapReady = true;
      return SWAP.READY;
    }
    return 0xff;
  };

  m.bus = {
    read(a) {
      if (a === MMU_CR) return m.cr;
      if (ioMapped(a)) return (a & 0xff00) === IO1 ? io1Read(a & 0xff) : 0xff;
      const r = rom(a);
      if (r === cart.roml) return r[a & 0x3ff];
      if (r === cart.romh) return r[a & 0xff];
      if (r) throw new Error(`the boot ROM read ${r} ROM at $${a.toString(16)}`);
      return ram[a];
    },
    write(a, v) {
      if (a === MMU_CR) {
        if (v === SYSTEM_BANK && !m.swapReady) m.systemBankBeforeReady = true;
        m.cr = v;
        return;
      }
      if (ioMapped(a)) {
        m.ioWrites.push([a, v]);
        if (a === IO1 + REG.CONTROL) m.controls.push(v);
        return;
      }
      ram[a] = v;
    },
  };
  return m;
}

function boot(prg, options) {
  const machine = c128(prg, options);
  const cpu = new Mos6502(machine.bus);
  cpu.pc = COLD_START;
  cpu.run((c) => c.pc === BASIC_WARM_START, 2_000_000);
  return machine;
}

const word = (ram, a) => ram[a] | (ram[a + 1] << 8);

test('the program lands at $1C01 and BASIC is restarted with RUN typed', () => {
  const prg = program(3000);
  const m = boot(prg);
  assert.deepEqual(m.ram.subarray(BASIC_START, BASIC_START + prg.length - 2), prg.subarray(2));
  assert.equal(word(m.ram, TEXT_TOP), BASIC_START + prg.length - 2);
  assert.equal(String.fromCharCode(...m.ram.subarray(KEYBOARD_BUFFER, KEYBOARD_BUFFER + m.ram[KEYBOARD_COUNT])), 'RUN\r');
  assert.equal(m.cr, SYSTEM_BANK);
});

test('it reads exactly the program from the stream, then vanishes the ROM and swaps the IO handler', () => {
  const prg = program(700);
  const m = boot(prg);
  assert.equal(m.streamed, prg.length);
  assert.deepEqual(m.controls, [CTL.VANISH_ROM, CTL.RUNNING_PRG]);
  assert.equal(m.streamedAfterControl, false);
});

test('it waits for the IO handler swap before restarting BASIC', () => {
  const m = boot(program(10), { swapBusyReads: 500 });
  assert.equal(m.swapReady, true);
  assert.equal(m.systemBankBeforeReady, false);
  assert.equal(m.cr, SYSTEM_BANK);
});

test('a program reaching up to $FF00 lands in RAM, not in the I/O chips', () => {
  const prg = program(0xff00 - BASIC_START);
  const m = boot(prg);
  assert.deepEqual(m.ram.subarray(BASIC_START, 0xff00), prg.subarray(2));
  assert.deepEqual(m.ioWrites.filter(([a]) => a !== IO1 + REG.CONTROL), []);
});
