// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const HARNESS_SOURCE = path.join(import.meta.dirname, 'c128_prg_harness.cpp');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('c++') && !inCi && 'c++ is not on PATH';

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'c128-prg-'));
  harnessPath = path.join(dir, 'harness');
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE],
    { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function run(lines) {
  const ran = spawnSync(harness(), { input: lines.join('\n') + '\n', encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  return ran.stdout.split(/\r?\n/).filter(Boolean);
}

// 10 PRINT, at $1C01: the link points at the end-of-program zeros just past the line's own zero
const ONE_LINE = '01 1c 07 1c 0a 00 99 00 00 00';
const EMPTY = '01 1c 00 00';
const LARGEST = 0xff00 - 0x1c01 + 2;

const classify = (host, prg) => run([`host ${host}`, `prg ${prg}`, 'classify'])[0];
const launch = (host, prg, size) => run([`host ${host}`, `prg ${prg}`, ...(size ? [`size ${size}`] : []), 'launch'])[0];

test('a $1C01 BASIC program runs in C128 mode on a C128 and in C64 mode on a C64', { skip }, () => {
  assert.equal(classify('c128', ONE_LINE), '1');
  assert.equal(classify('c64', ONE_LINE), '0');
});

test('an empty $1C01 program is taken on either machine, so a C64 is told it needs a C128', { skip }, () => {
  assert.equal(classify('c128', EMPTY), '1');
  assert.equal(classify('c64', EMPTY), '1');
});

test('anything else stays a C64 PRG', { skip }, () => {
  assert.equal(classify('c128', '01 08 07 08 0a 00 99 00 00 00'), '0', 'a C64 BASIC program');
  assert.equal(classify('c128', '01 1c a9 00 8d 20 d0 60'), '0', 'machine code at $1C01');
  assert.equal(classify('c128', '01 1c 07 1c 0a 00 99 00'), '0', 'a link to the end of the file, with no end of program');
  assert.equal(classify('c128', '01 1c 07 1c 0a 00 99 22 00 00'), '0', 'a link not just past a line end');
  assert.equal(classify('c128', '01 1c 05 1c 00 00 00 00'), '0', 'a link too short for a line');
  assert.equal(classify('c128', '01 1c'), '0', 'too short to hold a link');
});

test('a program is streamed through the boot ROM, its end patched in', { skip }, () => {
  assert.equal(launch('c128', ONE_LINE),
    'reset=1 boot=1 selects=0 game=0 exrom=0 roml=boot romh=boot end=1c09 xfer=prg:10:0 masks=03ff:00ff msg=Starting C128 mode');
});

test('a program ending at $FF00 still loads, and one byte more is refused before anything changes', { skip }, () => {
  assert.match(launch('c128', ONE_LINE, LARGEST), /^reset=1 .* roml=boot romh=boot end=ff00 /);
  assert.equal(launch('c128', ONE_LINE, LARGEST + 1),
    'reset=0 boot=0 selects=0 game=1 exrom=1 roml=none romh=none end=0000 xfer=none:0:4294967295 masks=1fff:1fff msg=Too big for C128 BASIC');
});

test('Exit to C128 BASIC resets with no ROM and selects the IO handler now', { skip }, () => {
  assert.equal(launch('c128', EMPTY),
    'reset=1 boot=1 selects=1 game=0 exrom=0 roml=none romh=none end=0000 xfer=none:0:4294967295 masks=1fff:1fff msg=Starting C128 mode');
});

test('on a C64, Exit to C128 BASIC is refused before anything changes', { skip }, () => {
  assert.equal(launch('c64', EMPTY),
    'reset=0 boot=0 selects=0 game=1 exrom=1 roml=none romh=none end=0000 xfer=none:0:4294967295 masks=1fff:1fff msg=C128 BASIC needs a C128');
});

const nextHandler = (host, prg, setting) => run([`host ${host}`, `prg ${prg}`, `ioh ${setting}`, 'launch', 'ioh'])[1];

test('a C128-mode launch drops Kernal Replace, whose GAME and EXROM mean nothing in 128 mode', { skip }, () => {
  assert.equal(nextHandler('c128', ONE_LINE, 'kernal'), 'none', 'a program');
  assert.equal(run(['host c128', `prg ${EMPTY}`, 'ioh kernal', 'launch', 'selected'])[1], 'none', 'Exit to C128 BASIC');
  assert.equal(nextHandler('c128', ONE_LINE, 'swiftlink'), 'swiftlink', 'a handler that works in 128 mode');
});

test('a refused C128-mode launch leaves Kernal Replace selected', { skip }, () => {
  assert.equal(nextHandler('c64', EMPTY, 'kernal'), 'kernal');
  assert.equal(run(['host c128', `prg ${ONE_LINE}`, `size ${LARGEST + 1}`, 'ioh kernal', 'launch', 'ioh'])[1], 'kernal');
});
