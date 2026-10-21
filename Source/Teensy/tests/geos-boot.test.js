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
const SYSTEM_FILE = 5;

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

const header = fs.readFileSync(DRIVER_HEADER, 'utf8');
const arrayOf = (name) => [...header.match(new RegExp(`${name}\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16));
const CODE = arrayOf('TRDriverCode');
const RELOCATIONS = arrayOf('TRDriverRelocations');
const ENTRIES = arrayOf('TRDriverEntries');

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

function bootFile(driver = stockDriver()) {
  const body = Buffer.alloc(BODY_SIZE);
  for (let i = 0; i < BODY_SIZE; i++) body[i] = (i * 31 + 7) & 0xff;
  driver.copy(body, DRIVER_AT_IN_BODY);
  const file = Buffer.alloc(2 + BODY_SIZE);
  file.writeUInt16LE(LOAD_ADDRESS, 0);
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

function run(args) {
  const r = spawnSync(args[0], args.slice(1), { encoding: 'utf8' });
  return { status: r.status, out: r.stdout, err: r.stderr };
}

function withHarness(body) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'geos-boot-'));
  try {
    const built = run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-o', path.join(dir, 'harness'), HARNESS_SOURCE]);
    assert.equal(built.status, 0, built.err);
    const asCpp = run(['c++', '-x', 'c++', '-std=gnu++17', '-Wall', '-Werror', '-fsyntax-only', HARNESS_SOURCE]);
    assert.equal(asCpp.status, 0, asCpp.err);
    const launch = (disk) => {
      fs.writeFileSync(path.join(dir, 'in.d81'), disk.image);
      const r = run([path.join(dir, 'harness'), path.join(dir, 'in.d81'), path.join(dir, 'out.prg'), path.join(dir, 'out.d81')]);
      return { ...r, prg: r.status === 0 ? fs.readFileSync(path.join(dir, 'out.prg')) : null, image: r.status === 0 ? fs.readFileSync(path.join(dir, 'out.d81')) : null };
    };
    body(launch);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

function standardDisk(driver) {
  const disk = new Disk();
  disk.addFile('GEOS', Buffer.from([1, 8, 0]));
  disk.addFile('GEOBOOT', bootFile(driver).file);
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
    const other = disk.addFile('pad color mgr', Buffer.from([1, 2, 3]), AUTO_EXEC);
    const plain = disk.addFile('CONFIGURE.DOC', Buffer.from([1, 2, 3]), 0);

    const first = launch(disk);
    assert.equal(first.status, 0, first.err);
    assert.match(first.out, /changed=1/);
    assert.equal(first.image[configure + SLOT_GEOS_TYPE], SYSTEM_FILE);
    assert.equal(first.image[second + SLOT_GEOS_TYPE], SYSTEM_FILE);
    assert.equal(first.image[other + SLOT_GEOS_TYPE], AUTO_EXEC, 'other auto-exec files stay');
    assert.equal(first.image[plain + SLOT_GEOS_TYPE], 0, 'a CONFIG file that is not auto-exec stays');

    const again = launch({ image: first.image });
    assert.match(again.out, /changed=0/);
    assert.equal(again.image.equals(first.image), true);
  });
});
