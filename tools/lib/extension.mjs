// SPDX-License-Identifier: MIT
//
// The extension package formats, in one place. Firmware parses these in
// Source/Teensy/MinimalBoot/Common/{VMABI,VMRegistry}.h; this module is the
// only thing that writes them, so the two can be checked against each other.
//
// A package is a directory /VMS/<id> on the SD card holding exactly three
// files: manifest.vmi, the module image, and the C64 client cartridge.

export const IMAGE_MAGIC = 0x314d564d;  // 'MVM1'
export const ABI = 2;
export const CODE_BASE = 0x18000, CODE_LIMIT = 0x30000;
export const DATA_BASE = 0x20014000, DATA_LIMIT = 0x20044000;
export const DATA_BYTES = DATA_LIMIT - DATA_BASE;
// The top 16 KiB of RAM2 belongs to the firmware (Teensy's CrashReport and the
// loader's boot failure record), so the guest arena stops short of it. Keep in
// step with VM_RAM_* in Source/Teensy/MinimalBoot/Common/VMABI.h.
export const RAM_RESERVED_BYTES = 16 * 1024;
export const RAM_BYTES = 512 * 1024 - RAM_RESERVED_BYTES;
export const RAM2_RO_BYTES = 80 * 1024;
export const PROFILE_LEGACY = 0, PROFILE_RAM2_RO = 1;
export const SERVICE = {
  FILES: 1, CLOCK: 2, PACKETS: 4, WRITE: 8, GUEST_RAM: 16, RAM2_RO: 128,
};
export const BASE_SERVICES = SERVICE.FILES | SERVICE.CLOCK | SERVICE.PACKETS | SERVICE.WRITE | SERVICE.GUEST_RAM;
export const KNOWN_SERVICES = BASE_SERVICES | SERVICE.RAM2_RO;
export const CLIENT_BYTES = 0x6070;
export const DESCRIPTOR_OFFSET = 0x4070;

// Reflected CRC32 (polynomial 0xedb88320), the same one vm_crc32 computes.
export function crc32(bytes) {
  let c = 0xffffffff;
  for (const value of bytes) {
    c ^= value;
    for (let i = 0; i < 8; i++) c = (c >>> 1) ^ (c & 1 ? 0xedb88320 : 0);
  }
  return (c ^ 0xffffffff) >>> 0;
}

// The 64-byte MVM1 header, followed by .text, then .data, then (profile 1 only)
// the RAM2 constants. bss is not stored; the loader zeroes it after the copy.
export function buildImage({ code, data = Buffer.alloc(0), bssBytes = 0, entry,
                             requiredServices = BASE_SERVICES,
                             profile = PROFILE_LEGACY, readOnly = Buffer.alloc(0) }) {
  if (!code?.length) throw new Error('Module image has no code');
  if (code.length > CODE_LIMIT - CODE_BASE) throw new Error(`Module code is ${code.length} bytes, window is ${CODE_LIMIT - CODE_BASE}`);
  if (data.length > DATA_BYTES) throw new Error('Module .data exceeds the DTCM window');
  if (data.length + bssBytes > DATA_BYTES) throw new Error(`Module .data + .bss is ${data.length + bssBytes} bytes, window is ${DATA_BYTES}`);
  if (!(entry & 1)) throw new Error(`Entry 0x${entry.toString(16)} has no Thumb bit; the module entry must be Thumb code`);
  if ((entry & ~1) < CODE_BASE || (entry & ~1) >= CODE_BASE + code.length) {
    throw new Error(`Entry 0x${entry.toString(16)} falls outside the module code window`);
  }
  if (requiredServices & ~KNOWN_SERVICES) {
    throw new Error(`Image requires services 0x${(requiredServices & ~KNOWN_SERVICES).toString(16)} that the base profile does not provide`);
  }
  if (profile === PROFILE_RAM2_RO) {
    if (!readOnly.length || readOnly.length > RAM2_RO_BYTES) throw new Error(`Profile 1 needs 1..${RAM2_RO_BYTES / 1024} KiB of RAM2 constants`);
    if (!(requiredServices & SERVICE.RAM2_RO)) throw new Error('Profile 1 must require VM_SERVICE_RAM2_RO');
  } else if (profile === PROFILE_LEGACY) {
    if (readOnly.length) throw new Error('Profile 0 stores no RAM2 constants');
    if (requiredServices & SERVICE.RAM2_RO) throw new Error('Profile 0 must not require VM_SERVICE_RAM2_RO');
  } else {
    throw new Error(`Unknown memory profile ${profile}`);
  }

  const payload = Buffer.concat([code, data, readOnly]);
  const header = Buffer.alloc(64);
  const fields = [IMAGE_MAGIC, ABI, 64, code.length, data.length, bssBytes,
                  entry, CODE_BASE, DATA_BASE, requiredServices, crc32(payload), 0,
                  profile, readOnly.length, 0, 0];
  fields.forEach((value, i) => header.writeUInt32LE(value >>> 0, i * 4));
  header.writeUInt32LE(crc32(header), 44);  // header_crc, over the header with the field zeroed
  return Buffer.concat([header, payload]);
}

