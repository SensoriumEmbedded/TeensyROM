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
  return ran.stdout.split(/\r?\n/).filter(Boolean);
}

const read = (offset) => `r ${offset.toString(16)}`;
const write = (offset, value) => [`d ${value.toString(16)}`, `w ${offset.toString(16)}`];

test('beside an REU, its registers answer and the rest of each 32 bytes reads $FF, as a 1764\'s do', { skip }, () => {
  const offsets = [0x09, 0x0a, 0x0b, BLOCK_ID, BLOCK_ID + 2, 0x1f, 0x2a, 0x20 + BLOCK_ID, 0xff];
  assert.deepEqual(run(['init 0', `banks ${BANKS.toString(16)}`, ...offsets.map(read)]),
    ['1f', '3f', 'ff', 'ff', 'ff', 'ff', '3f', 'ff', 'ff']);
});

test('with no RAM for the REU, nothing in IO2 answers, so software finds no REU there', { skip }, () => {
  assert.deepEqual(run(['init 0', 'banks 0', ...[0x00, 0x0a, BLOCK_ID].map(read)]), ['open', 'open', 'open']);
});

test('under the stock REU, the unused registers stay open bus', { skip }, () => {
  assert.deepEqual(run(['init 0', `banks ${BANKS.toString(16)}`, 'stock 0', ...[0x0a, BLOCK_ID, 0x1f].map(read)]), ['3f', 'open', 'open']);
});

// The C64 can start a transfer between a host job's setup and its slice, or between two slices.
test('a transfer the C64 starts leaves a host DMA job set up as it found it', { skip }, () => {
  const transfer = [...write(0x07, 0x10), ...write(0x08, 0x00), ...write(0x0a, 0x80), ...write(0x01, 0x91)];
  assert.deepEqual(run(['init 0', `banks ${BANKS.toString(16)}`, 'job c000', ...transfer, 'dma 0']), ['1 0 c000 40 0']);
});

test('a transfer the C64 starts at once leaves another owner\'s bus snoop in place', { skip }, () => {
  const transfer = [...write(0x07, 0x10), ...write(0x08, 0x00), ...write(0x01, 0x91)];
  assert.deepEqual(run(['init 0', `banks ${BANKS.toString(16)}`, 'watch 0', ...transfer, 'snoop 0']), ['watch']);
});

// A host write (WriteREUToken) goes into the REU's banks from the main loop, not over the C64 bus.
const at = (offset) => `at ${offset.toString(16)}`;
const fits = (length) => `fits ${length.toString(16)}`;
const runFrom = (length) => `run ${length.toString(16)}`;
const SIZE = BANKS * 0x2000;

test('a host write that reaches the REU\'s last byte fits, and one a byte longer is refused', { skip }, () => {
  assert.deepEqual(run([`banks ${BANKS.toString(16)}`,
    at(0), fits(SIZE), fits(SIZE + 1),
    at(SIZE - 1), fits(1), fits(2),
    at(SIZE), fits(0), fits(1),
    at(SIZE + 1), fits(0),
    at(0xffffff), fits(0xffffff)]),
  ['fits', 'refused', 'fits', 'refused', 'fits', 'refused', 'refused', 'refused']);
});

test('a host write is cut at each bank\'s end, so its next bytes go to the next bank', { skip }, () => {
  assert.deepEqual(run([`banks ${BANKS.toString(16)}`,
    at(0), runFrom(0x10000),
    at(0x1ff0), runFrom(0x40),
    at(0x2000), runFrom(0x40),
    at(SIZE - 0x10), runFrom(0x10)]),
  ['0 0 2000', '0 1ff0 10', '1 0 40', 'f 1ff0 10']);
});
