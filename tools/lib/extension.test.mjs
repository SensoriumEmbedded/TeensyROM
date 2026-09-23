// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  crc32, buildImage, parseImage, buildManifest, buildClientCrt,
  CODE_BASE, DATA_BASE, CLIENT_BYTES, DESCRIPTOR_OFFSET,
  BASE_SERVICES, SERVICE, PROFILE_RAM2_RO, RAM2_RO_BYTES, CODE_LIMIT,
  ASSIGNED_SERVICES, HOST_SERVICES, UNASSIGNED_SERVICES, SERVICE_EXAMPLE,
  buildHostPackage, parseHostPackage, hostSlotValid,
  HOST_ID_OFFSET, HOSTID_MAGIC, HOST_SLOT_BYTES, HOST_PACKAGE_HEADER_BYTES, ABI,
} from './extension.mjs';
import { VM_BASE } from './hex.mjs';

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

test('a header the loader would refuse is refused by the parser, not just a bad CRC', () => {
  // Corrupt a field and re-stamp the header CRC, so the damage reaches the
  // structural checks instead of stopping at the checksum.
  const restamped = (word, value) => {
    const b = Buffer.from(image());
    b.writeUInt32LE(value >>> 0, word * 4);
    b.writeUInt32LE(0, 44);
    b.writeUInt32LE(crc32(b.subarray(0, 64)), 44);
    return b;
  };
  assert.throws(() => parseImage(restamped(14, 1)), /Reserved header words/);
  assert.throws(() => parseImage(restamped(6, CODE_BASE)), /outside the module code window/);
  assert.equal(parseImage(restamped(9, BASE_SERVICES | 0x10000)).requiredServices, BASE_SERVICES | 0x10000);
  assert.throws(() => parseImage(restamped(12, 2)), /Unknown memory profile 2/);
  assert.throws(() => parseImage(restamped(12, PROFILE_RAM2_RO)), /Profile 1 needs/);
});

test('entry points the loader would refuse are refused at build time', () => {
  assert.throws(() => image({ entry: CODE_BASE }), /Thumb bit/);
  assert.throws(() => image({ entry: (CODE_BASE + 0x100) | 1 }), /outside the module code window/);
  assert.throws(() => image({ entry: (CODE_BASE - 2) | 1 }), /outside the module code window/);
});

test('a service assigned to another host packages and round trips through the parser', () => {
  for (const bit of [32, 512, 0x10000]) {
    assert.equal(parseImage(image({ requiredServices: BASE_SERVICES | bit })).requiredServices,
                 BASE_SERVICES | bit);
  }
  // The static_assert in VMABI.h, mirrored: no bit is both served and assigned.
  assert.equal(HOST_SERVICES & ASSIGNED_SERVICES, 0);
});

test('the mask --services offers as an example is one a module could really ship', () => {
  const source = fs.readFileSync(new URL('../build-extension.mjs', import.meta.url), 'utf8');
  const example = source.match(/--services wants one 32-bit mask such as (0x[0-9a-fA-F]+)/);
  assert.ok(example, 'build-extension.mjs no longer offers an example mask');
  assert.equal(Number(example[1]), BASE_SERVICES | SERVICE_EXAMPLE);
});

test('an unassigned service bit is refused at build time, and only there', () => {
  const unclaimed = 1 << 20;
  assert.equal(unclaimed & UNASSIGNED_SERVICES, unclaimed);
  assert.throws(() => image({ requiredServices: BASE_SERVICES | unclaimed }), /unassigned services 0x100000/);
  const forced = image({ requiredServices: BASE_SERVICES | unclaimed, allowUnassignedServices: true });
  assert.equal(parseImage(forced).requiredServices, BASE_SERVICES | unclaimed);
});

