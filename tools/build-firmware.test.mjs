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

// The extension image is built last, so a host sketch checked where it is used is checked
// only after the minimal and main images have compiled -- minutes, for a typo. These runs
// therefore pass no --skip-*-build: the assertion is the refusal *and* that no compile was
// started, which is what fails if the checks drift back down to the overlay.
const NO_BUILD_STARTED = /\[(minimal|main|extension)\] Building/;

test('every way a host sketch can be wrong is refused before an image is compiled', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  const sketch = path.join(dir, 'sketch');
  fs.mkdirSync(sketch, { recursive: true });
  const attempt = (target = sketch) => spawnSync(process.execPath, [script,
    '--target', 'tr-plus', '--arduino-data', path.join(dir, 'sdk'), '--out', path.join(dir, 'out'),
    '--host-sketch', target],
    { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
  const refused = (result, pattern) => {
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, pattern);
    assert.doesNotMatch(result.stdout, NO_BUILD_STARTED);
  };
  try {
    // Arduino takes the sketch directory name from its entry point, so there must be one.
    refused(attempt(), /must hold exactly one \.ino .*found 0/);
    fs.writeFileSync(path.join(sketch, 'One.ino'), '');
    fs.writeFileSync(path.join(sketch, 'Two.ino'), '');
    refused(attempt(), /must hold exactly one \.ino .*found 2/);
    fs.rmSync(path.join(sketch, 'Two.ino'));

    // A path that exists but is not a directory used to reach readdirSync and come back as
    // an ENOTDIR stack trace, which is a crash report rather than a refusal.
    refused(attempt(path.join(sketch, 'One.ino')), /Host sketch is not a directory/);

    // A subdirectory is refused, not skipped. The overlay copies files and does not
    // descend, so a src/ passed over silently builds a host without the caller's code --
    // and where the subdirectory shadows one of MinimalBoot's own, builds cleanly.
    fs.mkdirSync(path.join(sketch, 'src'));
    fs.writeFileSync(path.join(sketch, 'src/lib.cpp'), '');
    refused(attempt(), /may hold only files: the overlay does not descend into src/);
    fs.rmSync(path.join(sketch, 'src'), { recursive: true });

    // A symlink to a directory is the same hole wearing a different hat; a dangling one
    // cannot be copied at all. Both land in the same refusal rather than in a stat throw.
    const elsewhere = path.join(dir, 'elsewhere');
    fs.mkdirSync(elsewhere);
    fs.symlinkSync(elsewhere, path.join(sketch, 'linked'));
    refused(attempt(), /may hold only files: the overlay does not descend into linked/);
    fs.rmSync(path.join(sketch, 'linked'));
    fs.symlinkSync(path.join(dir, 'gone'), path.join(sketch, 'dangling'));
    refused(attempt(), /may hold only files: the overlay does not descend into dangling/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// `npm run <script> -- --host-sketch <mine>` appends the caller's argument after the one
// package.json already passes, and reading an option with indexOf() took the first. So the
// override lost silently and build:example-host built Source/Teensy/ExampleHost under the
// caller's name -- the stock-host-under-your-name failure the refusals above exist for,
// arriving through the documented way to run the build.
test('an option given twice is refused rather than resolved to one of them', () => {
  const twice = build('--target', 'tr-plus',
    '--host-sketch', 'Source/Teensy/ExampleHost', '--host-sketch', 'Source/Teensy/MyHost');
  assert.equal(twice.status, 1);
  assert.match(twice.stderr, /--host-sketch given more than once/);
  // Named for the shape that produces it, because that is where a caller meets it.
  assert.match(twice.stderr, /npm run/);
  // Not special to --host-sketch: the same first-wins read served every option here.
  assert.match(build('--target', 'tr-plus', '--target', 'tr').stderr,
    /--target given more than once/);
  assert.match(build('--target', 'tr-plus', '--out', 'a', '--out', 'b').stderr,
    /--out given more than once/);
  // A repeated *flag* carries no value to lose, so it is left idempotent and still reaches
  // the refusal it was always going to reach, rather than being caught by the check above.
  assert.match(build('--target', 'tr', '--with-extensions', '--with-extensions').stderr,
    /--with-extensions needs --target tr-plus/);
});

// A --host-sketch build is not the shipping image -- the slot holds a program this repo
// did not write -- and under the shipping name the two are one `ls` apart: an
// `npm run build:example-host` would leave an LED blinker where the release hex goes, and
// the --hex a host author hands to build-host-package.mjs would name the wrong thing. The
// name is read off the refusal, which prints finalOutput before anything is copied or
// compiled; the assertion is that the file the run claims is in its way is the marked one,
// which a reverted stem could not produce.
test('a --host-sketch build is named for its host, not for the shipping image', () => {
  const repo = path.dirname(path.dirname(script));
  const version = readSource(path.join(repo, 'Source/Teensy/MinimalBoot/Common/Common_Defs.h'))
    .match(/#define\s+TRVersion\s+"([^"]+)"/)[1];
  const dir = stubSdk();
  const out = path.join(dir, 'out');
  fs.mkdirSync(out, { recursive: true });
  try {
    fs.writeFileSync(path.join(out, `TeensyROM+_${version}_ExampleHost_full.hex`), '');
    const result = spawnSync(process.execPath, [script, '--target', 'tr-plus',
      '--arduino-data', path.join(dir, 'sdk'), '--out', out,
      '--host-sketch', path.join(repo, 'Source/Teensy/ExampleHost')],
      { encoding: 'utf8', timeout: 20_000 });
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, /TeensyROM\+_.*_ExampleHost_full\.hex already exists/);
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
