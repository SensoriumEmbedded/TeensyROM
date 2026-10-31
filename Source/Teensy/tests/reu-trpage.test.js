// SPDX-License-Identifier: MIT
// The REU's IO2 page under "REU+USB Mouse/Disk", where the TR's GEOS drivers look for the block device first.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const HARNESS_SOURCE = path.join(import.meta.dirname, 'reu_trpage_harness.cpp');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('c++') && !inCi && 'c++ is not on PATH';

const BANKS = 16;
const BLOCK_ID = 0x10;

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'reu-trpage-'));
  harnessPath = path.join(dir, 'harness');
  // the REU's DMA loops spin on an empty body, which the firmware's own build allows
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-Wno-empty-body', '-o', harnessPath,
    HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function run(lines) {
  const ran = spawnSync(harness(), { input: lines.join('\n') + '\n', encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.split('\n').filter(Boolean);
}

const read = (offset) => `r ${offset.toString(16)}`;

test('beside an REU, its registers answer and the rest of each 32 bytes reads $FF, as a 1764\'s do', { skip }, () => {
  const offsets = [0x09, 0x0a, 0x0b, BLOCK_ID, BLOCK_ID + 2, 0x1f, 0x2a, 0x20 + BLOCK_ID, 0xff];
  assert.deepEqual(run(['init 0', `banks ${BANKS}`, ...offsets.map(read)]),
    ['1f', '3f', 'ff', 'ff', 'ff', 'ff', '3f', 'ff', 'ff']);
});

test('with no RAM for the REU, nothing in IO2 answers, so software finds no REU there', { skip }, () => {
  assert.deepEqual(run(['init 0', 'banks 0', ...[0x00, 0x0a, BLOCK_ID].map(read)]), ['open', 'open', 'open']);
});
