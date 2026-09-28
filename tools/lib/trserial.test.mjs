// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import { parsePortListing, parseBanner, boardKind } from './trserial.mjs';

test('every board in a teensy_ports listing is kept, with the address teensy_post_compile needs', () => {
  const boards = parsePortListing(
    'usb:80000/1/0/5/2/2 COM4 (Teensy 4.1) Serial+MIDI\r\n' +
    'usb:80000/1/0/5/3/3 COM12 (Teensy 4.1) Serial+MIDI\r\n');
  assert.deepEqual(boards, [
    { port: 'COM4', bootloader: false, location: 'usb:80000/1/0/5/2/2', label: 'COM4 (Teensy 4.1) Serial+MIDI' },
    { port: 'COM12', bootloader: false, location: 'usb:80000/1/0/5/3/3', label: 'COM12 (Teensy 4.1) Serial+MIDI' },
  ]);
});

test('a board in the bootloader has no port, and a /dev path is a port', () => {
  const [waiting, mac] = parsePortListing(
    'usb:80000/1/0/5/3/3 (Teensy 4.1) Bootloader\n' +
    'usb:14100000 /dev/cu.usbmodem141101 (Teensy 4.1) Serial\n\n');
  assert.equal(waiting.port, null);
  assert.equal(waiting.bootloader, true);
  assert.equal(waiting.location, 'usb:80000/1/0/5/3/3');
  assert.equal(mac.port, '/dev/cu.usbmodem141101');
});

test('an empty listing is no boards', () => {
  assert.deepEqual(parsePortListing(''), []);
});

// As captured from a plain TeensyROM on 2026-09-27, and as Min_SerUSBIO.ino prints it.
const FULL = '\xcc\x64\n  FW: TeensyROM v0.8.0.11\r\n      Sep 25 2026, 03:03:51\r\n  Teensy: 816MHz  62.1C  UID: 14470230\r\n' +
  '  C64  PAL Vid  60 Hz\n  Boot: complete\n';
const MINIMAL = '\n  FW: TeensyROM+ v0.8.0.11(minimal)\r\n      Aug  5 2026, 10:41:55\r\n  Teensy: 816MHz  55.0C  UID: 19277260\r\n';

test('the version reply gives the build, its date and the chip ID', () => {
  const { raw, ...info } = parseBanner(FULL);
  assert.deepEqual(info, { version: 'TeensyROM v0.8.0.11', built: 'Sep 25 2026, 03:03:51', uid: '14470230', minimal: false });
});

test('MinimalBoot answering is told apart from the full firmware', () => {
  const { raw, ...info } = parseBanner(MINIMAL);
  assert.deepEqual(info, { version: 'TeensyROM+ v0.8.0.11', built: 'Aug  5 2026, 10:41:55', uid: '19277260', minimal: true });
  assert.equal(boardKind(info.version), 'tr-plus');
});

test('no answer is all nulls', () => {
  const { raw, ...info } = parseBanner('');
  assert.deepEqual(info, { version: null, built: null, uid: null, minimal: false });
});

test('the + in the version is what tells the cartridges apart', () => {
  assert.equal(boardKind('TeensyROM+ v0.8.0.11'), 'tr-plus');
  assert.equal(boardKind('TeensyROM v0.8.0.11'), 'tr');
  assert.equal(boardKind(null), null);
});
