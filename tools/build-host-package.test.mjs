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

// A combined firmware hex carries the extension image at VM_BASE among everything else,
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

test('the extension image is lifted back out of a combined firmware hex', () => {
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

test('a firmware hex with no extension slot is refused by name', () => {
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

test('exactly one input is required', () => {
  assert.match(run().stderr, /exactly one of --hex .* or --image/);
  assert.match(run('--hex', 'a.hex', '--image', 'b.bin').stderr, /exactly one of --hex .* or --image/);
});
