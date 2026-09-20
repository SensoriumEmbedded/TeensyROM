// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  crc32, buildImage, parseImage, buildManifest, buildClientCrt,
  CODE_BASE, DATA_BASE, CLIENT_BYTES, DESCRIPTOR_OFFSET,
  BASE_SERVICES, SERVICE, PROFILE_RAM2_RO96, RAM2_RO_BYTES, CODE_LIMIT,
} from './extension.mjs';

const thumbReturn = Buffer.from([0x70, 0x47]);  // bx lr
const image = (overrides = {}) => buildImage({ code: thumbReturn, entry: CODE_BASE | 1, ...overrides });

test('crc32 matches the reflected 0xedb88320 vector firmware computes', () => {
  assert.equal(crc32(Buffer.from('123456789')), 0xcbf43926);
  assert.equal(crc32(Buffer.alloc(0)), 0);
});

test('an image round trips through its own parser', () => {
  const data = Buffer.from([1, 2, 3, 4]);
  const header = parseImage(image({ data, bssBytes: 64 }));
  assert.equal(header.codeBytes, 2);
  assert.equal(header.dataBytes, 4);
  assert.equal(header.bssBytes, 64);
  assert.equal(header.entry, CODE_BASE | 1);
  assert.equal(header.codeBase, CODE_BASE);
  assert.equal(header.ramBase, DATA_BASE);
  assert.equal(header.requiredServices, BASE_SERVICES);
  assert.equal(header.profile, 0);
});

test('a corrupted payload or header is rejected, not silently loaded', () => {
  const built = image({ data: Buffer.from([9, 9, 9, 9]) });
  const payloadDamage = Buffer.from(built);
  payloadDamage[64 + 3] ^= 0xff;
  assert.throws(() => parseImage(payloadDamage), /payload CRC/);
  const headerDamage = Buffer.from(built);
  headerDamage.writeUInt32LE(headerDamage.readUInt32LE(20) + 1, 20);
  assert.throws(() => parseImage(headerDamage), /header CRC/);
  assert.throws(() => parseImage(built.subarray(0, built.length - 1)), /bytes, header describes/);
});

test('entry points the loader would refuse are refused at build time', () => {
  assert.throws(() => image({ entry: CODE_BASE }), /Thumb bit/);
  assert.throws(() => image({ entry: (CODE_BASE + 0x100) | 1 }), /outside the module code window/);
  assert.throws(() => image({ entry: (CODE_BASE - 2) | 1 }), /outside the module code window/);
});

test('an image cannot ask for more than the base profile provides', () => {
  assert.throws(() => image({ requiredServices: BASE_SERVICES | 32 }), /base profile does not provide/);
  assert.throws(() => image({ requiredServices: BASE_SERVICES | 512 }), /base profile does not provide/);
});

test('the RAM2 read-only profile and the legacy profile stay consistent', () => {
  const readOnly = Buffer.alloc(1024, 0xab);
  const header = parseImage(image({
    profile: PROFILE_RAM2_RO96, readOnly, requiredServices: BASE_SERVICES | SERVICE.RAM2_RO,
  }));
  assert.equal(header.profile, PROFILE_RAM2_RO96);
  assert.equal(header.readOnlyBytes, 1024);
  assert.throws(() => image({ profile: PROFILE_RAM2_RO96, readOnly }), /must require VM_SERVICE_RAM2_RO/);
  assert.throws(() => image({ readOnly }), /Profile 0 stores no RAM2 constants/);
  assert.throws(() => image({
    profile: PROFILE_RAM2_RO96, readOnly: Buffer.alloc(RAM2_RO_BYTES + 1), requiredServices: BASE_SERVICES | SERVICE.RAM2_RO,
  }), /1\.\.96 KiB/);
});

test('code larger than the module window is refused with its measurement', () => {
  assert.throws(() => image({ code: Buffer.alloc(CODE_LIMIT - CODE_BASE + 1) }), /window is 98304/);
});

test('a manifest is six strict lines', () => {
  assert.equal(buildManifest({ id: 'HELLO', extensions: 'hi' }), 'VM1\nHELLO\nhi\nengine.mvm\nclient.crt\nEND\n');
  assert.equal(buildManifest({ id: 'GBVM', extensions: ['gb', 'gbc'] }).split('\n')[2], 'gb,gbc');
});

test('a manifest cannot claim an extension the stock menu owns', () => {
  for (const extension of ['prg', 'CRT', 'd64', 'sid']) {
    assert.throws(() => buildManifest({ id: 'X', extensions: extension }), /belongs to the stock menu/);
  }
  assert.throws(() => buildManifest({ id: 'X', extensions: 'toolong' + 'x' }), /must be 1\.\.7 characters/);
  assert.throws(() => buildManifest({ id: 'X', extensions: 'a.b' }), /Invalid extension/);
  assert.throws(() => buildManifest({ id: '../escape', extensions: 'hi' }), /Invalid id/);
  assert.throws(() => buildManifest({ id: 'X'.repeat(24), extensions: 'hi' }), /shorter than 24/);
});

test('a client cartridge is byte-exact where the firmware looks for things', () => {
  const bank0 = Buffer.alloc(8192, 0x11), bank1 = Buffer.alloc(8192, 0x22);
  const crt = buildClientCrt({ id: 'HELLO', bank0, bank1 });
  assert.equal(crt.length, CLIENT_BYTES);
  assert.equal(crt.subarray(0, 16).toString('latin1'), 'C64 CARTRIDGE   ');
  assert.equal(crt[23], 32, 'cartridge type 32 (EasyFlash)');

  // VMHostBoot walks the three CHIPs by these exact field values.
  const chipAt = (offset, bank, address) => {
    assert.equal(crt.subarray(offset, offset + 4).toString('latin1'), 'CHIP');
    assert.equal(crt.readUInt16BE(offset + 10), bank);
    assert.equal(crt.readUInt16BE(offset + 12), address);
    assert.equal(crt.readUInt16BE(offset + 14), 8192);
  };
  chipAt(64, 0, 0x8000);
  chipAt(64 + 8208, 0, 0xa000);
  chipAt(64 + 2 * 8208, 1, 0x8000);

  // preflight() CRCs the two banks straight out of the file at these offsets.
  assert.deepEqual(crt.subarray(80, 80 + 8192), bank0);
  assert.deepEqual(crt.subarray(8288, 8288 + 8192), bank1);

  const descriptor = crt.subarray(DESCRIPTOR_OFFSET, DESCRIPTOR_OFFSET + 128);
  assert.equal(descriptor.subarray(0, 4).toString('latin1'), 'VMH1');
  assert.equal(descriptor[4], 2, 'ABI');
  assert.equal(descriptor.subarray(16, 21).toString('latin1'), 'HELLO');
  assert.equal(descriptor[16 + 5], 0, 'package id stays NUL terminated');
  assert.equal(descriptor.readUInt32LE(8), crc32(Buffer.concat([bank0, bank1])));
  assert.equal(descriptor.readUInt32LE(124), crc32(descriptor.subarray(0, 124)));
});

test('short client banks are padded rather than shifting the descriptor', () => {
  const crt = buildClientCrt({ id: 'HELLO', bank0: Buffer.from([1, 2, 3]), bank1: Buffer.alloc(0) });
  assert.equal(crt.length, CLIENT_BYTES);
  assert.equal(crt.subarray(DESCRIPTOR_OFFSET, DESCRIPTOR_OFFSET + 4).toString('latin1'), 'VMH1');
  assert.equal(crt[83], 0, 'unused bank space is zero filled');
});
