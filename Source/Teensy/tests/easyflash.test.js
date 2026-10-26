// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { blankComments } from '../../../tools/lib/source-text.mjs';
import { makeEasyFlashFixture } from './easyflash-fixtures.mjs';

const root = path.resolve(import.meta.dirname, '..');
// Main-image coverage is deliberately limited to routing and the static table
// boundary. Extract complete named functions, without editing their bodies.
function definition(source, name) {
  const clean = blankComments(source);
  const match = new RegExp(String.raw`^[^\n;{}]*\b${name}\s*\([^;{}]*\)\s*\{`, 'm').exec(clean);
  assert(match, `Missing production function ${name}`);
  const open = clean.indexOf('{', match.index);
  let depth = 0;
  for (let at = open; at < clean.length; ++at) {
    if (clean[at] === '{') ++depth;
    else if (clean[at] === '}' && --depth === 0) return source.slice(match.index, at + 1);
  }
  assert.fail(`Unbalanced production function ${name}`);
}

test('EasyFlash native loader, routing and fixed VM profile regressions', (t) => {
  const candidates = process.env.CXX ? [process.env.CXX] : ['g++', 'clang++',
    ...(process.platform === 'win32' ? ['C:/msys64/mingw32/bin/g++.exe'] : [])];
  const compiler = candidates.find((command) => spawnSync(command, ['--version'], { windowsHide: true }).status === 0);
  if (!compiler) return t.skip('Set CXX to g++ or clang++ to run the native firmware harness');
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'teensyrom-easyflash-'));
  try {
    const parser = fs.readFileSync(path.join(root, 'FileParsers.ino'), 'utf8');
    const loader = fs.readFileSync(path.join(root, 'DriveDirLoad.ino'), 'utf8');
    assert.match(blankComments(parser), /uint8_t\s+NumCrtChips\s*=\s*0\s*;/);
    assert.match(blankComments(parser), /StructCrtChip\s+CrtChips\s*\[\s*MAX_CRT_CHIPS\s*\]\s*;/);
    fs.writeFileSync(path.join(temporary, 'main-functions.h'), [
      ...['BootCrtFromSD', 'ParseCRTHeader', 'ParseChipHeader', 'SetTypeFromCRT'].map((name) => definition(parser, name)),
      ...['LoadFile', 'FreeCrtChips'].map((name) => definition(loader, name)),
    ].join('\n'));
    const fixtures = [{mib: 2}, {mib: 4}, {mib: 2, sparse: true, reverse: true}, {mib: 4, sparse: true, reverse: true}].map((options) => {
      const file = path.join(temporary, `bank-identifiers-${options.mib}m${options.sparse ? '-sparse-reversed' : ''}.crt`);
      fs.writeFileSync(file, makeEasyFlashFixture(options)); return file;
    });
    // Optional local readback only; cartridge bytes are never copied into Git.
    fixtures.push(...JSON.parse(process.env.TEENSYROM_EASYFLASH_CRTS ?? '[]').map((file) => fs.realpathSync(file)));
    const env = { ...process.env, PATH: path.dirname(compiler) + path.delimiter + process.env.PATH };
    for (const [profile, flags] of [['minimal', []], ['main', ['-DTEST_MAIN_IMAGE']], ['vm-host', ['-DVM_HOST_PROFILE']]]) {
      const executable = path.join(temporary, profile + (process.platform === 'win32' ? '.exe' : ''));
      const compiled = spawnSync(compiler, ['-std=c++17', '-O2', '-UNDEBUG', ...flags,
        '-I', temporary, '-I', root, path.join(import.meta.dirname, 'easyflash-harness.cpp'), '-o', executable],
      { encoding: 'utf8', windowsHide: true, env });
      assert.equal(compiled.status, 0, `${profile}: ${compiled.error ?? ''}\n${compiled.stdout}\n${compiled.stderr}`);
      const result = spawnSync(executable, fixtures, { encoding: 'utf8', windowsHide: true, env });
      assert.equal(result.status, 0, `${profile}: ${result.error ?? ''}\n${result.stdout}\n${result.stderr}`);
      assert.match(result.stdout, /PASS/); t.diagnostic(result.stdout.trim());
    }
  } finally {
    fs.rmSync(temporary, { recursive: true, force: true });
  }
});
