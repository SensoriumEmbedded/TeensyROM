// SPDX-License-Identifier: MIT
// The USB mouse registers GeoRAM serves at $DF00, held to the register map in USBMouse.c.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const HARNESS_SOURCE = path.join(import.meta.dirname, 'usb_mouse_harness.cpp');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('c++') && !inCi && 'c++ is not on PATH';

const X = 0, Y = 1, BUTTONS = 2;
const COUNTS_PER_STEP = 4;
const MAX_STEP = 127;

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'usb-mouse-'));
  harnessPath = path.join(dir, 'harness');
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function run(lines) {
  const ran = spawnSync(harness(), { input: lines.join('\n') + '\n', encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.split('\n').filter(Boolean);
}

const report = (dx, dy, buttons = 0) => [`report ${dx} ${dy} ${buttons.toString(16)}`, 'poll'];
const read = (offset) => `r ${offset}`;
const signed = (byte) => (parseInt(byte, 16) << 24) >> 24;

test('the id is TRM at +3, and nothing answers after it', { skip }, () => {
  assert.deepEqual(run([3, 4, 5, 6, 7].map(read)), ['54', '52', '4d', 'open', 'open']);
});

test('a large movement arrives over several reads, 127 at most each, then the remainder', { skip }, () => {
  const steps = 300;
  const got = run([...report(steps * COUNTS_PER_STEP, 0), ...Array(5).fill(read(X))]).map(signed);
  assert.deepEqual(got, [MAX_STEP, MAX_STEP, steps - 2 * MAX_STEP, 0, 0]);
});

test('X and Y keep their own movement, whatever order they are read in', { skip }, () => {
  const got = run([...report(200 * COUNTS_PER_STEP, -150 * COUNTS_PER_STEP), ...[X, Y, Y, X, Y, X, X].map(read)]).map(signed);
  assert.deepEqual(got, [127, -127, -23, 73, 0, 0, 0]);
});

test('counts below a step carry over to the next report, rounding toward negative', { skip }, () => {
  assert.deepEqual(run([...report(3, -1), read(X), read(Y), ...report(1, 0), read(X), read(Y)]).map(signed), [0, -1, 1, 0]);
});

// A driver reads +0 and +1 from its interrupt, so any read can land while PollUSBMouse
// is part way through updating the byte the next read will drive.
for (const axis of [X, Y]) {
  test(`axis ${axis}: a read landing during the poll's update neither repeats nor loses movement`, { skip }, () => {
    const unread = 100, more = 20;
    const move = (steps) => report(axis === X ? steps * COUNTS_PER_STEP : 0, axis === Y ? steps * COUNTS_PER_STEP : 0);
    const lines = run([...move(unread), `race ${axis}`, ...move(more), ...Array(5).fill(read(axis))]);
    assert.equal(lines[0], `race ${unread.toString(16)}`);
    assert.deepEqual(lines.slice(1).map(signed), [more, 0, 0, 0, 0]);
  });

  test(`axis ${axis}: a read landing during the poll's update that leaves the same byte ready does not repeat movement`, { skip }, () => {
    const steps = 10;
    const move = (n) => report(axis === X ? n * COUNTS_PER_STEP : 0, axis === Y ? n * COUNTS_PER_STEP : 0);
    const lines = run([...move(steps), `race ${axis}`, ...move(steps), ...Array(3).fill(read(axis))]);
    assert.equal(lines[0], `race ${steps.toString(16).padStart(2, '0')}`);
    assert.deepEqual(lines.slice(1).map(signed), [steps, 0, 0]);
  });
}

test('a weak compare-exchange failing for no reason still leaves the new movement ready', { skip }, () => {
  assert.deepEqual(run(['spurious 0', ...report(20 * COUNTS_PER_STEP, 0), read(X)]).map(signed), [20]);
});

test('the handler starting discards movement from before it, both polled and still in the controller', { skip }, () => {
  const polled = report(40 * COUNTS_PER_STEP, 40 * COUNTS_PER_STEP);
  const pending = `report ${30 * COUNTS_PER_STEP} ${30 * COUNTS_PER_STEP} 0`;
  const got = run([...polled, pending, 'init', 'poll', read(X), read(Y), ...report(COUNTS_PER_STEP, -COUNTS_PER_STEP), read(X), read(Y)]).map(signed);
  assert.deepEqual(got, [0, 0, 1, -1]);
});

test('the buttons register follows the latest report, and clears when the mouse is unplugged', { skip }, () => {
  const got = run([...report(0, 0, 0b101), read(BUTTONS), ...report(0, 0, 0b010), read(BUTTONS), 'unplug', 'poll', read(BUTTONS)]);
  assert.deepEqual(got, ['05', '02', '00']);
});

test('a report the controller held when the mouse was unplugged is not delivered on replug', { skip }, () => {
  const got = run(['unplug', `report ${10 * COUNTS_PER_STEP} 0 1`, 'poll', 'plug', 'poll', read(X), read(BUTTONS)]);
  assert.deepEqual(got, ['00', '00']);
});
