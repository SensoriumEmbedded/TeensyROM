// SPDX-License-Identifier: MIT
// The item name the TR menu sends the C64 (rsstItemName), cut to the display width
// and, with Show Extensions off, without its extension.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/menu_item_name_harness.c');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('cc') && !inCi && 'cc is not on PATH';

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'menu-item-name-'));
  harnessPath = path.join(dir, 'menu_item_name_harness');
  const built = spawnSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function shown(hide, name) {
  const ran = spawnSync(harness(), [], { input: `${hide ? 1 : 0} ${name}\n`, encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.trim().slice(1, -1);
}

test('a name longer than the display hides its own extension, not a dot inside the cut', { skip }, () => {
  assert.equal(shown(true, 'TeensyROM+_0.9.0.69-DMA-noisr_full.hex'), 'TeensyROM+_0.9.0.69-DMA-noisr_full');
  assert.equal(shown(true, 'TeensyROM+_0.9.0.69-DMA-noisr_full_long.hex'), 'TeensyROM+_0.9.0.69-DMA-noisr_full');
});

test('a short name loses only its extension', { skip }, () => {
  assert.equal(shown(true, 'Game v1.2.prg'), 'Game v1.2');
  assert.equal(shown(true, 'NoExtension'), 'NoExtension');
  assert.equal(shown(true, ''), '');
});

test('the extension is cut where the display width ends inside it', { skip }, () => {
  // 33 characters, then ".prg": the cut keeps the dot and nothing after it
  const stem = 'A'.repeat(33);
  assert.equal(shown(true, `${stem}.prg`), stem);
  assert.equal(shown(false, `${stem}.prg`), `${stem}.`);
});

test('with Show Extensions on, the name is only cut to the display width', { skip }, () => {
  assert.equal(shown(false, 'TeensyROM+_0.9.0.69-DMA-noisr_full.hex'), 'TeensyROM+_0.9.0.69-DMA-noisr_full');
  assert.equal(shown(false, 'Game v1.2.prg'), 'Game v1.2.prg');
});
