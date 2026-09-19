// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { commandExists } from './lib/toolchain.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const builder = path.join(root, 'tools/build-c64.mjs');
const build = (...args) => spawnSync(process.execPath, [builder, ...args], { encoding: 'utf8' });

test('--list names every project and what it needs', () => {
  const r = build('--list');
  assert.equal(r.status, 0);
  assert.match(r.stdout, /^SettingsMenu\s+acme/m);
  assert.match(r.stdout, /^TRCustomBasicCommands\s+kickass/m);
  assert.match(r.stdout, /^BASIC\s+no assembler/m);
});

test('an unknown project is an error that lists the valid ones', () => {
  const r = build('--project', 'Nope');
  assert.equal(r.status, 1);
  assert.match(r.stderr, /unknown project "Nope".*SettingsMenu/);
});

test('an unknown or incomplete argument is an error, not silently ignored', () => {
  assert.equal(build('--projct', 'x').status, 1);
  assert.match(build('--projct', 'x').stderr, /Unknown argument --projct/);
  assert.match(build('--rom-dir').stderr, /Missing value for --rom-dir/);
});

// The real thing. Only when ACME is already installed: tests never download tools.
test('building reproduces the committed headers', { skip: !commandExists('acme') && 'acme is not on PATH' }, () => {
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'c64-build-test-'));
  try {
    const r = build('--project', 'SettingsMenu,TODCheck,BASIC', '--rom-dir', out);
    assert.equal(r.status, 0, r.stdout + r.stderr);
    const built = fs.readdirSync(out).sort();
    assert.ok(built.includes('SettingsMenu.prg.h') && built.includes('TODCheck.prg.h') && built.includes('SID_check.prg.h'));
    for (const file of built) {
      const committed = fs.readFileSync(path.join(root, 'Source/Teensy/TRMenuFiles/ROMs', file), 'utf8').replaceAll('\r\n', '\n');
      assert.equal(fs.readFileSync(path.join(out, file), 'utf8'), committed, `${file} differs from the committed header`);
    }
  } finally {
    fs.rmSync(out, { recursive: true, force: true });
  }
});
