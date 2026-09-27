// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {extensionLinkerScript} from './extension-image.mjs';
import {buildImage, parseImage, HOST_SERVICES} from './extension.mjs';

const linkers = path.resolve(import.meta.dirname, '../BootLinkerFiles');
test('64 KiB host gives the package 128 KiB without moving DTCM, stack or flash', () => {
  const normal = extensionLinkerScript(linkers);
  assert.equal(normal, extensionLinkerScript(linkers, 96));
  const small = extensionLinkerScript(linkers, 64);
  assert.equal(small, normal.replace('_etext <= 0x18000', '_etext <= 0x10000'));
  assert.match(small, /_itcm_block_count = 6;/);
  for (const invalid of [0, 32, 65, 128, '64', NaN]) {
    assert.throws(() => extensionLinkerScript(linkers, invalid), /64 or 96/);
  }
});
test('MPE transfer and CODE128 bits can identify registration without being stock services', () => {
  const requiredServices = 0x30001f;
  const image = buildImage({code: Buffer.from([0x70, 0x47, 0, 0]), entry: 0x18001, requiredServices});
  assert.equal(parseImage(image).requiredServices, requiredServices);
  assert.equal(HOST_SERVICES & 0x300000, 0);
});
