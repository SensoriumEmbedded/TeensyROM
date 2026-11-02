// SPDX-License-Identifier: MIT
// The shared D64/D71/D81 engine (Source/Teensy/DiskImage), built with the firmware's warnings as C and as C++.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/disk_image_harness.c');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['cc', 'c++'].find((tool) => !commandExists(tool));
const skip = missingTool && !inCi && `${missingTool} is not on PATH`;

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  assert.equal(missingTool, undefined, 'CI installs every tool this test needs, so it must not skip there');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'disk-image-'));
  harnessPath = path.join(dir, 'harness');
  const built = spawnSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  const asCpp = spawnSync('c++', ['-x', 'c++', '-std=gnu++17', '-Wall', '-Werror', '-fsyntax-only', HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(asCpp.status, 0, `the firmware compiles this as C++: ${asCpp.stderr}`);
  return harnessPath;
}

const runHarness = (...args) => {
  const ran = spawnSync(harness(), args, { encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.trim();
};

// 1541 DOS order: a file starts on the track next to the directory (18, or 40 on a 1581), at sector 0
const NEW_DISKS = {
  d64: 'free=664,661 blocks=3 first=17/0 same=1',
  d71: 'free=1328,1325 blocks=3 first=17/0 same=1',
  d81: 'free=3160,3157 blocks=3 first=39/0 same=1',
};

for (const [kind, expected] of Object.entries(NEW_DISKS)) {
  test(`${kind}: a file saved on a blank disk reads back, and takes its blocks from the free count`, { skip }, () => {
    assert.equal(runHarness('new', kind), expected);
  });
}

test('the image kind follows the file size, error bytes and 40-track D64s included', { skip }, () => {
  const sizes = {
    174847: 'none',
    174848: 'd64 35',
    175531: 'd64 35',
    196608: 'd64 40',
    197376: 'd64 40',
    349696: 'd71 70',
    351062: 'd71 70',
    819200: 'd81 80',
    822400: 'd81 80',
  };
  for (const [size, expected] of Object.entries(sizes)) assert.equal(runHarness('size', size), expected, `${size} bytes`);
});
