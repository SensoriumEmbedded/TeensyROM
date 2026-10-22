// SPDX-License-Identifier: MIT
//
// Only the paths that end before the tool looks for a board: nothing here may
// touch a Teensy that happens to be plugged in.
import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const flash = (...args) => spawnSync(process.execPath, [path.join(root, 'tools/flash-firmware.mjs'), ...args], { encoding: 'utf8' });

test('--help names every option and how the board is chosen', () => {
  const r = flash('--help');
  assert.equal(r.status, 0);
  for (const option of ['--hex', '--uid', '--port', '--all', '--check', '--help']) assert.match(r.stdout, new RegExp(option));
  assert.match(r.stdout, /Which board is written/);
  assert.match(r.stdout, /Examples:/);
});

test('an unknown or incomplete argument is an error, not silently ignored', () => {
  assert.match(flash('--allx').stderr, /Unknown argument --allx/);
  assert.equal(flash('--allx').status, 1);
  assert.match(flash('--uid').stderr, /Missing value for --uid/);
  assert.match(flash('--hex', '--all').stderr, /Missing value for --hex/);
});

test('a --hex that does not exist is refused before any board is looked for', () => {
  const r = flash('--hex', path.join(root, 'build/no-such-image.hex'));
  assert.equal(r.status, 1);
  assert.match(r.stderr, /No such hex/);
  assert.doesNotMatch(r.stdout, /Board/);
});
