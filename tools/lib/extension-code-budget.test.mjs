// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {extensionLinkerScript, HOST_CODE_KIB} from './extension-image.mjs';
import {CODE_BASE} from './extension.mjs';

const linkers = path.resolve(import.meta.dirname, '../BootLinkerFiles');
const progmemBlock = (ld) => ld.slice(ld.indexOf('.text.progmem : {'), ld.indexOf('.text.itcm : {'));

test('the stock host is linked at 64 KiB, and 96 KiB moves only its code ceiling', () => {
  const stock = extensionLinkerScript(linkers);
  assert.equal(stock, extensionLinkerScript(linkers, 64));
  assert.match(stock, /ASSERT\(_etext <= 0x10000,/);
  assert.equal(extensionLinkerScript(linkers, 96), stock.replace(
    '_etext <= 0x10000, "Host code exceeds its 64 KiB', '_etext <= 0x18000, "Host code exceeds its 96 KiB'));
  assert.match(stock, /_itcm_block_count = 6;/);
  for (const invalid of [0, 32, 65, 128, '64', NaN]) {
    assert.throws(() => extensionLinkerScript(linkers, invalid), /64 or 96/);
  }
});

test('no host code budget reaches into the module code window', () => {
  for (const kib of HOST_CODE_KIB) assert.ok(kib * 1024 <= CODE_BASE, `${kib} KiB`);
});

// ld gives an input section to the first output section that names it, so a library
// pattern outside .text.progmem, or after it, leaves that code in ITCM.
test('the SD card libraries run from flash, matched by their library directory', () => {
  const progmem = progmemBlock(extensionLinkerScript(linkers));
  for (const library of ['SdFat', 'SD', 'SPI']) {
    assert.ok(progmem.includes(`*/libraries/${library}/*(.text*)`), library);
  }
});
