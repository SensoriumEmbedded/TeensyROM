// SPDX-License-Identifier: MIT
// Argument handling, plus one fixture run that stops before the first compile. Every case
// here is answered before the script spawns a compiler, so each costs a process spawn
// rather than a build.
//
// That extensions are *on* by default for --target tr-plus is carried here by the
// "extensions are on by default" test, which reads the reservation line the script prints
// before any compile. The build workflow's "carries no host" step (.github/workflows/
// build.yml) asserts the same thing end to end, by counting the host packages beside each
// finished hex -- one for the TR+, none for the TR -- but only after a full compile of both
// targets, so that is the slow net and this is the fast one. Common_Defs.h's #error is
// not a net for this at all: it fires only on an extensions build *without*
// Fab04_FullDMACapable, the opposite direction from a default that silently flipped off.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { definesMacro, readSource } from './lib/source-text.mjs';

const script = path.join(path.dirname(fileURLToPath(import.meta.url)), 'build-firmware.mjs');
const repoRoot = path.dirname(path.dirname(script));
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
  // The plain-TR reason is still named, but the message may not send the caller to
  // --target tr-plus: the flag is refused there too, so that would be a second refusal.
  assert.match(result.stderr, /--with-extensions no longer exists/);
  assert.match(result.stderr, /Fab 0\.4 full DMA/);
});

