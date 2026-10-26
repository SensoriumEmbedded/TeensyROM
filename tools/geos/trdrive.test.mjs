// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { commandExists } from '../lib/toolchain.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const trdrive = path.join(root, 'tools/geos/trdrive.py');
const committedHeader = path.join(root, 'Source/Teensy/GEOS/TRDriver.h');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['acme', 'python3'].find((tool) => !commandExists(tool));

test('the committed disk driver header matches the driver source', { skip: missingTool && !inCi && `${missingTool} is not on PATH` }, () => {
  assert.equal(missingTool, undefined, 'CI installs ACME and Python, so this test must not skip there');
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'trdrive-test-'));
  try {
    const header = path.join(out, 'TRDriver.h');
    const r = spawnSync('python3', [trdrive, header], { encoding: 'utf8' });
    assert.equal(r.status, 0, r.stdout + r.stderr);
    const committed = fs.readFileSync(committedHeader, 'utf8').replaceAll('\r\n', '\n');
    assert.equal(fs.readFileSync(header, 'utf8'), committed, 'rerun tools/geos/trdrive.py');
  } finally {
    fs.rmSync(out, { recursive: true, force: true });
  }
});
