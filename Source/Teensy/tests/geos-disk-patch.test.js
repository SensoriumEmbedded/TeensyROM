// SPDX-License-Identifier: MIT
// The firmware's GEOS boot-disk patch, held byte for byte to tools/geos/trmouse.py on synthetic boot disks.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const TRMOUSE = path.resolve(FIRMWARE_ROOT, '../../tools/geos/trmouse.py');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/geos_disk_patch_harness.c');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['acme', 'python3', 'cc', 'c++'].find((tool) => !commandExists(tool));
const skip = missingTool && !inCi && `${missingTool} is not on PATH`;

const SECTOR = 256;
const PAD = 0xa0;
const USR_CLOSED = 0x83;
const INPUT_128 = 15;
const MOUSE_NAME = 'TR USB MOUSE';
const DATE_IN_SLOT = 25;
const DATE_BYTES = 5;

const zoneSectors = (track) => {
  const zone = (track - 1) % 35 + 1;
  return zone <= 17 ? 21 : zone <= 24 ? 19 : zone <= 30 ? 18 : 17;
};

const FORMATS = {
  d64: { tracks: 35, sectors: zoneSectors, dirTrack: 18, firstDir: 1 },
  d71: { tracks: 70, sectors: zoneSectors, dirTrack: 18, firstDir: 1 },
  d81: { tracks: 80, sectors: () => 40, dirTrack: 40, firstDir: 3 },
};

function offset(format, track, sector) {
  let blocks = sector;
  for (let t = 1; t < track; t++) blocks += format.sectors(t);
  return blocks * SECTOR;
}

function imageSize(format) {
  return offset(format, format.tracks + 1, 0);
}

function bamPlaces(kind, track) {
  if (kind === 'd81') {
    const entry = 0x10 + ((track - 1) % 40) * 6;
    const sector = 1 + Math.floor((track - 1) / 40);
    return { count: [40, sector, entry], bitmap: [40, sector, entry + 1] };
  }
  if (track <= 35) return { count: [18, 0, 4 + (track - 1) * 4], bitmap: [18, 0, 5 + (track - 1) * 4] };
  return { count: [18, 0, 0xdd + track - 36], bitmap: [53, 0, (track - 36) * 3] };
}

function at(disk, [track, sector, byte]) {
  return offset(disk.format, track, sector) + byte;
}

function setTrackFree(disk, track, freeSectors) {
  const { count, bitmap } = bamPlaces(disk.kind, track);
  const bits = at(disk, bitmap);
  for (let s = 0; s < disk.format.sectors(track); s++) {
    const mask = 1 << (s % 8);
    if (freeSectors.includes(s)) disk.image[bits + (s >> 3)] |= mask;
    else disk.image[bits + (s >> 3)] &= ~mask;
  }
  disk.image[at(disk, count)] = freeSectors.length;
}

function allSectors(disk, track) {
  return Array.from({ length: disk.format.sectors(track) }, (_, s) => s);
}

function writeHeader(disk) {
  const { image, format, kind } = disk;
  const header = offset(format, format.dirTrack, 0);
  image.set([format.dirTrack, format.firstDir], header);
  if (kind === 'd81') {
    image.set([40, 2, 0x44, 0xbb], offset(format, 40, 1));
    image.set([0, 0xff, 0x44, 0xbb], offset(format, 40, 2));
  }
  if (kind === 'd71') image[header + 3] = 0x80;
  image[offset(format, format.dirTrack, format.firstDir) + 1] = 0xff;
}

function blankDisk(kind) {
  const format = FORMATS[kind];
  const disk = { kind, format, image: new Uint8Array(imageSize(format)) };
  writeHeader(disk);
  for (let track = 1; track <= format.tracks; track++) setTrackFree(disk, track, allSectors(disk, track));
  const systemSectors = kind === 'd81' ? [0, 1, 2, 3] : [0, 1];
  setTrackFree(disk, format.dirTrack, allSectors(disk, format.dirTrack).filter((s) => !systemSectors.includes(s)));
  if (kind === 'd71') setTrackFree(disk, 53, []);
  return disk;
}

function putEntry(disk, slot, { name, type = USR_CLOSED, geosType = 0 }) {
  const base = offset(disk.format, disk.format.dirTrack, disk.format.firstDir) + slot * 32;
  disk.image.fill(PAD, base + 5, base + 21);
  disk.image.set(Buffer.from(name, 'latin1'), base + 5);
  disk.image[base + 2] = type;
  disk.image[base + 24] = geosType;
}

function withDriver(kind) {
  const disk = blankDisk(kind);
  putEntry(disk, 0, { name: '128 JOYSTICK', geosType: INPUT_128 });
  putEntry(disk, 1, { name: '128 DESKTOP', geosType: 6 });
  return disk;
}

