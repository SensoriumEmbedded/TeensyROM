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
const trmouse = path.join(root, 'tools/geos/trmouse.py');
const committedHeaders = { 128: 'Source/Teensy/GEOS/TRMouse128.h', 64: 'Source/Teensy/GEOS/TRMouse64.h' };

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const missingTool = ['acme', 'python3'].find((tool) => !commandExists(tool));

for (const [target, committedPath] of Object.entries(committedHeaders)) {
  test(`the committed GEOS ${target} driver header matches the driver source`, { skip: missingTool && !inCi && `${missingTool} is not on PATH` }, () => {
    assert.equal(missingTool, undefined, 'CI installs ACME and Python, so this test must not skip there');
    const out = fs.mkdtempSync(path.join(os.tmpdir(), 'trmouse-test-'));
    try {
      const header = path.join(out, 'TRMouse.h');
      const r = spawnSync('python3', [trmouse, '--target', target, '--c-header', header], { encoding: 'utf8' });
      assert.equal(r.status, 0, r.stdout + r.stderr);
      const committed = fs.readFileSync(path.join(root, committedPath), 'utf8').replaceAll('\r\n', '\n');
      const first = committed.match(/Code\[\d+\] PROGMEM = \{\s*0x4c, 0x([0-9a-f]{2}), 0x([0-9a-f]{2})/);
      assert.equal(parseInt(first[2] + first[1], 16) >> 8, target === '64' ? 0xfe : 0xfd, 'the first jump lands in the target\'s own driver area');
      assert.equal(fs.readFileSync(header, 'utf8'), committed, `rerun tools/geos/trmouse.py --target ${target} --c-header`);
    } finally {
      fs.rmSync(out, { recursive: true, force: true });
    }
  });
}
