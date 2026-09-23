// SPDX-License-Identifier: MIT
// Argument handling only. Both cases here are refusals that happen before the script
// copies the Teensy core, so they cost a process spawn rather than a build.
//
// That extensions are *on* by default for --target tr-plus is not testable this cheaply:
// the reservation happens after that copy. The real builds carry that claim -- CI builds
// both targets, and Common_Defs.h fails the compile if an extensions build ever reaches a
// target without Fab04_FullDMACapable.
import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const script = path.join(path.dirname(fileURLToPath(import.meta.url)), 'build-firmware.mjs');
// The timeout is part of the assertion. Every case here must be refused before the script
// starts copying the Teensy core; without it, a build that stopped refusing would run for
// minutes and report as a slow pass rather than a failure.
const build = (...args) =>
  spawnSync(process.execPath, [script, ...args], { encoding: 'utf8', timeout: 20_000 });

test('extensions are refused on a target whose DMA cannot blank the screen', () => {
  const result = build('--target', 'tr', '--with-extensions');
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /--with-extensions needs --target tr-plus/);
});

test('a missing or unknown target is refused', () => {
  assert.match(build().stderr, /Use --target tr or --target tr-plus/);
  assert.match(build('--target', 'tr-minus').stderr, /Use --target tr or --target tr-plus/);
});