test('opting out of extensions is refused where there were never any to opt out of', () => {
  const result = build('--target', 'tr', '--no-extensions');
  assert.equal(result.status, 1);
  assert.match(result.stderr, /--no-extensions needs --target tr-plus/);
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

// The source of every argument name the script reads, which is also the only thing keeping
// the declared lists honest. Comments are stripped, so the prose that discusses a
// flag cannot stand in for a call site.
const builderSource = readSource(script);
const argumentReads = new Map(
  [...builderSource.matchAll(/\b(option|flag)\('(--[a-z0-9-]+)'/g)].map((m) => [m[2], m[1]]));
// The lists moved into the scanArgs({ options, flags }) call when the scan was shared with
// the other build scripts; the claim they make is unchanged, so this reads the new shape
// rather than being dropped with the old one.
const namesIn = (key) => {
  const literal = builderSource.match(new RegExp(`\\b${key}:\\s*\\[([^\\]]*)\\]`));
  assert.ok(literal, `${key} is no longer a list of literals in the scanArgs call; this test cannot read it`);
  return new Set([...literal[1].matchAll(/'(--[a-z0-9-]+)'/g)].map((m) => m[1]));
};

// The two lists restate, by hand, the name at every option()/flag() call
// site. Drift either way reintroduces the hazard the refusal was added for: a new call site
// missing from the sets makes the script refuse its own argument, and a set entry with no
// call site is a name the script accepts and then ignores -- which is what "silently builds
// the image you did not ask for" looked like before the refusal existed.
test('the known-argument sets name exactly the arguments the script reads', () => {
  assert.ok(argumentReads.size >= 10, `only ${argumentReads.size} argument reads found; the scrape broke`);
  const declared = { option: namesIn("options"), flag: namesIn("flags") };
  for (const [name, kind] of argumentReads) {
    assert.ok(declared[kind].has(name), `${name} is read with ${kind}() but is not in the matching set`);
  }
  for (const kind of ['option', 'flag']) {
    for (const name of declared[kind]) {
      assert.equal(argumentReads.get(name), kind, `${name} is in the ${kind} set but nothing reads it that way`);
    }
  }
});

// The default itself, which no argument check can reach. The reservation line prints before
// the first compile, so a stub SDK is enough to read it: the two files the "installed core
// unchanged" guard hashes, and the tools directory the private copy symlinks. Every build
// step is skipped, so ARDUINO_CLI only has to name a file that exists -- it is never run.
// The core version is read out of the builder rather than written here again: tools/
// check-pins.mjs lists every place that literal lives so a bump cannot leave one behind,
// and a copy in a test file is a place it does not list.
const coreVersion = builderSource.match(/TEENSY_CORE_VERSION = '([\d.]+)'/)?.[1];
assert.ok(coreVersion, 'TEENSY_CORE_VERSION not found in build-firmware.mjs');
const trVersion = readSource(path.join(repoRoot, 'Source/Teensy/MinimalBoot/Common/Common_Defs.h'))
  .match(/#define\s+TRVersion\s+"([^"]+)"/)?.[1];
assert.ok(trVersion, 'TRVersion not found in Common_Defs.h');

function stubSdk() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tr-build-firmware-args-'));
  const core = path.join(dir, `sdk/packages/teensy/hardware/avr/${coreVersion}/cores/teensy4`);
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

    // Fab04FeatureCtl.h's own comment invites uncommenting #define Fab04_Features for a
    // manual Fab 0.4 build, and in that state `--target tr` is a contradiction the builder
    // refuses by design -- so this leg reads the refusal instead of asserting exit 0, or a
    // supported local edit turns into a failure of a test about something else. Either way
    // the claim this leg carries is the same one: a plain TR reserves no extension slot.
    const fab04Active = definesMacro(fs.readFileSync(
      path.join(repoRoot, 'Source/Teensy/MinimalBoot/Common/Fab04FeatureCtl.h'), 'utf8'), 'Fab04_Features');
    const plain = upTo('--target', 'tr');
    if (fab04Active) assert.match(plain.stderr, /Fab04_Features is #define'd/);
    else assert.equal(plain.status, 0, plain.stderr);
    assert.doesNotMatch(plain.stdout, RESERVATION);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// The other half of --no-extensions: the loaderless TR+ is a different image from the
// shipping one, so it gets a different filename. Nothing downstream can tell the two apart
// by content at a glance, which is why sharing a name let one be published as the other.
// The existence check runs before the first compile and names the exact path it would
// write, so it is the cheapest place to read the planned output name.
test('a --no-extensions TR+ writes its own filename, not the shipping one', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const version = fs.readFileSync(
    path.join(repoRoot, 'Source/Teensy/MinimalBoot/Common/Common_Defs.h'), 'utf8')
    .match(/^\s*#define\s+TRVersion\s+"([^"]+)"/m)?.[1];
  assert.ok(version, 'TRVersion not found in Common_Defs.h');
  const dir = stubSdk();
  const out = path.join(dir, 'out');
  fs.mkdirSync(out, { recursive: true });
  // No --skip-combine: that is what makes the script check the output path at all.
  const attempt = () => spawnSync(process.execPath, [script,
    '--target', 'tr-plus', '--no-extensions',
    '--arduino-data', path.join(dir, 'sdk'), '--out', out,
    '--skip-minimal-build', '--skip-teensy-build'],
    { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
  try {
    // A shipping hex already sitting there is not this build's output, so it neither stops
    // the build nor gets overwritten by it.
    fs.writeFileSync(path.join(out, `TeensyROM+_${version}_full.hex`), '');
    assert.doesNotMatch(attempt().stderr, /already exists/);

    fs.writeFileSync(path.join(out, `TeensyROM+_${version}_noext_full.hex`), '');
    assert.match(attempt().stderr,
      new RegExp(`TeensyROM\\+_${version.replace(/\./g, '\\.')}_noext_full\\.hex already exists`));
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
    /--with-extensions no longer exists/);
});

// A --host-sketch build is a host and nothing else. Building the minimal and main images
// around it would produce the stock firmware byte for byte under a name that pairs it with
// someone else's host -- the fork-and-ship shape this build no longer offers. The names are
// read off the refusal, which prints each planned output before anything is copied or
// compiled: the host package in the way is refused by its own name, and a firmware hex
// already sitting there is not this build's to overwrite or to be stopped by.
test('a --host-sketch build plans its host package and no firmware hex', () => {
  const dir = stubSdk();
  const out = path.join(dir, 'out');
  fs.mkdirSync(out, { recursive: true });
  const attempt = () => spawnSync(process.execPath, [script, '--target', 'tr-plus',
    '--arduino-data', path.join(dir, 'sdk'), '--out', out,
    '--host-sketch', path.join(repoRoot, 'Source/Teensy/ExampleHost')],
    { encoding: 'utf8', timeout: 20_000 });
  try {
    fs.writeFileSync(path.join(out, `TeensyROM+_${trVersion}_full.hex`), '');
    fs.writeFileSync(path.join(out, `TeensyROM+_${trVersion}_ExampleHost_full.hex`), '');
    fs.writeFileSync(path.join(out, `TeensyROM+_${trVersion}_ExampleHost.TRH`), '');
    const result = attempt();
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, /TeensyROM\+_.*_ExampleHost\.TRH already exists/);
    assert.doesNotMatch(result.stderr, /_full\.hex already exists/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// The half of that the output names cannot show: that the two firmware images are not
// compiled either. /bin/echo stands in for arduino-cli, so the first image the run starts is
// the last -- its --show-properties answer has no build.flags.defs in it -- and the stub
// core carries the two anchors the USB_DISABLED patches need to get that far.
test('a --host-sketch build compiles the host and neither firmware image', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  const core = path.join(dir, `sdk/packages/teensy/hardware/avr/${coreVersion}/cores/teensy4`);
  fs.writeFileSync(path.join(core, 'yield.cpp'), 'if (Serial.available()) serialEvent();\n');
  fs.writeFileSync(path.join(core, 'startup.c'), '{\n\t\tusb_isr();\n}\n');
  try {
    const result = spawnSync(process.execPath, [script, '--target', 'tr-plus',
      '--arduino-data', path.join(dir, 'sdk'), '--out', path.join(dir, 'out'),
      '--host-sketch', path.join(repoRoot, 'Source/Teensy/ExampleHost')],
      { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stdout, /\[extension\] Building/);
    assert.doesNotMatch(result.stdout, /\[(minimal|main)\] Building/);
    assert.match(result.stderr, /build\.flags\.defs/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// The stock host rides beside the shipping hex rather than inside it, under the name of the
// sketch it was built from. Both outputs are checked before the first compile, so each is
// refused by its own name; a build that skips the host plans no package and is not stopped
// by one, and gets as far as the next refusal instead.
test('a tr-plus build plans the stock host as its own .TRH beside the hex', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  const out = path.join(dir, 'out');
  fs.mkdirSync(out, { recursive: true });
  const attempt = (...args) => spawnSync(process.execPath, [script, '--target', 'tr-plus',
    '--arduino-data', path.join(dir, 'sdk'), '--out', out, ...args],
    { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
  const hex = path.join(out, `TeensyROM+_${trVersion}_full.hex`);
  const host = path.join(out, `TeensyROM+_${trVersion}_VMBoot.TRH`);
  try {
    fs.writeFileSync(hex, '');
    let result = attempt();
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, /TeensyROM\+_.*_full\.hex already exists/);
    fs.rmSync(hex);

    fs.writeFileSync(host, '');
    result = attempt();
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, /TeensyROM\+_.*_VMBoot\.TRH already exists/);

    result = attempt('--skip-extension-build', '--skip-minimal-build');
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, /Combining requires both firmware images/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// Skipping a firmware image without --skip-combine can never produce the hex, and the host
// is packaged after the combine. Refused before the first compile, rather than after minutes
// of compiling a host that is then thrown away; the refusal names the way to get just that.
test('a combine that cannot happen is refused before anything compiles', {
  skip: process.platform === 'win32' && 'needs /bin/echo as a stand-in for arduino-cli',
}, () => {
  const dir = stubSdk();
  try {
    for (const skip of ['--skip-minimal-build', '--skip-teensy-build']) {
      const result = spawnSync(process.execPath, [script, '--target', 'tr-plus',
        '--arduino-data', path.join(dir, 'sdk'), '--out', path.join(dir, 'out'), skip],
        { encoding: 'utf8', timeout: 60_000, env: { ...process.env, ARDUINO_CLI: '/bin/echo' } });
      assert.equal(result.status, 1, result.stdout + result.stderr);
      assert.match(result.stderr, /Combining requires both firmware images/);
      assert.match(result.stderr, /--host-sketch Source\/Teensy\/VMBoot/);
      assert.doesNotMatch(result.stdout, NO_BUILD_STARTED);
    }
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

// The example host is documentation that compiles, so the four contract points it is
// meant to demonstrate are asserted here rather than left to a reader to notice.
test('the example host carries the four things a host owes', () => {
  // Comments blanked, because the file explains the VM_BOOT_EXECUTE_MIN trap in prose and
  // the assertion below is about what the code does, not about what it talks about.
  const ino = readSource(path.join(repoRoot, 'Source/Teensy/ExampleHost/ExampleHost.ino'));
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
