// SPDX-License-Identifier: MIT
// What the TR menu reads from a D64, D71 or D81 (Source/Teensy/DiskImage/MenuDisk.h, used by D64.ino).
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/menu_disk_harness.c');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['cc', 'c++'].find((tool) => !commandExists(tool));
const skip = missingTool && !inCi && `${missingTool} is not on PATH`;

const SECTOR = 256;
const KIND = { d64: 0, d71: 1, d81: 2 };
const zoneSectors = (track) => {
  const zone = (track - 1) % 35 + 1;
  return zone <= 17 ? 21 : zone <= 24 ? 19 : zone <= 30 ? 18 : 17;
};
const sectorsOn = (kind, track) => (kind === 'd81' ? 40 : kind === 'd64' && track > 35 ? 17 : zoneSectors(track));

function offset(kind, track, sector) {
  let blocks = sector;
  for (let t = 1; t < track; t++) blocks += sectorsOn(kind, t);
  return blocks * SECTOR;
}

// A file laid along the given sectors, its last one partly used; returns its bytes
function putFile(image, kind, sectors, lastLength) {
  const parts = [];
  sectors.forEach(([track, sector], i) => {
    const last = i === sectors.length - 1;
    const length = last ? lastLength : SECTOR - 2;
    const data = Buffer.from(Array.from({ length }, (_, n) => (n * 13 + i * 7 + track) & 0xff));
    const at = offset(kind, track, sector);
    image[at] = last ? 0 : sectors[i + 1][0];
    image[at + 1] = last ? length + 1 : sectors[i + 1][1];
    data.copy(image, at + 2);
    parts.push(data);
  });
  return Buffer.concat(parts);
}

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  assert.equal(missingTool, undefined, 'CI installs every tool this test needs, so it must not skip there');
  harnessPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'menu-disk-')), 'harness');
  const built = spawnSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  const asCpp = spawnSync('c++', ['-x', 'c++', '-std=gnu++17', '-Wall', '-Werror', '-fsyntax-only', HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(asCpp.status, 0, `the firmware compiles this as C++: ${asCpp.stderr}`);
  return harnessPath;
}

function load(kind, image, [track, sector], capacity = 1 << 20) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'menu-disk-run-'));
  try {
    const input = path.join(dir, 'in'), out = path.join(dir, 'out');
    fs.writeFileSync(input, image);
    const ran = spawnSync(harness(), ['read', KIND[kind], input, track, sector, capacity, out], { encoding: 'utf8' });
    assert.equal(ran.status, 0, ran.stderr);
    const said = ran.stdout.trim();
    return { said, bytes: said.startsWith('length=') ? fs.readFileSync(out) : null };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

const IMAGES = [
  { label: 'd64', kind: 'd64', size: 174848, chain: [[17, 0], [1, 5], [35, 16]] },
  { label: 'd64 with error bytes', kind: 'd64', size: 175531, chain: [[18, 18], [35, 16]] },
  { label: 'd64 with 40 tracks', kind: 'd64', size: 196608, chain: [[35, 16], [40, 16]] },
  { label: 'd71', kind: 'd71', size: 349696, chain: [[35, 0], [36, 20], [70, 16]] },
  { label: 'd81', kind: 'd81', size: 819200, chain: [[1, 0], [40, 39], [80, 39]] },
  { label: 'd81 with error bytes', kind: 'd81', size: 822400, chain: [[80, 0]] },
];

for (const { label, kind, size, chain } of IMAGES) {
  test(`${label}: a file loads whole, its last sector only as far as its link says`, { skip }, () => {
    const image = Buffer.alloc(size);
    const expected = putFile(image, kind, chain, 100);
    const { said, bytes } = load(kind, image, chain[0]);
    assert.equal(said, `length=${expected.length}`);
    assert.deepEqual(bytes, expected);
  });
}

test('a short D64 (Realm.d64 is 171520 bytes) opens, and a link past its end is a broken file', { skip }, () => {
  const image = Buffer.alloc(171520);
  const expected = putFile(image, 'd64', [[1, 0], [1, 10]], 3);
  assert.deepEqual(load('d64', image, [1, 0]).bytes, expected);
  putFile(image, 'd64', [[2, 0], [35, 16]], 3);
  assert.equal(load('d64', image, [2, 0]).said, 'broken');
});

test('a chain that loops, or links off the image, is a broken file', { skip }, () => {
  const image = Buffer.alloc(174848);
  putFile(image, 'd64', [[1, 0], [1, 1]], 10);
  image[offset('d64', 1, 1)] = 1;
  image[offset('d64', 1, 1) + 1] = 0;
  assert.equal(load('d64', image, [1, 0]).said, 'broken');
  putFile(image, 'd64', [[2, 0], [36, 0]], 10);
  assert.equal(load('d64', image, [2, 0]).said, 'broken');
});

test('a file longer than the buffer is too big, and one exactly its size fits', { skip }, () => {
  const image = Buffer.alloc(174848);
  const expected = putFile(image, 'd64', [[1, 0], [1, 1]], 100);
  assert.equal(load('d64', image, [1, 0], expected.length - 1).said, 'too big');
  assert.equal(load('d64', image, [1, 0], expected.length).said, `length=${expected.length}`);
});

test('a file of a size outside its kind\'s is refused', { skip }, () => {
  for (const [kind, size] of [['d64', 171519], ['d64', 197377], ['d71', 349695], ['d71', 351063], ['d81', 819199], ['d81', 822401]]) {
    assert.equal(load(kind, Buffer.alloc(size), [1, 0]).said, 'refused', `${kind} of ${size} bytes`);
  }
});
