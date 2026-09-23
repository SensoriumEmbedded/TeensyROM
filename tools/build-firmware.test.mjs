// SPDX-License-Identifier: MIT
// Argument handling, plus one fixture run that stops before the first compile. Every case
// here is answered before the script spawns a compiler, so each costs a process spawn
// rather than a build.
//
// That extensions are *on* by default for --target tr-plus is carried by the last test in
// this file, which reads the reservation line the script prints before any compile. Nothing
// else carries it: the build workflow compiles both targets but asserts nothing about
// whether the tr-plus hex contains the loader, and Common_Defs.h's #error fires only on an
// extensions build *without* Fab04_FullDMACapable -- the opposite direction from a default
// that silently flipped off.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { readSource } from './lib/source-text.mjs';

const script = path.join(path.dirname(fileURLToPath(import.meta.url)), 'build-firmware.mjs');
// The timeout bounds how long a lost refusal takes to fail, not whether it fails.
// spawnSync leaves status null on a kill, so assert.notEqual(status, 0) alone would not
// catch it -- assert.match(stderr, ...) is what does, and nothing before it can make that
// regex match text a removed refusal never printed. Without the timeout, a build that
// stopped refusing would run for minutes (confirmed to at least 90s: arduino-cli download,
// then a real, successful minimal-image compile) before this test failed instead of after
// a process spawn -- a slow failure, not a slow pass.
const build = (...args) =>
  spawnSync(process.execPath, [script, ...args], { encoding: 'utf8', timeout: 20_000 });

test('extensions are refused on a target whose DMA cannot blank the screen', () => {
  const result = build('--target', 'tr', '--with-extensions');
  assert.equal(result.status, 1);
  assert.match(result.stderr, /--with-extensions needs --target tr-plus/);
});

test('a missing or unknown target is refused', () => {
  assert.match(build().stderr, /Use --target tr or --target tr-plus/);
  assert.match(build('--target', 'tr-minus').stderr, /Use --target tr or --target tr-plus/);
});

test('a misspelled opt-out is refused rather than silently shipping the loader', () => {
  // Extensions are on by default for tr-plus, so an argument this script merely ignores
  // produces the opposite image from the one asked for, and exits 0 saying so.
  for (const typo of ['--no-extension', '--noextensions', '--no_extensions', '-no-extensions']) {
    const result = build('--target', 'tr-plus', typo);
    assert.equal(result.status, 1, `${typo} was accepted`);
    assert.ok(result.stderr.includes(`Unknown argument ${typo}`), `${typo}: ${result.stderr}`);
  }
});

test('--with-extensions is refused on every target, not ignored on the one it matched', () => {
  const plus = build('--target', 'tr-plus', '--with-extensions');
  assert.equal(plus.status, 1);
  assert.match(plus.stderr, /--with-extensions no longer exists/);
  // Contradicting flags refuse rather than letting one of them quietly win.
  assert.match(build('--target', 'tr-plus', '--with-extensions', '--no-extensions').stderr,
    /--with-extensions no longer exists/);
});

test('the equals form of an option is refused by name', () => {
  assert.match(build('--target=tr-plus').stderr, /Unknown argument --target=tr-plus/);
});

// The default itself, which no argument check can reach. The reservation line prints before
// the first compile, so a stub SDK is enough to read it: the two files the "installed core
// unchanged" guard hashes, and the tools directory the private copy symlinks. Every build
// step is skipped, so ARDUINO_CLI only has to name a file that exists -- it is never run.
function stubSdk() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tr-build-firmware-args-'));
  const core = path.join(dir, 'sdk/packages/teensy/hardware/avr/1.61.0/cores/teensy4');
  fs.mkdirSync(core, { recursive: true });
  fs.mkdirSync(path.join(dir, 'sdk/packages/teensy/tools'), { recursive: true });
  for (const stub of ['bootdata.c', 'imxrt1062_t41.ld']) fs.writeFileSync(path.join(core, stub), '');
  return dir;
}

const RESERVATION = /Reserving \d+K at 0x60280000/;

