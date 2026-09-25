// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { encodeHex, VM_BASE } from './lib/hex.mjs';
import { parseHostPackage, hostDescriptor, HOST_PACKAGE_HEADER_BYTES } from './lib/extension.mjs';
import { hostImage } from './lib/fixtures.mjs';
import { hostImageFromHex } from './build-host-package.mjs';

const script = path.join(path.dirname(fileURLToPath(import.meta.url)), 'build-host-package.mjs');
const run = (...args) =>
  spawnSync(process.execPath, [script, ...args], { encoding: 'utf8', timeout: 60_000 });

// A hex handed to --hex may hold bytes outside the slot as well as the image at VM_BASE,
// so the fixture puts something below it too: lifting the slot out has to ignore that.
function firmwareHex(image, { at = VM_BASE, alsoAtBase = true, gap = null } = {}) {
  const bytes = new Map();
  if (alsoAtBase) for (let i = 0; i < 16; i++) bytes.set(0x60000000 + i, 0x11);
  for (let i = 0; i < image.length; i++) {
    if (gap && i >= gap.from && i < gap.to) continue;
    bytes.set(at + i, image[i]);
  }
  return encodeHex(bytes);
}

const sandbox = () => fs.mkdtempSync(path.join(os.tmpdir(), 'trh-'));

test('the extension image is lifted out of a hex that holds more than the slot', () => {
  const image = hostImage();
  assert.deepEqual(hostImageFromHex(firmwareHex(image)), image);
});

test('a hole in the slot reads as the 0xFF an erased part holds', () => {
  const image = hostImage();
  const gap = { from: 0x3000, to: 0x3040 };
  const holed = hostImageFromHex(firmwareHex(image, { gap }));

  // The gap is filled, not closed up: every byte after it keeps the offset the linker
  // gave it, which is the whole reason the boot words and descriptor still parse.
  assert.equal(holed.length, image.length);
  assert.equal(holed.subarray(0, gap.from).compare(image.subarray(0, gap.from)), 0);
  assert.equal(holed.subarray(gap.to).compare(image.subarray(gap.to)), 0);
  assert.equal(holed.subarray(gap.from, gap.to).compare(Buffer.alloc(gap.to - gap.from, 0xff)), 0);
});

test('a hex with nothing in the extension slot, which is every firmware hex, is refused by name', () => {
  const bytes = new Map();
  for (let i = 0; i < 16; i++) bytes.set(0x60000000 + i, 0x11);
  assert.throws(() => hostImageFromHex(encodeHex(bytes)), /nothing in the extension slot/);
});

test('a raw host image packages into a .TRH the device reader accepts', () => {
  const dir = sandbox();
  const imagePath = path.join(dir, 'host.bin');
  fs.writeFileSync(imagePath, hostImage({ name: 'Example' }));
  const out = path.join(dir, 'EXAMPLE.TRH');

  const result = run('--image', imagePath, '--out', out);
  assert.equal(result.status, 0, result.stderr);

  const pkg = fs.readFileSync(out);
  const header = parseHostPackage(pkg);
  assert.equal(header.payloadBytes, 0x8000);
  assert.equal(hostDescriptor(pkg.subarray(HOST_PACKAGE_HEADER_BYTES)).name, 'Example');
});

test('a host image the minimal loader would not enter is refused before it is written', () => {
  const dir = sandbox();
  const imagePath = path.join(dir, 'bad.bin');
  fs.writeFileSync(imagePath, hostImage({ flashMagic: 0 }));
  const out = path.join(dir, 'BAD.TRH');

  const result = run('--image', imagePath, '--out', out);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /build-host-package: /);
  assert.equal(fs.existsSync(out), false, 'a refused package must not be written');
});

// The descriptor's twelve bytes are third-party. Without --out the packager names the
// file after them, so they have to be treated as bytes rather than as a path fragment:
// "../../pwned" is eleven of the twelve, and put the .TRH two directories above the one
// the developer was looking in, on top of whatever was already there. The developer then
// copies the file they expected -- stale or absent -- to the card.
test('a descriptor name cannot steer the output path out of the source directory', () => {
  const dir = sandbox();
  const nested = path.join(dir, 'build', 'run-x');
  fs.mkdirSync(nested, { recursive: true });
  const imagePath = path.join(nested, 'host.bin');
  fs.writeFileSync(imagePath, hostImage({ name: '../../pwned' }));

  const result = run('--image', imagePath);
  assert.equal(result.status, 0, result.stderr);

  // Written where it belongs, under the name the bytes reduce to, and nothing escaped.
  assert.equal(fs.existsSync(path.join(nested, 'PWNED.TRH')), true);
  assert.equal(fs.existsSync(path.join(dir, 'PWNED.TRH')), false);
  assert.equal(fs.existsSync(path.join(dir, 'build', 'PWNED.TRH')), false);

  // And it says so, rather than handing back a differently-named file in silence.
  assert.match(result.stdout, /is not a filename; writing PWNED\.TRH/);
});

test('separators, dots and control bytes do not survive into the filename', () => {
  const cases = [
    ['a/b/c', 'ABC.TRH'],
    ['..', 'HOST.TRH'],
    ['.hidden', 'HIDDEN.TRH'],
    ['My Host!', 'MYHOST.TRH'],
    // Only the ESC and the bracket go: "31m" are ordinary filename characters and stay,
    // which is the point -- the rule drops what cannot be a filename, not what looks odd.
    ['\x1b[31mred', '31MRED.TRH'],
    ['///', 'HOST.TRH'],             // nothing survives: falls back rather than writing ".TRH"
  ];
  for (const [name, expected] of cases) {
    const dir = sandbox();
    const imagePath = path.join(dir, 'host.bin');
    fs.writeFileSync(imagePath, hostImage({ name }));
    const result = run('--image', imagePath);
    assert.equal(result.status, 0, `${name}: ${result.stderr}`);
    assert.equal(fs.existsSync(path.join(dir, expected)), true, `${name} -> ${expected}`);
    // Every file written sits directly in the source directory, whatever the name said.
    assert.deepEqual(fs.readdirSync(dir).filter((f) => f.endsWith('.TRH')), [expected]);
  }
});

test('the printed descriptor name carries no escape sequence to the terminal', () => {
  const dir = sandbox();
  const imagePath = path.join(dir, 'host.bin');
  fs.writeFileSync(imagePath, hostImage({ name: 'A\x1b[2JB' }));

  const result = run('--image', imagePath);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /Host "A\?\[2JB"/);
  assert.equal(/[\u0000-\u001f\u007f-\u009f]/.test(result.stdout.replace(/\n/g, '')), false);
});

test('an explicit --out is the developer\'s own choice and is not second-guessed', () => {
  // CI passes --out, and gating it on the descriptor would break that. Only the name the
  // packager derives for itself is untrusted.
  const dir = sandbox();
  const imagePath = path.join(dir, 'host.bin');
  fs.writeFileSync(imagePath, hostImage({ name: '../../pwned' }));
  const out = path.join(dir, 'chosen', 'ANYWHERE.TRH');
  fs.mkdirSync(path.dirname(out), { recursive: true });

  const result = run('--image', imagePath, '--out', out);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(fs.existsSync(out), true);
});

test('exactly one input is required', () => {
  assert.match(run().stderr, /exactly one of --hex .* or --image/);
  assert.match(run('--hex', 'a.hex', '--image', 'b.bin').stderr, /exactly one of --hex .* or --image/);
});
