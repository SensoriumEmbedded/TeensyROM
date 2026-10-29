// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Mos6502 } from '../lib/mos6502.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const committedHeader = path.join(root, 'Source/Teensy/GEOS/Geos128Boot.h');

const MMU_CR = 0xff00;
const MMU_RAM_SHARE = 0xd506;
const KERNAL_RESET_SHARE = 0x04;
const FUNCTION_ROM_AT_8000 = 0x08;
const COLD_START = 0x8000;
const BLOCK = 0xdf00;
const STATUS = { BUSY: 0x80, DONE: 0x5a, PAST_END: 0xe2 };
const CMD_BOOT = 5;
const PAGE = 256;
const QUARTER = 64;

function readHeader() {
  const text = fs.readFileSync(committedHeader, 'utf8');
  const define = (name) => parseInt(text.match(new RegExp(`#define ${name}\\s+(0x[0-9a-f]+)`))[1], 16);
  const bytes = (name) => Uint8Array.from([...text.match(new RegExp(`${name}\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16)));
  return {
    chipSize: define('GEOS128_BOOT_CHIP_SIZE'),
    params: define('GEOS128_BOOT_PARAMS'),
    vectors: define('GEOS128_BOOT_VECTORS'),
    roml: bytes('Geos128BootRoml'),
    romVectors: bytes('Geos128BootVectors'),
  };
}

// The cartridge as GeosBuildC128Boot fills it: ROML with the load address, page count and entry patched in, ROMH
// holding only the vectors.
function cartridge({ load, pages, entry }) {
  const h = readHeader();
  const roml = new Uint8Array(h.chipSize);
  const romh = new Uint8Array(h.chipSize);
  roml.set(h.roml);
  romh.set(h.romVectors, h.vectors);
  for (const [i, word] of [load, pages, entry].entries()) {
    roml[h.params + 2 * i] = word & 0xff;
    roml[h.params + 2 * i + 1] = word >> 8;
  }
  return { roml, romh };
}

// The block device's registers at $DF00 as docs/TR_Block_Device.md describes them, serving BOOT from image.
// busyReads: STATUS reads $80 this many times after each CMD. staleEcho: for this many ECHO reads after STATUS turns
// done, the device still shows the previous command's result, its token and its page. glitchEvery: every nth read
// comes back with bits ORed in, as the Kawari does. echoGlitch: ORed into the first stale ECHO read.
function blockDevice(image, { busyReads = 0, staleEcho = 0, glitchEvery = 0, echoGlitch = 0, neverDone = false } = {}) {
  let buffer = new Uint8Array(PAGE), previousBuffer = buffer;
  const regs = { addr0: 0, addr1: 0, page: 0, token: 0 };
  let status = 0, echo = 0, previousToken = 0, busy = 0, stale = 0, reads = 0;
  const device = {
    commands: 0,
    write(offset, value) {
      switch (offset) {
        case 0x08: return device.command(value);
        case 0x09: regs.addr0 = value; return;
        case 0x0a: regs.addr1 = value; return;
        case 0x0b: regs.page = value & 3; return;
        case 0x0c: regs.token = value; return;
      }
    },
    command(cmd) {
      device.commands++;
      previousToken = echo;
      previousBuffer = buffer;
      buffer = new Uint8Array(PAGE);
      echo = regs.token;
      busy = neverDone ? Infinity : busyReads;
      stale = staleEcho;
      const start = ((regs.addr1 << 8) | regs.addr0) * PAGE;
      if (cmd !== CMD_BOOT) throw new Error(`the loader sent command ${cmd}`);
      if (start >= image.length) {
        status = STATUS.PAST_END;
        return;
      }
      buffer.set(image.subarray(start, start + PAGE));
      status = STATUS.DONE;
    },
    read(offset) {
      reads++;
      const glitch = glitchEvery && reads % glitchEvery === 0 ? 0x41 : 0;
      return device.answer(offset) | glitch;
    },
    answer(offset) {
      if (offset === 0x0d) {
        if (busy > 0) {
          busy--;
          return STATUS.BUSY;
        }
        return status;
      }
      if (offset === 0x0e) {
        if (busy === 0 && stale > 0) {
          const glitch = stale === staleEcho ? echoGlitch : 0;
          stale--;
          return previousToken | glitch;
        }
        return echo;
      }
      const shown = stale > 0 ? previousBuffer : buffer;
      if (offset >= 0x40 && offset < 0x80) return shown[regs.page * QUARTER + offset - 0x40];
      return 0xff;
    },
  };
  return device;
}

// A C128 as the boot ROM sees it: two 64K RAM banks behind the MMU's configuration register, the shared RAM at the
// bottom, I/O at $D000 (the block device at $DF00) and the cartridge's function ROM at $8000 and $C000.
function c128(cart, device) {
  const ram = [new Uint8Array(0x10000), new Uint8Array(0x10000)];
  const io = new Map([[MMU_RAM_SHARE, KERNAL_RESET_SHARE]]);
  const ioWrites = [];
  const ramWrites = [];
  const machine = { ram, io, ioWrites, ramWrites, cr: FUNCTION_ROM_AT_8000 };

  const bank = (address) => {
    const share = io.get(MMU_RAM_SHARE);
    const sharedTop = [0x0400, 0x1000, 0x2000, 0x4000][share & 3];
    if (share & 0x04 && address < sharedTop) return 0;
    return (machine.cr >> 6) & 1;
  };
  const ioMapped = (address) => address >= 0xd000 && address < 0xe000 && !(machine.cr & 0x01);
  const romAt = (address) => {
    if (address >= 0x8000 && address < 0xc000) return ((machine.cr >> 2) & 3) === 2 ? cart.roml : null;
    if (address >= 0xc000) return ((machine.cr >> 4) & 3) === 2 ? cart.romh : null;
    return null;
  };
  const systemRom = (address) =>
    (address >= 0x4000 && address < 0x8000 && !(machine.cr & 0x02)) ||
    (address >= 0x8000 && address < 0xc000 && ((machine.cr >> 2) & 3) < 2) ||
    (address >= 0xc000 && ((machine.cr >> 4) & 3) < 2 && !ioMapped(address));

  machine.bus = {
    read(address) {
      if (address === MMU_CR) return machine.cr;
      if (ioMapped(address)) {
        if (address >= BLOCK) return device.read(address - BLOCK);
        return io.get(address) ?? 0xff;
      }
      const rom = romAt(address);
      if (rom) return rom[address & 0x1fff];
      if (systemRom(address)) throw new Error(`the boot ROM read the C128's own ROM at $${address.toString(16)}`);
      return ram[bank(address)][address];
    },
    write(address, value) {
      if (address === MMU_CR) {
        machine.cr = value;
        return;
      }
      if (ioMapped(address)) {
        ioWrites.push([address, value]);
        if (address >= BLOCK) device.write(address - BLOCK, value);
        else io.set(address, value);
        return;
      }
      const b = bank(address);
      ramWrites.push([b, address]);
      ram[b][address] = value;
    },
  };
  return machine;
}

function bootImage(pages, last = PAGE) {
  const image = new Uint8Array((pages - 1) * PAGE + last);
  for (let i = 0; i < image.length; i++) image[i] = (i * 7 + (i >> 8)) & 0xff;
  return image;
}

// The kernal's cold start calls the function ROM with IRQs on and the ROM mapped at $8000.
function boot({ load = 0x2b19, pages = 3, entry = 0x2f3c, image = bootImage(pages), device: options, limit } = {}) {
  const device = blockDevice(image, options);
  const machine = c128(cartridge({ load, pages, entry }), device);
  const cpu = new Mos6502(machine.bus);
  cpu.pc = COLD_START;
  cpu.p &= ~0x04;
  return { cpu, machine, device, image, load, entry, limit };
}

const run = (b, done) => b.cpu.run(() => done(b), b.limit ?? 5_000_000);
const entered = (b) => b.cpu.pc === b.entry;
const borderFlashes = (b) => b.machine.ioWrites.filter(([a]) => a === 0xd020).length >= 3;

function assertLoaded(b) {
  const bank1 = b.machine.ram[1];
  const expected = new Uint8Array(Math.ceil(b.image.length / PAGE) * PAGE);
  expected.set(b.image);
  assert.deepEqual(bank1.subarray(b.load, b.load + expected.length), expected);
}

test('the boot ROM pulls every page into bank 1 and enters GEOS with I/O in, the screen on and device 8 current', () => {
  const b = boot({ pages: 150, image: bootImage(150, 0x19) });
  run(b, entered);
  assertLoaded(b);
  assert.equal(b.machine.cr, 0x7e, 'bank 1 with I/O');
  assert.equal(b.machine.io.get(0xd011), 0x1b);
  assert.equal(b.machine.io.get(0xd030), 0, '1 MHz');
  assert.equal(b.machine.ram[0][0xba], 8, 'curDevice, in the shared zero page');
  assert.equal(b.device.commands, 150);
});

test('the boot ROM writes no bank 0 RAM outside the shared bottom 8K, and nothing in bank 1 but the image', () => {
  const b = boot();
  run(b, entered);
  const pagesEnd = b.load + 3 * PAGE;
  const stray = b.machine.ramWrites.filter(([bank, a]) => (bank === 0 ? a >= 0x2000 : a < b.load || a >= pagesEnd));
  assert.deepEqual(stray, []);
});

test('the boot ROM waits out a busy device and a result still naming the previous command', () => {
  const b = boot({ device: { busyReads: 40, staleEcho: 3 } });
  run(b, entered);
  assertLoaded(b);
});

// Tokens run 1, 2, 3, so for the first and third pages the previous token with bit 0 ORed in reads as this one.
test('the boot ROM reads ECHO until two reads agree, so a glitch cannot pass off the previous page as this one', () => {
  const b = boot({ device: { staleEcho: 2, echoGlitch: 0x01 } });
  run(b, entered);
  assertLoaded(b);
});

test('the boot ROM reads each register until two reads agree, so a glitched read does not reach memory', () => {
  for (const glitchEvery of [5, 7, 11]) {
    const b = boot({ device: { glitchEvery } });
    run(b, entered);
    assertLoaded(b);
  }
});

test('a page the device refuses stops the boot with the screen on and the border flashing', () => {
  const b = boot({ pages: 4, image: bootImage(3) });
  run(b, borderFlashes);
  assert.equal(b.machine.io.get(0xd011), 0x1b);
  assert.notEqual(b.cpu.pc, b.entry);
  assert.equal(b.device.commands, 4);
});

test('a device that never finishes stops the boot instead of hanging it', () => {
  const b = boot({ device: { neverDone: true }, limit: 20_000_000 });
  run(b, borderFlashes);
  assert.equal(b.device.commands, 1);
});
