// SPDX-License-Identifier: MIT
//
// Compiles and runs the extension-loader conformance suite on the host. No
// Teensy, no ARM toolchain and no SD card: every test here compiles the real
// firmware headers and the real module source against packages written by the
// real packager, so the JavaScript writer and the C++ reader are checked
// against each other rather than each against itself.
//
// Hardware acceptance is a separate step and is not implied by a pass here.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { registryFixture } from './lib/fixtures.mjs';
import { VM_BASE, VM_LIMIT } from './lib/hex.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);
const option = (name, fallback) => { const i = args.indexOf(name); return i < 0 ? fallback : args[i + 1]; };
const compiler = option('--cxx', process.env.CXX ?? 'g++');
const output = fs.mkdtempSync(path.join(os.tmpdir(), 'vm-verify-'));
const keep = args.includes('--keep');

function run(exe, argv, label) {
  const result = spawnSync(exe, argv, { cwd: root, encoding: 'utf8', windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
  // A failed assert() aborts, which leaves status null and the signal set --
  // so checking status alone reports an aborted C++ test as a pass.
  if (result.error || result.status || result.signal) {
    process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
    const why = result.error ?? (result.signal ? `signal ${result.signal}` : `exit ${result.status}`);
    throw new Error(`${label} failed (${why})`);
  }
  return (result.stdout ?? '') + (result.stderr ?? '');
}

function native(name, argv = []) {
  const exe = path.join(output, name + (process.platform === 'win32' ? '.exe' : ''));
  run(compiler, ['-std=c++17', '-O2', '-Wall', '-Wextra',
    ...(process.platform === 'win32' ? ['-static'] : []),
    '-I', root, path.join(root, 'vm/tests', name + '.cpp'), '-o', exe], `compiling ${name}`);
  process.stdout.write(run(exe, argv, name));
}

const sandbox = (prefix) => fs.mkdtempSync(path.join(output, prefix));

// The flash slot the extension image is linked into is written down twice: here,
// where the hex is partitioned, and in VMBootImage.h, where the minimal image
// decides whether that slot holds an image it may jump to. Neither side can see
// the other, so compare them.
function checkBootSlot() {
  const header = fs.readFileSync(path.join(root, 'Source/Teensy/MinimalBoot/Common/VMBootImage.h'), 'utf8');
  for (const [name, expected] of [['base', VM_BASE], ['limit', VM_LIMIT]]) {
    const match = header.match(new RegExp(`uint32_t ${name} = (0x[0-9a-fA-F]+)u?;`));
    if (!match) throw new Error(`VMBootImage.h no longer declares ${name}`);
    if (parseInt(match[1], 16) !== expected) {
      throw new Error(`VmBootImage::${name} is ${match[1]}, but hex.mjs partitions the extension slot at 0x${expected.toString(16)}`);
    }
  }
  console.log('PASS: VmBootImage::base/limit match the extension flash partition in tools/lib/hex.mjs');
}

checkBootSlot();

process.stdout.write(run(process.execPath, ['--test', 'tools/lib/extension.test.mjs'], 'package format unit tests'));

// One fixture tree, written by the packager, read by every C++ test below.
const fixture = registryFixture(sandbox('fixture-'));
fs.mkdirSync(path.join(fixture, 'VMS/HELLO/DATA'), { recursive: true });
fs.writeFileSync(path.join(fixture, 'VMS/HELLO/DATA/Sample.hi'), 'content');

native('files_test', [sandbox('files-sandbox-')]);
native('image_test', [path.join(fixture, 'VMS/HELLO/engine.mvm')]);
native('registry_test', [fixture, sandbox('registry-sandbox-')]);
native('launch_test', [fixture, sandbox('launch-sandbox-')]);
native('listing_test');
native('scheduler_test');
native('fail_test');
native('hello_module_test', [sandbox('hello-sandbox-')]);

if (keep) console.log(`Artifacts kept in ${output}`);
else fs.rmSync(output, { recursive: true, force: true });
console.log('PASS: extension loader conformance (package format, file services, image validation, registry, launch routing, listing invalidation, failure reporting, reference module)');
