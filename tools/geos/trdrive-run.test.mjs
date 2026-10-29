// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Mos6502 } from '../lib/mos6502.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const GEOS_DIR = path.join(root, 'Source/Teensy/GEOS');

const ENTRY = { ReadBlock: 7, WriteBlock: 8 };
const R1 = 0x04;
const R4 = 0x0a;
const BUFFER = 0x8000;
const CALLER = 0x0200;
const ERR_DEV_NOT_FOUND = 13;
const BESIDE_GEORAM = 0xdf00;
const BESIDE_REU = 0xde00;
const REU_REGISTERS = 0x0b;
const REU_GHOSTS = 0x1f;
const PAGE = 256;
const QUARTER = 64;

function readDriver(header, prefix) {
  const text = fs.readFileSync(path.join(GEOS_DIR, header), 'utf8');
  const array = (name) => [...text.match(new RegExp(`${name}\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16));
  const base = parseInt(text.match(/#define TR_DRIVER\w*_BASE (0x[0-9a-f]+)/)[1], 16);
  return { base, code: array(`${prefix}Code`), entries: array(`${prefix}Entries`) };
}

const DRIVERS = [
  ['GEOS 64', readDriver('TRDriver.h', 'TRDriver')],
  ['GEOS 128', readDriver('TRDriver128.h', 'TRDriver128')],
];

// The TR block device (docs/TR_Block_Device.md) answering READ and WRITE at once, its blocks keyed by ADDR1:ADDR0
function blockDevice(blocks) {
  const regs = { addr0: 0, addr1: 0, page: 0, token: 0 };
  let buffer = new Uint8Array(PAGE), status = 0, echo = 0;
  return {
    write(offset, value) {
      if (offset >= 0x40 && offset < 0x80) buffer[regs.page * QUARTER + offset - 0x40] = value;
      else if (offset === 0x09) regs.addr0 = value;
      else if (offset === 0x0a) regs.addr1 = value;
      else if (offset === 0x0b) regs.page = value & 3;
      else if (offset === 0x0c) regs.token = value;
      else if (offset === 0x08) {
        const key = (regs.addr1 << 8) | regs.addr0;
        if (value === 1) buffer = Uint8Array.from(blocks.get(key) ?? new Uint8Array(PAGE));
        if (value === 2) blocks.set(key, Uint8Array.from(buffer));
        status = 0x5a;
        echo = regs.token;
      }
    },
    read(offset) {
      if (offset >= 0x40 && offset < 0x80) return buffer[regs.page * QUARTER + offset - 0x40];
      if (offset === 0x0d) return status;
      if (offset === 0x0e) return echo;
      if (offset >= 0x10 && offset < 0x13) return 'TRB'.charCodeAt(offset - 0x10);
      return 0;
    },
  };
}

// What answers at $DF00 beside an REU: its registers, holding what a program left in them, ghosted every 32 bytes,
// the unused ones reading $FF
function reuRegisters(regs) {
  return {
    write() {},
    read: (offset) => ((offset & REU_GHOSTS) < REU_REGISTERS ? regs[offset & REU_GHOSTS] : 0xff),
  };
}

// GeoRAM's window: RAM, holding whatever the page it shows holds
function geoRamWindow(bytes) {
  return { write: (offset, value) => (bytes[offset] = value), read: (offset) => bytes[offset] };
}

function machine(driver, pages) {
  const ram = new Uint8Array(0x10000);
  const ioWrites = [];
  ram.set(driver.code, driver.base);
  const bus = {
    read: (address) => (pages[address & 0xff00] ? pages[address & 0xff00].read(address & 0xff) : ram[address]),
    write(address, value) {
      const page = pages[address & 0xff00];
      if (!page) ram[address] = value;
      else {
        ioWrites.push(address);
        page.write(address & 0xff, value);
      }
    },
  };
  return { ram, ioWrites, pages, driver, bus };
}

function call(m, entry, track, sector) {
  const cpu = new Mos6502(m.bus);
  const at = m.driver.base + m.driver.entries[entry];
  m.ram.set([0x20, at & 0xff, at >> 8], CALLER);
  m.ram.set([track, sector], R1);
  m.ram.set([BUFFER & 0xff, BUFFER >> 8], R4);
  cpu.pc = CALLER;
  cpu.run((c) => c.pc === CALLER + 3);
  return cpu.x;
}

const sectorOf = (seed) => Uint8Array.from({ length: PAGE }, (_, i) => (i * 13 + seed) & 0xff);
const key = (track, sector) => (sector << 8) | track;

for (const [name, driver] of DRIVERS) {
  test(`${name}: the driver reads through the registers beside GeoRAM, not the GeoRAM window that shows "TRB" too`, () => {
    const window = new Uint8Array(PAGE);
    window.set([...'TRB'].map((c) => c.charCodeAt(0)), 0x10);
    const m = machine(driver, {});
    m.pages[BESIDE_GEORAM] = blockDevice(new Map([[key(1, 2), sectorOf(1)]]));
    m.pages[BESIDE_REU] = geoRamWindow(window);
    assert.equal(call(m, ENTRY.ReadBlock, 1, 2), 0);
    assert.deepEqual(m.ram.subarray(BUFFER, BUFFER + PAGE), sectorOf(1));
    assert.deepEqual(m.ioWrites.filter((a) => a < BESIDE_GEORAM), [], 'nothing written to the GeoRAM window');
  });

  test(`${name}: the driver reads and writes through the registers beside an REU, whatever the REU's own registers hold`, () => {
    const blocks = new Map([[key(3, 4), sectorOf(2)]]);
    const m = machine(driver, {});
    m.pages[BESIDE_REU] = blockDevice(blocks);
    m.pages[BESIDE_GEORAM] = reuRegisters([0x10, 0x00, 0x54, 0x52, 0x42, 0x54, 0x52, 0x42, 0, 0, 0]);
    assert.equal(call(m, ENTRY.ReadBlock, 3, 4), 0);
    assert.deepEqual(m.ram.subarray(BUFFER, BUFFER + PAGE), sectorOf(2));

    m.ram.set(sectorOf(3), BUFFER);
    assert.equal(call(m, ENTRY.WriteBlock, 5, 6), 0);
    assert.deepEqual(blocks.get(key(5, 6)), sectorOf(3));
    assert.deepEqual(m.ioWrites.filter((a) => a >= BESIDE_GEORAM), [], 'nothing written to the REU');
  });

  test(`${name}: the driver finds the registers again when they move, and reports a missing device`, () => {
    const m = machine(driver, {});
    m.pages[BESIDE_GEORAM] = blockDevice(new Map([[key(1, 0), sectorOf(4)]]));
    assert.equal(call(m, ENTRY.ReadBlock, 1, 0), 0);

    m.pages[BESIDE_GEORAM] = reuRegisters(new Array(REU_REGISTERS).fill(0));
    m.pages[BESIDE_REU] = blockDevice(new Map([[key(1, 0), sectorOf(5)]]));
    assert.equal(call(m, ENTRY.ReadBlock, 1, 0), 0);
    assert.deepEqual(m.ram.subarray(BUFFER, BUFFER + PAGE), sectorOf(5));

    m.pages[BESIDE_REU] = reuRegisters(new Array(REU_REGISTERS).fill(0));
    assert.equal(call(m, ENTRY.ReadBlock, 1, 0), ERR_DEV_NOT_FOUND);
  });
}