function fullDirectory(kind) {
  const disk = blankDisk(kind);
  for (let slot = 0; slot < 8; slot++) {
    putEntry(disk, slot, slot === 3 ? { name: '128 JOYSTICK', geosType: INPUT_128 } : { name: `FILE ${slot}` });
  }
  return disk;
}

const PATCHED_CASES = {
  'a blank boot disk': blankDisk,
  'a disk whose first input driver comes earlier': withDriver,
  'a full first directory sector': fullDirectory,
};

const REFUSED_CASES = {
  'a disk that already has the driver': (kind) => {
    const disk = withDriver(kind);
    putEntry(disk, 2, { name: MOUSE_NAME, geosType: INPUT_128 });
    return disk;
  },
  'a BAM count below its bitmap': (kind) => {
    const disk = blankDisk(kind);
    disk.image[at(disk, bamPlaces(kind, 1).count)] -= 1;
    return disk;
  },
  'a header linking outside the directory': (kind) => {
    const disk = blankDisk(kind);
    disk.image[offset(disk.format, disk.format.dirTrack, 0)] = 1;
    return disk;
  },
  'a full directory whose BAM frees its sector': (kind) => {
    const disk = fullDirectory(kind);
    const free = allSectors(disk, disk.format.dirTrack).filter((s) => s >= disk.format.firstDir);
    setTrackFree(disk, disk.format.dirTrack, free);
    return disk;
  },
};

function maskMouseDate(image) {
  const name = Buffer.concat([Buffer.from(MOUSE_NAME, 'latin1'), Buffer.alloc(16 - MOUSE_NAME.length, PAD)]);
  const found = Buffer.from(image).indexOf(name);
  assert.notEqual(found, -1, 'the patched image has no TR USB MOUSE entry');
  const date = found - 5 + DATE_IN_SLOT;
  const masked = Buffer.from(image);
  masked.fill(0, date, date + DATE_BYTES);
  return masked;
}

function run(command, args) {
  return spawnSync(command, args, { encoding: 'utf8' });
}

function patchBoth(dir, label, image) {
  const input = path.join(dir, `${label}.in`);
  const viaPython = path.join(dir, `${label}.py.out`);
  const viaFirmware = path.join(dir, `${label}.c.out`);
  fs.writeFileSync(input, image);
  return {
    python: { ...run('python3', [TRMOUSE, input, viaPython]), out: viaPython },
    firmware: { ...run(path.join(dir, 'harness'), [input, viaFirmware]), out: viaFirmware },
  };
}

test('the firmware patch matches trmouse.py', { skip }, async (t) => {
  assert.equal(missingTool, undefined, 'CI installs every tool this test needs, so it must not skip there');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'geos-disk-patch-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));

  const built = run('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-o', path.join(dir, 'harness'), HARNESS_SOURCE]);
  assert.equal(built.status, 0, built.stderr);
  const asCpp = run('c++', ['-x', 'c++', '-std=gnu++17', '-Wall', '-Werror', '-fsyntax-only', HARNESS_SOURCE]);
  assert.equal(asCpp.status, 0, `the firmware compiles this as C++: ${asCpp.stderr}`);

  for (const kind of Object.keys(FORMATS)) {
    for (const [label, make] of Object.entries(PATCHED_CASES)) {
      await t.test(`${kind}: ${label}`, () => {
        const { python, firmware } = patchBoth(dir, `${kind}-${label}`, make(kind).image);
        assert.equal(python.status, 0, python.stderr);
        assert.equal(firmware.status, 0, firmware.stderr);
        assert.deepEqual(maskMouseDate(fs.readFileSync(firmware.out)), maskMouseDate(fs.readFileSync(python.out)));
      });
    }
    for (const [label, make] of Object.entries(REFUSED_CASES)) {
      await t.test(`${kind}: refuses ${label}`, () => {
        const { python, firmware } = patchBoth(dir, `${kind}-${label}`, make(kind).image);
        assert.equal(firmware.status, 1, `the firmware patched it: ${firmware.stderr}`);
        assert.notEqual(python.status, 0, 'trmouse.py patched it');
        assert.equal(firmware.stderr.trim(), python.stderr.trim());
      });
    }
  }

  await t.test('d71: refuses a single-sided disk', () => {
    const disk = blankDisk('d71');
    disk.image[offset(disk.format, 18, 0) + 3] = 0;
    const { python, firmware } = patchBoth(dir, 'd71-single', disk.image);
    assert.equal(firmware.status, 1);
    assert.equal(firmware.stderr.trim(), python.stderr.trim());
  });
});
