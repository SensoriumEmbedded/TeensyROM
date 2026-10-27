// SPDX-License-Identifier: MIT
// Saving GeoRAM or REU banks to SD while the computer keeps writing them.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const HARNESS_SOURCE = path.join(import.meta.dirname, 'ram_banks_harness.cpp');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('c++') && !inCi && 'c++ is not on PATH';

const BANKS_BYTES = 4 * 0x2000;
const PRELOAD = 'georam.ram 3';

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ram-banks-'));
  harnessPath = path.join(dir, 'harness');
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function save(...args) {
  const ran = spawnSync(harness(), args, { encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.split('\n').filter(Boolean);
}

test('the REU save is the banks as they were when it began, and the computer runs again', { skip }, () => {
  assert.deepEqual(save('numbered'), ['consistent', PRELOAD, `reu000.reu ${BANKS_BYTES}`]);
});

test('the GeoRAM save is the banks as they were when it began, and the computer runs again', { skip }, () => {
  assert.deepEqual(save('replacing'), ['consistent', 'georam.bak 3', `georam.ram ${BANKS_BYTES}`]);
});

test('a computer that cannot be stopped is not saved, and the last save is kept', { skip }, () => {
  assert.deepEqual(save('replacing', 'holdfails'), [PRELOAD]);
  assert.deepEqual(save('numbered', 'holdfails'), [PRELOAD]);
});

test('a write the card refuses still lets the computer run again', { skip }, () => {
  assert.deepEqual(save('replacing', 'writefails'), [PRELOAD, 'georam.tmp 8192']);
});
