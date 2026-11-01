// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import { parsePortListing } from './trserial.mjs';

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
