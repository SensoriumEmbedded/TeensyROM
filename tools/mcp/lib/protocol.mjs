// SPDX-License-Identifier: MIT
//
// The TeensyROM control protocol's wire values (docs/ControlComms.md).
// Commands and their integer fields go to the board most significant byte
// first; the board answers least significant byte first.

export const TOKENS = {
  LAUNCH_FILE: 0x6444,
  RESET_C64: 0x64ee,
  VERSION_INFO: 0x6476,
  FW_CHECK: 0x64e0,
  READ_C64_MEM: 0x64fd,
  POST_FILE: 0x64bb,
  DELETE_FILE: 0x64cf,
  GET_DIR_NDJSON: 0x64de,
  ACK: 0x64cc,
  FAIL: 0x9b7f,
  FW_FULL: 0x64e2,
  FW_MINIMAL: 0x64e1,
  DIR_START: 0x5a5a,
  DIR_END: 0xa5a5,
};

export const DRIVES = { usb: 0, sd: 1, teensy: 2 };

export const IMAGES = { [TOKENS.FW_FULL]: 'main', [TOKENS.FW_MINIMAL]: 'minimal' };

// The firmware's own name for each wire value, so a test can check them
// against Source/Teensy.
export const FIRMWARE_NAMES = {
  LaunchFileToken: TOKENS.LAUNCH_FILE,
  ResetC64Token: TOKENS.RESET_C64,
  VersionInfoToken: TOKENS.VERSION_INFO,
  FWCheckToken: TOKENS.FW_CHECK,
  ReadC64MemToken: TOKENS.READ_C64_MEM,
  PostFileToken: TOKENS.POST_FILE,
  DeleteFileToken: TOKENS.DELETE_FILE,
  GetDirNDJSONToken: TOKENS.GET_DIR_NDJSON,
  AckToken: TOKENS.ACK,
  FailToken: TOKENS.FAIL,
  FWFullToken: TOKENS.FW_FULL,
  FWMinimalToken: TOKENS.FW_MINIMAL,
  StartDirectoryListToken: TOKENS.DIR_START,
  EndDirectoryListToken: TOKENS.DIR_END,
  rmtUSBDrive: DRIVES.usb,
  rmtSD: DRIVES.sd,
  rmtTeensy: DRIVES.teensy,
};

export function toBoard(value, width = 2) {
  const bytes = Buffer.alloc(width);
  bytes.writeUIntBE(value, 0, width);
  return bytes;
}

export function fromBoard(bytes) {
  return bytes.readUInt16LE(0);
}

export function boardReply(value) {
  const bytes = Buffer.alloc(2);
  bytes.writeUInt16LE(value, 0);
  return bytes;
}

export function pathField(path) {
  return Buffer.from(`${path}\0`, 'latin1');
}

export function checksum(data) {
  let sum = 0;
  for (const byte of data) sum = (sum + byte) & 0xffff;
  return sum;
}