test('the RAM2 read-only profile and the legacy profile stay consistent', () => {
  const readOnly = Buffer.alloc(1024, 0xab);
  const header = parseImage(image({
    profile: PROFILE_RAM2_RO, readOnly, requiredServices: BASE_SERVICES | SERVICE.RAM2_RO,
  }));
  assert.equal(header.profile, PROFILE_RAM2_RO);
  assert.equal(header.readOnlyBytes, 1024);
  assert.throws(() => image({ profile: PROFILE_RAM2_RO, readOnly }), /must require VM_SERVICE_RAM2_RO/);
  assert.throws(() => image({ readOnly }), /Profile 0 stores no RAM2 constants/);
  assert.throws(() => image({
    profile: PROFILE_RAM2_RO, readOnly: Buffer.alloc(RAM2_RO_BYTES + 1), requiredServices: BASE_SERVICES | SERVICE.RAM2_RO,
  }), /1\.\.80 KiB/);
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
  assert.throws(() => buildManifest({ id: 'HI', extensions: 'hi', module: 'm'.repeat(32) }), /shorter than 32/);
  assert.throws(() => buildManifest({ id: 'HI', extensions: 'hi', client: 'c'.repeat(32) }), /shorter than 32/);
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

const hostImage = ({ bytes = 0x8000, declared = null, entry = VM_BASE + 0x2001,
                     flashMagic = 0x42464346, abi = ABI } = {}) => {
  const image = Buffer.alloc(bytes, 0xa5);
  const put = (offset, value) => image.writeUInt32LE(value >>> 0, offset);
  put(0x0, flashMagic);
  put(0x1000, 0x432000d1);
  put(0x1004, entry);
  put(0x1020, VM_BASE);
  put(0x1024, declared ?? bytes);
  put(HOST_ID_OFFSET, HOSTID_MAGIC);
  put(HOST_ID_OFFSET + 4, abi);
  put(HOST_ID_OFFSET + 8, HOST_SERVICES);
  put(HOST_ID_OFFSET + 12, bytes);
  image.fill(0, HOST_ID_OFFSET + 16, HOST_ID_OFFSET + 32);
  image.write('TestHost', HOST_ID_OFFSET + 16, 'latin1');
  return image;
};

test('a host package round-trips and leaves the image byte-identical', () => {
  const image = hostImage();
  const pkg = buildHostPackage({ image });
  const header = parseHostPackage(pkg);
  assert.equal(pkg.length, HOST_PACKAGE_HEADER_BYTES + image.length);
  assert.ok(pkg.subarray(HOST_PACKAGE_HEADER_BYTES).equals(image), 'payload is what objcopy produced');
  assert.equal(header.name, 'TestHost');
  assert.equal(header.abi, ABI);
  assert.equal(header.targetBase, VM_BASE);
});

test('an image short of the length it declares is padded with the 0xFF an erased page holds', () => {
  const image = hostImage({ bytes: 0x8000, declared: 0x8000 + 0xc00 });
  const pkg = buildHostPackage({ image });
  assert.equal(parseHostPackage(pkg).payloadBytes, 0x8000 + 0xc00);
  assert.ok(pkg.subarray(HOST_PACKAGE_HEADER_BYTES + 0x8000).every((b) => b === 0xff));
});

test('an image the minimal loader would not enter is refused before it can be installed', () => {
  assert.throws(() => buildHostPackage({ image: hostImage({ flashMagic: 0 }) }), /would not be entered|fails the checks/);
  assert.throws(() => buildHostPackage({ image: hostImage({ entry: VM_BASE + 0x2000 }) }), /fails the checks/);
  assert.throws(() => buildHostPackage({ image: hostImage({ entry: VM_BASE + 0x9001 }) }), /fails the checks/);
});

test('an image longer than the slot, or shorter than it claims, is refused', () => {
  assert.throws(() => buildHostPackage({ image: hostImage({ bytes: HOST_SLOT_BYTES + 0x1000 }) }), /slot is/);
  assert.throws(() => buildHostPackage({ image: hostImage({ bytes: 0x8000, declared: 0x4000 }) }), /declares 16384 bytes/);
});

test('a host carrying another ABI is refused rather than installed and rejected on target', () => {
  assert.throws(() => buildHostPackage({ image: hostImage({ abi: ABI + 1 }) }), /ABI/);
});

test('corrupting any header byte, or the payload at either end, is caught', () => {
  // The whole header, as host_install_test.cpp sweeps it, rather than a sample
  // of it. Sweeping the payload too is the same property but thirty seconds of
  // CRC, so it is sampled and the name says so.
  const pkg = buildHostPackage({ image: hostImage() });
  const positions = [...Array(HOST_PACKAGE_HEADER_BYTES).keys()]
    .concat([HOST_PACKAGE_HEADER_BYTES, HOST_ID_OFFSET, pkg.length - 1]);
  for (const at of positions) {
    const bad = Buffer.from(pkg);
    bad[at] ^= 0x80;
    assert.throws(() => parseHostPackage(bad), new RegExp('.'), `corruption at ${at} went unnoticed`);
  }
});

test('hostSlotValid agrees with the five words the minimal image reads', () => {
  const ok = { flashMagic: 0x42464346, vectorMagic: 0x432000d1, entry: VM_BASE + 0x1001,
               bootBase: VM_BASE, imageBytes: 0x8000 };
  assert.ok(hostSlotValid(ok));
  assert.ok(!hostSlotValid({ ...ok, entry: VM_BASE + 0x1000 }), 'a non-Thumb entry is refused');
  assert.ok(!hostSlotValid({ ...ok, bootBase: 0 }));
  assert.ok(!hostSlotValid({ ...ok, imageBytes: HOST_SLOT_BYTES + 1 }));
  assert.ok(!hostSlotValid({ ...ok, imageBytes: 0x1000 }));
});
