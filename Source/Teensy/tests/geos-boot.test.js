// SPDX-License-Identifier: MIT
// The firmware's GEOS 64 boot PRG and CONFIGURE handling, run on synthetic D81 boot disks.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/geos_boot_harness.c');
const DRIVER_HEADER = path.join(FIRMWARE_ROOT, 'GEOS/TRDriver.h');
const DRIVER128_HEADER = path.join(FIRMWARE_ROOT, 'GEOS/TRDriver128.h');
const BOOT_ROM_HEADER = path.join(FIRMWARE_ROOT, 'GEOS/Geos128Boot.h');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['cc', 'c++'].find((tool) => !commandExists(tool));
const skip = missingTool && !inCi && `${missingTool} is not on PATH`;

const SECTOR = 256;
const SECTORS_PER_TRACK = 40;
const TRACKS = 80;
const DIR_TRACK = 40;
const FIRST_DIR_SECTOR = 3;
const FIRST_FILE_TRACK = 1;
const DIR_SLOT = 32;
const SLOT_TYPE = 2;
const SLOT_FIRST = 3;
const SLOT_NAME = 5;
const SLOT_GEOS_TYPE = 24;
const PAD = 0xa0;
const USR_CLOSED = 0x83;
const AUTO_EXEC = 14;
const SYSTEM_FILE = 4;

const DRIVER_AREA = 0x9000;
const DRIVER_SIZE = 0xd80;
const TURBO_START = 0x9100;
const READ_BLOCK = 0x9400;
const TYPE_BYTE = 0x4e;
const JUMP_SLOTS = 0x30;
const JUMP_COUNT = 10;
const LOAD_ADDRESS = 0x2c98;
const DRIVER_AT_IN_BODY = 0x4400;
const BODY_SIZE = 0x6000;
const BASIC_START = 0x0801;