// Reads back what buildImage wrote, applying the checks vm_valid_header applies
// on target. Used by the tests and by the packager's self-check, so a header
// the loader would refuse is refused here instead of on the C64.
export function parseImage(image) {
  if (image.length < 64) throw new Error('Image is shorter than its header');
  const field = (i) => image.readUInt32LE(i * 4);
  const header = {
    magic: field(0), abi: field(1), headerBytes: field(2), codeBytes: field(3),
    dataBytes: field(4), bssBytes: field(5), entry: field(6), codeBase: field(7),
    ramBase: field(8), requiredServices: field(9), payloadCrc: field(10), headerCrc: field(11),
    profile: field(12), readOnlyBytes: field(13),
  };
  if (header.magic !== IMAGE_MAGIC) throw new Error('Not an MVM1 image');
  if (header.abi !== ABI) throw new Error(`Image is ABI ${header.abi}, loader is ABI ${ABI}`);
  if (header.headerBytes !== 64) throw new Error('Unexpected header length');
  if (header.codeBase !== CODE_BASE || header.ramBase !== DATA_BASE) throw new Error('Image was linked for a different memory map');
  const zeroed = Buffer.from(image.subarray(0, 64));
  zeroed.writeUInt32LE(0, 44);
  if (crc32(zeroed) !== header.headerCrc) throw new Error('Image header CRC mismatch');
  if (field(14) || field(15)) throw new Error('Reserved header words must be zero');
  if (!header.codeBytes || header.codeBytes > CODE_LIMIT - CODE_BASE) {
    throw new Error(`Module code is ${header.codeBytes} bytes, window is ${CODE_LIMIT - CODE_BASE}`);
  }
  if (header.dataBytes > DATA_BYTES || header.bssBytes > DATA_BYTES - header.dataBytes) {
    throw new Error(`Module .data + .bss is ${header.dataBytes + header.bssBytes} bytes, window is ${DATA_BYTES}`);
  }
  if (!(header.entry & 1) || (header.entry & ~1) < CODE_BASE || (header.entry & ~1) >= CODE_BASE + header.codeBytes) {
    throw new Error(`Entry 0x${header.entry.toString(16)} falls outside the module code window`);
  }
  if (header.requiredServices & ~KNOWN_SERVICES) {
    throw new Error(`Image requires services 0x${(header.requiredServices & ~KNOWN_SERVICES).toString(16)} that the base profile does not provide`);
  }
  if (header.profile === PROFILE_RAM2_RO) {
    if (!header.readOnlyBytes || header.readOnlyBytes > RAM2_RO_BYTES) throw new Error(`Profile 1 needs 1..${RAM2_RO_BYTES / 1024} KiB of RAM2 constants`);
    if (!(header.requiredServices & SERVICE.RAM2_RO)) throw new Error('Profile 1 must require VM_SERVICE_RAM2_RO');
  } else if (header.profile === PROFILE_LEGACY) {
    if (header.readOnlyBytes) throw new Error('Profile 0 stores no RAM2 constants');
    if (header.requiredServices & SERVICE.RAM2_RO) throw new Error('Profile 0 must not require VM_SERVICE_RAM2_RO');
  } else {
    throw new Error(`Unknown memory profile ${header.profile}`);
  }
  const payloadBytes = header.codeBytes + header.dataBytes + header.readOnlyBytes;
  if (image.length !== 64 + payloadBytes) throw new Error(`Image is ${image.length} bytes, header describes ${64 + payloadBytes}`);
  if (crc32(image.subarray(64)) !== header.payloadCrc) throw new Error('Image payload CRC mismatch');
  return header;
}

