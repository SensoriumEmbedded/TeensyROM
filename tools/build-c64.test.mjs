// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { commandExists } from './lib/toolchain.mjs';
import { loadProjects } from './lib/c64-projects.mjs';

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

// KickAssembler projects are left out: it needs a JRE, and this test never
// downloads a tool.
const acmeOnlyProjects = loadProjects(root).filter((p) => p.steps.every((step) => step.assembler !== 'kickass'));
const expectedHeaders = acmeOnlyProjects.flatMap((p) => p.headers.map((header) => header.dest)).sort();

// A runner sets CI to one of these to mean it is not one.
const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');

// The real thing. Skipped without ACME locally, but CI installs it, so there a
// missing assembler is the failure rather than a silent pass.
test('building reproduces the committed headers', { skip: !commandExists('acme') && !inCi && 'acme is not on PATH' }, () => {
  assert.ok(commandExists('acme'), 'CI installs ACME, so this test must not skip there');
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'c64-build-test-'));
  try {
    const r = build('--project', acmeOnlyProjects.map((p) => p.name).join(','), '--rom-dir', out);
    assert.equal(r.status, 0, r.stdout + r.stderr);
    const built = fs.readdirSync(out).sort();
    assert.deepEqual(built, expectedHeaders, 'the build wrote a different set of headers than the manifest declares');
    for (const file of built) {
      const committed = fs.readFileSync(path.join(root, 'Source/Teensy/TRMenuFiles/ROMs', file), 'utf8').replaceAll('\r\n', '\n');
      assert.equal(fs.readFileSync(path.join(out, file), 'utf8'), committed, `${file} differs from the committed header`);
    }
  } finally {
    fs.rmSync(out, { recursive: true, force: true });
  }
});