const arrayIn = (file) => (name) => [...fs.readFileSync(file, 'utf8').match(new RegExp(`${name}\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16));
const arrayOf = arrayIn(DRIVER_HEADER);
const CODE = arrayOf('TRDriverCode');
const RELOCATIONS = arrayOf('TRDriverRelocations');
const ENTRIES = arrayOf('TRDriverEntries');
const ENTRIES_128 = arrayIn(DRIVER128_HEADER)('TRDriver128Entries');
const BOOT_ROML = arrayIn(BOOT_ROM_HEADER)('Geos128BootRoml');
const BOOT_VECTORS = arrayIn(BOOT_ROM_HEADER)('Geos128BootVectors');
const BOOT_PAGE_AT = arrayIn(BOOT_ROM_HEADER)('Geos128BootPageAt');
const REGS_BESIDE_GEORAM = 0xdf;
const REGS_BESIDE_REU = 0xde;
const BOOT_CHIP = 0x2000;
const BOOT_PARAMS = 0x21;

const offsetOf = (track, sector) => ((track - 1) * SECTORS_PER_TRACK + sector) * SECTOR;

function stockDriver() {
  const driver = Buffer.alloc(DRIVER_SIZE, 0xea);
  const vectors = [TURBO_START, 0x9140, 0x9180, 0x91c0, 0x9200, 0x9240, 0x9280, READ_BLOCK, 0x9500, 0x9540];
  for (let i = 0; i < 24; i++) driver.writeUInt16LE(vectors[i] ?? 0x9600 + i, 2 * i);
  for (let i = 0; i < JUMP_COUNT; i++) {
    driver[JUMP_SLOTS + 3 * i] = 0x4c;
    driver.writeUInt16LE(0x9700 + i, JUMP_SLOTS + 3 * i + 1);
  }
  driver[TYPE_BYTE] = 3;
  return driver;
}

// GEOS 128's 1581 driver keeps NewDisk after ReadBlock, outside the run the TR driver takes
const NEW_DISK_128 = 0x9480;
function stockDriver128() {
  const driver = stockDriver();
  driver.writeUInt16LE(NEW_DISK_128, 2 * 6);
  return driver;
}

const START_AT_IN_BODY = 0x1000;
const START_DATE = [0xa9, 0x17, 0x8d, 0x18, 0x85, 0xa9, 0x03, 0x8d, 0x17, 0x85, 0xa9, 0x5c, 0x8d, 0x16, 0x85];
const LOOP_END = [0xca, 0xd0, 0xe4];
const START_CLOCK = [0xad, 0x0f, 0xdc, 0x29, 0x7f, 0x8d, 0x0f, 0xdc, 0xa9, 0x81, 0x8d, 0x0b, 0xdc, 0xa9, 0x00, 0x8d, 0x0a, 0xdc, 0x8d, 0x09, 0xdc, 0x8d, 0x08, 0xdc];
const START_UP = [...START_DATE, ...LOOP_END, ...START_CLOCK];
const CLOCK_AT = START_DATE.length + LOOP_END.length;

const CRB = 0xdc0f;
const ALARM_SELECT = 0x80;
const TOD_TENTHS = 0xdc08;
const TOD_HOURS = 0xdc0b;
const PM = 0x80;

// Runs the start-up clock code the way a 6526 takes it: X is zero from the loop before it unless given, writes go
// to the alarm while CRB selects it, and writing hour 12 flips AM/PM. Returns the TOD registers it sets, tenths
// first; other stores land in memory.
function runClock(code, { x = 0, memory = {} } = {}) {
  let a = 0;
  let crb = ALARM_SELECT;
  const tod = [];
  const write = (address, value) => {
    if (address === CRB) crb = value;
    else if (address < TOD_TENTHS || address > TOD_HOURS) memory[address] = value;
    else if (crb & ALARM_SELECT) assert.fail(`$${address.toString(16)} written while CRB selects the alarm`);
    else tod[address - TOD_TENTHS] = address === TOD_HOURS && (value & 0x1f) === 0x12 ? value ^ PM : value;
  };
  for (let pc = 0; pc < code.length; ) {
    const op = code[pc];
    const word = code[pc + 1] | (code[pc + 2] << 8);
    if (op === 0xa9) (a = code[pc + 1]), (pc += 2);
    else if (op === 0xa2) (x = code[pc + 1]), (pc += 2);
    else if (op === 0x29) (a &= code[pc + 1]), (pc += 2);
    else if (op === 0xad && word === CRB) (a = crb), (pc += 3);
    else if (op === 0x8d) write(word, a), (pc += 3);
    else if (op === 0x8e) write(word, x), (pc += 3);
    else assert.fail(`unexpected opcode $${op.toString(16)}`);
  }
  return tod;
}

const GEOS64 = { load: LOAD_ADDRESS, driverAt: DRIVER_AT_IN_BODY, startUpAt: START_AT_IN_BODY };

function bootFile(driver = stockDriver(), startUp = [], layout = GEOS64) {
  const size = layout.size ?? BODY_SIZE;
  const body = Buffer.alloc(size);
  for (let i = 0; i < size; i++) body[i] = (i * 31 + 7) & 0xff;
  driver.copy(body, layout.driverAt);
  Buffer.from(startUp).copy(body, layout.startUpAt);
  if (layout.resetKey) Buffer.from(layout.resetKey.bytes).copy(body, layout.resetKey.at);
  const file = Buffer.alloc(2 + size);
  file.writeUInt16LE(layout.load, 0);
  body.copy(file, 2);
  return { file, body };
}

class Disk {
  constructor() {
    this.image = Buffer.alloc(TRACKS * SECTORS_PER_TRACK * SECTOR);
    this.nextSector = 0;
    this.slots = [];
    const header = offsetOf(DIR_TRACK, 0);
    this.image[header] = DIR_TRACK;
    this.image[header + 1] = FIRST_DIR_SECTOR;
    this.image[offsetOf(DIR_TRACK, FIRST_DIR_SECTOR) + 1] = 0xff;
  }

  addFile(name, data, geosType = 0) {
    const sectors = [];
    for (let at = 0; at < data.length || !sectors.length; at += SECTOR - 2) sectors.push(this.nextSector++);
    sectors.forEach((n, i) => {
      const track = FIRST_FILE_TRACK + Math.floor(n / SECTORS_PER_TRACK);
      const o = offsetOf(track, n % SECTORS_PER_TRACK);
      const chunk = data.subarray(i * (SECTOR - 2), (i + 1) * (SECTOR - 2));
      const last = i === sectors.length - 1;
      const next = sectors[i + 1];
      this.image[o] = last ? 0 : FIRST_FILE_TRACK + Math.floor(next / SECTORS_PER_TRACK);
      this.image[o + 1] = last ? chunk.length + 1 : next % SECTORS_PER_TRACK;
      chunk.copy(this.image, o + 2);
    });
    const slot = offsetOf(DIR_TRACK, FIRST_DIR_SECTOR) + this.slots.length * DIR_SLOT;
    this.image[slot + SLOT_TYPE] = USR_CLOSED;
    this.image[slot + SLOT_FIRST] = FIRST_FILE_TRACK + Math.floor(sectors[0] / SECTORS_PER_TRACK);
    this.image[slot + SLOT_FIRST + 1] = sectors[0] % SECTORS_PER_TRACK;
    this.image.fill(PAD, slot + SLOT_NAME, slot + SLOT_NAME + 16);
    this.image.write(name, slot + SLOT_NAME, 'latin1');
    this.image[slot + SLOT_GEOS_TYPE] = geosType;
    this.slots.push(slot);
    return slot;
  }
}

function run(args, env = {}) {
  const r = spawnSync(args[0], args.slice(1), { encoding: 'utf8', env: { ...process.env, ...env } });
  return { status: r.status, out: r.stdout, err: r.stderr };
}

function withHarness(body) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'geos-boot-'));
  try {
    const built = run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-o', path.join(dir, 'harness'), HARNESS_SOURCE]);
    assert.equal(built.status, 0, built.err);
    const asCpp = run(['c++', '-x', 'c++', '-std=gnu++17', '-Wall', '-Werror', '-fsyntax-only', HARNESS_SOURCE]);
    assert.equal(asCpp.status, 0, asCpp.err);
    const launch = (disk, time = [], version = '64', env = {}) => {
      fs.writeFileSync(path.join(dir, 'in.d81'), disk.image);
      const r = run([path.join(dir, 'harness'), version, path.join(dir, 'in.d81'), path.join(dir, 'out.prg'), path.join(dir, 'out.d81'), ...time.map(String)], env);
      const out = r.status === 0 ? fs.readFileSync(path.join(dir, 'out.prg')) : null;
      const image = r.status === 0 ? fs.readFileSync(path.join(dir, 'out.d81')) : null;
      const boot = r.out.match(/ boot=(\d+),(\d+)/);
      if (!boot) return { ...r, prg: out, image };
      const [at, size] = [Number(boot[1]), Number(boot[2])];
      assert.equal(at + size, out.length, 'the boot image ends the output');
      // the boot file as the disk holds it, load address first, and the ROM in front of it
      return { ...r, prg: out.subarray(at - 2), rom: out.subarray(0, at - 2), boot: out.subarray(at), image };
    };
    body(launch);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

function standardDisk(driver, startUp = []) {
  const disk = new Disk();
  disk.addFile('GEOS', Buffer.from([1, 8, 0]));
  disk.addFile('GEOBOOT', bootFile(driver, startUp).file);
  return disk;
}

// GEOBOOT128 as the stock GEOS 128 2.0 disk has it: loaded at $2B19, the 1581 driver staged at $7000 with the
// reset key just below it, and the clock and date set in one run at $2C24
const GEOS128_LOAD = 0x2b19;
const RESET_KEY = [0x43, 0x42, 0x4d, 0xe4, 0x03];
const START_UP_128 = [
  0xa9, 0x00, 0x8d, 0xc5, 0x88, ...START_CLOCK,
  0xa9, 0x5c, 0x8d, 0x16, 0x85, 0xa9, 0x03, 0x8d, 0x17, 0x85, 0xa9, 0x17, 0x8d, 0x18, 0x85,
];
const geos128Layout = (load = GEOS128_LOAD, size = BODY_SIZE) => ({
  load,
  size,
  driverAt: 0x7000 - load,
  startUpAt: 0x2c24 - load,
  resetKey: { at: 0x6ff5 - load, bytes: RESET_KEY },
});
const GEOS128 = geos128Layout();
const DATE = { 0x8516: 26, 0x8517: 10, 0x8518: 21 };

function geos128Disk(startUp = START_UP_128, layout = GEOS128) {
  const disk = new Disk();
  disk.addFile('GEOS128', Buffer.from([1, 8, 0]));
  disk.addFile('GEOBOOT128', bootFile(stockDriver128(), startUp, layout).file);
  return disk;
}

test('the boot PRG carries the boot file, with the TR disk driver in place of the stock transport', { skip }, () => {
  withHarness((launch) => {
    const r = launch(standardDisk());
    assert.equal(r.status, 0, r.err);
    const { body } = bootFile();
    const place = 2 + LOAD_ADDRESS - BASIC_START;

    assert.equal(r.prg.readUInt16LE(0), BASIC_START);
    assert.equal(r.prg.length, place + BODY_SIZE);
    assert.equal(r.prg.subarray(2, 14).toString('hex'), '0b080a009e323036310000' + '00');
    assert.equal(r.prg.subarray(31, place).every((b) => b === 0), true, 'zero padding up to the load address');
    assert.deepEqual([...r.prg.subarray(2 + 12 + 14, 2 + 12 + 17)], [0x4c, 0x3c, 0x2f], 'jumps to the boot entry');

    const patched = r.prg.subarray(place + DRIVER_AT_IN_BODY, place + DRIVER_AT_IN_BODY + DRIVER_SIZE);
    const expected = Buffer.from(CODE);
    for (const at of RELOCATIONS) expected.writeUInt16LE(expected.readUInt16LE(at) + TURBO_START - DRIVER_AREA, at);
    const start = TURBO_START - DRIVER_AREA;
    assert.equal(patched.subarray(start, start + CODE.length).equals(expected), true, 'relocated driver at InitForIO');
    assert.equal(patched.subarray(start + CODE.length).equals(stockDriver().subarray(start + CODE.length)), true, 'the rest of the stock driver stays');
    for (let i = 0; i < 10; i++) assert.equal(patched.readUInt16LE(2 * i), TURBO_START + ENTRIES[i], `vector ${i}`);
    for (const [slot, entry] of [[6, 10], [7, 11], [9, 12]]) assert.equal(patched.readUInt16LE(JUMP_SLOTS + 3 * slot + 1), TURBO_START + ENTRIES[entry], `jump slot ${slot}`);
    assert.equal(patched[TYPE_BYTE], 3);
    assert.equal(r.prg.subarray(place, place + DRIVER_AT_IN_BODY).equals(body.subarray(0, DRIVER_AT_IN_BODY)), true, 'bytes before the driver');
    assert.equal(r.prg.subarray(place + DRIVER_AT_IN_BODY + DRIVER_SIZE).equals(body.subarray(DRIVER_AT_IN_BODY + DRIVER_SIZE)), true, 'bytes after the driver');
  });
});

test('the boot PRG starts GEOS with the given date and time', { skip }, () => {
  withHarness((launch) => {
    const r = launch(standardDisk(undefined, START_UP));
    const place = 2 + LOAD_ADDRESS - BASIC_START + START_AT_IN_BODY;
    const at = r.prg.subarray(place, place + START_UP.length);

    assert.match(r.out, /clock=1/);
    assert.deepEqual([at[1], at[6], at[11]], [21, 10, 26], 'day, month, year');
    assert.deepEqual([...at.subarray(START_DATE.length, CLOCK_AT)], LOOP_END);
    assert.deepEqual(runClock(at.subarray(CLOCK_AT)), [0x00, 0x00, 0x07, PM | 0x03], '3:07 PM');
  });
});

test('the TOD holds noon and midnight as 12 PM and 12 AM', { skip }, () => {
  withHarness((launch) => {
    const place = 2 + LOAD_ADDRESS - BASIC_START + START_AT_IN_BODY + CLOCK_AT;
    for (const [hour, held] of [[12, PM | 0x12], [0, 0x12], [11, 0x11], [23, PM | 0x11]]) {
      const r = launch(standardDisk(undefined, START_UP), [hour, 30]);
      assert.equal(r.status, 0, r.err);
      assert.deepEqual(runClock(r.prg.subarray(place, place + START_CLOCK.length)), [0x00, 0x00, 0x30, held], `${hour}:30`);
    }
  });
});

test('a boot file without the known start-up code keeps the date and time it has', { skip }, () => {
  withHarness((launch) => {
    const place = 2 + LOAD_ADDRESS - BASIC_START;
    for (const startUp of [[], [...START_DATE, ...START_CLOCK]]) {
      const r = launch(standardDisk(undefined, startUp));
      assert.match(r.out, /clock=0/);
      assert.equal(r.prg.subarray(place, place + DRIVER_AT_IN_BODY).equals(bootFile(undefined, startUp).body.subarray(0, DRIVER_AT_IN_BODY)), true);
    }
  });
});

test('a boot file whose driver is not recognized is refused', { skip }, () => {
  withHarness((launch) => {
    const odd = stockDriver();
    odd[TYPE_BYTE] = 9;
    const r = launch(standardDisk(odd));
    assert.equal(r.status, 1);
    assert.match(r.err, /disk driver is not one the TR can replace/);

    const crowded = stockDriver();
    crowded.writeUInt16LE(0x9110, 2 * 7);
    assert.equal(launch(standardDisk(crowded)).status, 1, 'no room for the TR driver before ReadBlock');

    const shared = stockDriver();
    shared.writeUInt16LE(TURBO_START + 0x20, 2 * 12);
    assert.equal(launch(standardDisk(shared)).status, 1, 'a vector the TR driver does not replace points into the replaced run');

    const jumped = stockDriver();
    jumped.writeUInt16LE(TURBO_START + 0x20, JUMP_SLOTS + 3 * 2 + 1);
    assert.equal(launch(standardDisk(jumped)).status, 1, 'a jump slot the TR driver does not replace points into the replaced run');
  });
});

test('a disk with no boot file is refused', { skip }, () => {
  withHarness((launch) => {
    const disk = new Disk();
    disk.addFile('GEOS', Buffer.from([1, 8, 0]));
    const r = launch(disk);
    assert.equal(r.status, 1);
    assert.match(r.err, /boot file is not on the disk/);
  });
});

test('CONFIGURE files stop being auto-executed, and nothing else changes', { skip }, () => {
  withHarness((launch) => {
    const disk = standardDisk();
    const configure = disk.addFile('CONFIGURE', Buffer.from([1, 2, 3]), AUTO_EXEC);
    const second = disk.addFile('CONFIG 2.0r', Buffer.from([1, 2, 3]), AUTO_EXEC);
    const geos128 = ['128 CONFIGURE', '128 CONFIG 2.0r', '64 CONFIG 2.0r'].map((name) => disk.addFile(name, Buffer.from([1, 2, 3]), AUTO_EXEC));
    const other = disk.addFile('pad color mgr', Buffer.from([1, 2, 3]), AUTO_EXEC);
    const plain = disk.addFile('CONFIGURE.DOC', Buffer.from([1, 2, 3]), 0);

    const first = launch(disk);
    assert.equal(first.status, 0, first.err);
    assert.match(first.out, /changed=1/);
    assert.equal(first.image[configure + SLOT_GEOS_TYPE], SYSTEM_FILE);
    assert.equal(first.image[second + SLOT_GEOS_TYPE], SYSTEM_FILE);
    for (const slot of geos128) assert.equal(first.image[slot + SLOT_GEOS_TYPE], SYSTEM_FILE, 'GEOS 128 names CONFIG after a prefix');
    assert.equal(first.image[other + SLOT_GEOS_TYPE], AUTO_EXEC, 'other auto-exec files stay');
    assert.equal(first.image[plain + SLOT_GEOS_TYPE], 0, 'a CONFIG file that is not auto-exec stays');

    const again = launch({ image: first.image });
    assert.match(again.out, /changed=0/);
    assert.equal(again.image.equals(first.image), true);
  });
});

test('the GEOS 128 boot image is the boot file with the TR disk driver in place and the reset key broken', { skip }, () => {
  withHarness((launch) => {
    const r = launch(geos128Disk(), [], '128');
    assert.equal(r.status, 0, r.err);
    const { file } = bootFile(stockDriver128(), START_UP_128, GEOS128);
    const driverAt = 2 + GEOS128.driverAt;
    const keyAt = 2 + GEOS128.resetKey.at;
    const startAt = 2 + GEOS128.startUpAt;

    assert.equal(r.prg.length, file.length);
    assert.equal(r.prg.readUInt16LE(0), GEOS128_LOAD);
    const patched = r.prg.subarray(driverAt, driverAt + DRIVER_SIZE);
    for (let i = 0; i < 10; i++) assert.equal(patched.readUInt16LE(2 * i), TURBO_START + ENTRIES_128[i], `vector ${i}`);
    const oldNewDisk = NEW_DISK_128 - DRIVER_AREA;
    assert.equal(patched.subarray(oldNewDisk, READ_BLOCK - DRIVER_AREA + 0x100).equals(stockDriver128().subarray(oldNewDisk, READ_BLOCK - DRIVER_AREA + 0x100)), true, 'the stock NewDisk stays');
    assert.deepEqual([...r.prg.subarray(keyAt, keyAt + RESET_KEY.length)], [0, ...RESET_KEY.slice(1)], 'the C of CBM is gone');

    const unchanged = (from, to) => assert.equal(r.prg.subarray(from, to).equals(file.subarray(from, to)), true, `bytes ${from}-${to}`);
    unchanged(0, startAt);
    unchanged(startAt + START_UP_128.length, keyAt);
    unchanged(keyAt + 1, driverAt);
    unchanged(driverAt + DRIVER_SIZE, file.length);
  });
});

test('GEOS 128 starts with the given date and time, whatever X holds', { skip }, () => {
  withHarness((launch) => {
    const startAt = 2 + GEOS128.startUpAt;
    for (const [hour, minute, held] of [[0, 5, 0x12], [12, 30, PM | 0x12], [23, 59, PM | 0x11], [15, 7, PM | 0x03]]) {
      const r = launch(geos128Disk(), [hour, minute], '128');
      assert.match(r.out, /clock=1/);
      const memory = {};
      const tod = runClock(r.prg.subarray(startAt, startAt + START_UP_128.length), { x: 0x5a, memory });
      assert.deepEqual(tod, [0x00, 0x00, parseInt(String(minute), 16), held], `${hour}:${minute}`);
      assert.deepEqual(memory, { 0x88c5: 0, ...DATE }, 'the store before the run, then year, month, day');
    }
  });
});

test('a GEOS 128 boot file without its start-up code keeps the date and time it has', { skip }, () => {
  withHarness((launch) => {
    const startAt = 2 + GEOS128.startUpAt;
    for (const startUp of [START_UP_128.slice(5), START_UP]) {
      const r = launch(geos128Disk(startUp), [], '128');
      assert.equal(r.status, 0, r.err);
      assert.match(r.out, /clock=0/, 'GEOS 64\'s runs are not GEOS 128\'s');
      assert.deepEqual([...r.prg.subarray(startAt, startAt + startUp.length)], startUp);
    }
  });
});

test('a GEOS 128 boot file whose reset key is not where GEOS 128 keeps it is refused', { skip }, () => {
  withHarness((launch) => {
    for (const bytes of [[0x43, 0x42, 0x4d, 0xe5, 0x03], [0, 0, 0, 0, 0]]) {
      const r = launch(geos128Disk(START_UP_128, { ...GEOS128, resetKey: { at: GEOS128.resetKey.at, bytes } }), [], '128');
      assert.equal(r.status, 1);
      assert.match(r.err, /reset key is not where this version keeps it/);
    }
    const early = launch(geos128Disk(START_UP_128, { ...GEOS128, load: 0x7000 }), [], '128');
    assert.equal(early.status, 1, 'a boot file loaded above the key');
    assert.match(early.err, /reset key/);
    const short = launch(geos128Disk(START_UP_128, { ...GEOS128, load: 0x0f00 }), [], '128');
    assert.equal(short.status, 1, 'a boot file that ends before the key');
    assert.match(short.err, /reset key/);
  });
});

test('the GEOS 128 boot ROM is told where the boot image goes, how many pages it has, and where to enter it', { skip }, () => {
  withHarness((launch) => {
    for (const size of [BODY_SIZE, BODY_SIZE + 1]) {
      const r = launch(geos128Disk(START_UP_128, geos128Layout(GEOS128_LOAD, size)), [], '128');
      assert.equal(r.status, 0, r.err);
      assert.equal(r.rom.length, 2 * BOOT_CHIP);
      assert.equal(r.boot.length, size, 'BOOT serves the body, without its load address');
      const roml = r.rom.subarray(0, BOOT_CHIP), romh = r.rom.subarray(BOOT_CHIP);
      assert.deepEqual([...roml.subarray(0, BOOT_PARAMS)], BOOT_ROML.slice(0, BOOT_PARAMS));
      assert.equal(roml.readUInt16LE(BOOT_PARAMS), GEOS128_LOAD);
      assert.equal(roml.readUInt16LE(BOOT_PARAMS + 2), Math.ceil(size / SECTOR), 'a short last page counts');
      assert.equal(roml.readUInt16LE(BOOT_PARAMS + 4), 0x2f3c);
      assert.deepEqual([...roml.subarray(BOOT_PARAMS + 6, BOOT_ROML.length)], BOOT_ROML.slice(BOOT_PARAMS + 6));
      for (const at of BOOT_PAGE_AT) assert.equal(roml[at], REGS_BESIDE_GEORAM, `the block device's page at ROML ${at}`);
      assert.equal(roml.subarray(BOOT_ROML.length).every((b) => b === 0), true);
      assert.deepEqual([...romh.subarray(BOOT_CHIP - BOOT_VECTORS.length)], BOOT_VECTORS, 'the CPU vectors end ROMH');
      assert.equal(romh.subarray(0, BOOT_CHIP - BOOT_VECTORS.length).every((b) => b === 0), true, 'ROMH is not a function ROM of its own');
    }
  });
});