test('extensions are on by default for tr-plus, and only there', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  const upTo = (...args) => spawnSync(process.execPath, [script,
    '--arduino-data', path.join(dir, 'sdk'), '--out', path.join(dir, 'out'),
    '--skip-minimal-build', '--skip-teensy-build', '--skip-extension-build', '--skip-combine',
    ...args], { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
  try {
    const plus = upTo('--target', 'tr-plus');
    assert.equal(plus.status, 0, plus.stderr);
    assert.match(plus.stdout, RESERVATION);

    const plusOptedOut = upTo('--target', 'tr-plus', '--no-extensions');
    assert.equal(plusOptedOut.status, 0, plusOptedOut.stderr);
    assert.doesNotMatch(plusOptedOut.stdout, RESERVATION);

    const plain = upTo('--target', 'tr');
    assert.equal(plain.status, 0, plain.stderr);
    assert.doesNotMatch(plain.stdout, RESERVATION);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// --host-sketch: the seam a third-party host is built through. Every case below is a
// refusal rather than a silent fallback, because the fallback is this repo's own host --
// a build that ignored the flag would ship the stock VM host under someone else's name
// and pass every other check in this file.
test('--host-sketch is refused where there is no extension slot to build into', () => {
  assert.match(build('--target', 'tr', '--host-sketch', 'Source/Teensy/ExampleHost').stderr,
    /--host-sketch needs --target tr-plus/);
  assert.match(build('--target', 'tr-plus', '--no-extensions',
    '--host-sketch', 'Source/Teensy/ExampleHost').stderr,
    /--host-sketch has nothing to build with --no-extensions/);
  assert.match(build('--target', 'tr-plus', '--skip-extension-build',
    '--host-sketch', 'Source/Teensy/ExampleHost').stderr,
    /--host-sketch and --skip-extension-build contradict each other/);
});

test('a host sketch directory that is not there is refused before anything is built', () => {
  const result = build('--target', 'tr-plus', '--host-sketch', 'Source/Teensy/NoSuchHost');
  assert.equal(result.status, 1);
  assert.match(result.stderr, /Host sketch directory not found:.*NoSuchHost/);
});

test('a host sketch names its entry point by holding exactly one .ino', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  const sketch = path.join(dir, 'sketch');
  fs.mkdirSync(sketch, { recursive: true });
  const attempt = () => spawnSync(process.execPath, [script,
    '--target', 'tr-plus', '--arduino-data', path.join(dir, 'sdk'), '--out', path.join(dir, 'out'),
    '--skip-minimal-build', '--skip-teensy-build', '--skip-combine', '--host-sketch', sketch],
    { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
  try {
    assert.match(attempt().stderr, /must hold exactly one \.ino .*found 0/);
    fs.writeFileSync(path.join(sketch, 'One.ino'), '');
    fs.writeFileSync(path.join(sketch, 'Two.ino'), '');
    assert.match(attempt().stderr, /must hold exactly one \.ino .*found 2/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// The example host is documentation that compiles, so the four contract points it is
// meant to demonstrate are asserted here rather than left to a reader to notice.
test('the example host carries the four things a host owes', () => {
  const repo = path.dirname(path.dirname(script));
  // Comments blanked, because the file explains the VM_BOOT_EXECUTE_MIN trap in prose and
  // the assertion below is about what the code does, not about what it talks about.
  const ino = readSource(path.join(repo, 'Source/Teensy/ExampleHost/ExampleHost.ino'));
  // 1. The descriptor, read out of flash by the main image without booting the host.
  assert.match(ino, /section\("\.vmhostid"\)/);
  assert.match(ino, /VmHostId vmHostId = \{ VM_HOSTID_MAGIC, VM_ABI/);
  // 2. The marker, which is what authorizes a run. Testing the boot indicator instead is
  // the trap: minimal has already replaced VM_BOOT_EXECUTE_MIN by the time a host sees it,
  // so that test never passes, every launch falls through to the main app, and from the
  // C64 that is indistinguishable from a host that failed.
  assert.match(ino, /strcmp\(marker, VM_HOST_MARKER\)/);
  assert.doesNotMatch(ino, /VM_BOOT_EXECUTE_MIN/);
  // 3. The record the main image collects on the way back up, and 4. the boot indicator
  // corrected before the reset that reads it.
  assert.match(ino, /VmFail::set\(code, detail\)/);
  assert.match(ino, /EEPROM\.write\(VM_EEP_BOOTIND_ADDR, VM_BOOT_FROM_MIN\)/);
});