const NAME = /^[A-Za-z0-9_.-]+$/;
// Extensions the stock menu owns. A package may not claim any of them.
export const PROTECTED_EXTENSIONS = ['prg', 'crt', 'hex', 'p00', 'sid', 'kla', 'koa', 'ocp',
  'pic', 'art', 'aas', 'hpi', 'txt', 'nfo', 'md', 'seq', 'd64', 'd71', 'd81', 'reu'];

// Six ASCII lines, newline terminated. The firmware parser is strict about all
// six, so build it here rather than by hand.
export function buildManifest({ id, extensions, module = 'engine.mvm', client = 'client.crt' }) {
  const list = Array.isArray(extensions) ? extensions.join(',') : extensions;
  // The limits are the firmware's Manifest field widths, which readManifest
  // rejects the line for exceeding.
  for (const [label, value, limit] of [['id', id, 24], ['module', module, 32], ['client', client, 32]]) {
    if (!NAME.test(value) || value.includes('..')) throw new Error(`Invalid ${label}: ${value}`);
    if (value.length >= limit) throw new Error(`Package ${label} must be shorter than ${limit} characters`);
  }
  if (!list.length || list.length > 7) throw new Error(`Extension list "${list}" must be 1..7 characters`);
  for (const extension of list.split(',')) {
    if (!extension || !NAME.test(extension) || extension.includes('.')) throw new Error(`Invalid extension: ${extension}`);
    if (PROTECTED_EXTENSIONS.includes(extension.toLowerCase())) throw new Error(`Extension "${extension}" belongs to the stock menu`);
  }
  return `VM1\n${id}\n${list}\n${module}\n${client}\nEND\n`;
}

// The client cartridge: an ordinary 16 KiB EasyFlash .crt, plus a third CHIP
// carrying the 128-byte VMH1 descriptor that ties the cartridge to its package.
// Exactly 0x6070 bytes; the firmware rejects any other size.
export function buildClientCrt({ id, bank0, bank1, name = id }) {
  if (bank0.length > 8192 || bank1.length > 8192) throw new Error('Each client bank is at most 8192 bytes');
  if (Buffer.byteLength(id) >= 24) throw new Error('Package id must be shorter than 24 characters');
  const low = Buffer.alloc(8192), high = Buffer.alloc(8192), spare = Buffer.alloc(8192);
  bank0.copy(low); bank1.copy(high);

  const header = Buffer.alloc(64);
  header.write('C64 CARTRIDGE   ', 0, 'latin1');
  header.writeUInt32BE(64, 16);      // header length
  header.writeUInt16BE(0x0100, 20);  // version 1.0
  header.writeUInt16BE(32, 22);      // cartridge type 32: EasyFlash
  header.writeUInt8(0, 24);          // EXROM
  header.writeUInt8(0, 25);          // GAME
  header.write(name.slice(0, 31), 32, 'latin1');

  const chip = (bank, address, contents) => {
    const head = Buffer.alloc(16);
    head.write('CHIP', 0, 'latin1');
    head.writeUInt32BE(16 + contents.length, 4);
    head.writeUInt16BE(0, 8);         // ROM
    head.writeUInt16BE(bank, 10);
    head.writeUInt16BE(address, 12);
    head.writeUInt16BE(contents.length, 14);
    return Buffer.concat([head, contents]);
  };

  const descriptor = Buffer.alloc(128);
  descriptor.write('VMH1', 0, 'latin1');
  descriptor.writeUInt8(ABI, 4);
  descriptor.writeUInt32LE(crc32(Buffer.concat([low, high])), 8);
  descriptor.write(id, 16, 'latin1');
  descriptor.writeUInt32LE(crc32(descriptor.subarray(0, 124)), 124);
  descriptor.copy(spare);

  const crt = Buffer.concat([header, chip(0, 0x8000, low), chip(0, 0xa000, high), chip(1, 0x8000, spare)]);
  if (crt.length !== CLIENT_BYTES) throw new Error(`Client cartridge is ${crt.length} bytes, firmware requires ${CLIENT_BYTES}`);
  if (crt.subarray(DESCRIPTOR_OFFSET, DESCRIPTOR_OFFSET + 4).toString('latin1') !== 'VMH1') {
    throw new Error('Descriptor did not land at its fixed offset');
  }
  return crt;
}