test('a GEOS 128 boot file that would reach I/O or the shared RAM is refused', { skip }, () => {
  withHarness((launch) => {
    const fits = (load, size) => launch(geos128Disk(START_UP_128, geos128Layout(load, size)), [], '128');
    assert.equal(fits(GEOS128_LOAD, 0xa400).status, 0, 'its last page ends below $D000');
    assert.equal(fits(0x2c00, 0xa400).status, 0, 'its last page ends at $D000');
    for (const [load, size] of [[GEOS128_LOAD, 0xa401], [0x1fff, BODY_SIZE]]) {
      const r = fits(load, size);
      assert.equal(r.status, 1, `load ${load.toString(16)}, ${size} bytes`);
      assert.match(r.err, /does not fit in memory/);
    }
    assert.equal(fits(0x2000, BODY_SIZE).status, 0, 'loaded right above the shared RAM');
  });
});

test('the GEOS 128 boot ROM reaches the block device on the page the launch puts it', { skip }, () => {
  withHarness((launch) => {
    const r = launch(geos128Disk(), [], '128', { REGS_PAGE: REGS_BESIDE_REU.toString(16) });
    assert.equal(r.status, 0, r.err);
    const roml = [...r.rom.subarray(0, BOOT_ROML.length)];
    assert.ok(BOOT_PAGE_AT.length > 0);
    for (const at of BOOT_PAGE_AT) assert.equal(roml[at], REGS_BESIDE_REU, `ROML ${at}`);
    const beside = launch(geos128Disk(), [], '128');
    const moved = roml.flatMap((byte, at) => (byte === beside.rom[at] ? [] : [at]));
    assert.deepEqual(moved, BOOT_PAGE_AT, 'nothing else in the ROM changes with the page');
  });
});
