// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { boardKind, parseBanner } from '../lib/banner.mjs';

// As captured from a plain TeensyROM on 2026-09-27, and as Min_SerUSBIO.ino prints it.
const FULL = '\xcc\x64\n  FW: TeensyROM v0.8.0.11\r\n      Sep 25 2026, 03:03:51\r\n  Teensy: 816MHz  62.1C  UID: 14470230\r\n' +
  '  C64  PAL Vid  60 Hz\n  Boot: complete\n';
const MINIMAL = '\n  FW: TeensyROM+ v0.8.0.11(minimal)\r\n      Aug  5 2026, 10:41:55\r\n  Teensy: 816MHz  55.0C  UID: 19277260\r\n';

test('the main image\'s reply gives the build, the chip ID, the machine and the boot state', () => {
  const { raw, ...info } = parseBanner(FULL);
  assert.deepEqual(info, {
    version: 'TeensyROM v0.8.0.11', built: 'Sep 25 2026, 03:03:51', uid: '14470230', minimal: false,
    machine: 'C64', video: 'PAL', boot: 'complete',
  });
});

test('a C128 still booting its menu says so', () => {
  const info = parseBanner('  FW: TeensyROM+ v0.8.0.13\n  C128  NTSC Vid  60 Hz\n  Boot: in progress\n');
  assert.equal(info.machine, 'C128');
  assert.equal(info.video, 'NTSC');
  assert.equal(info.boot, 'in progress');
});

test('MinimalBoot answering is told apart from the full firmware', () => {
  const { raw, ...info } = parseBanner(MINIMAL);
  assert.deepEqual(info, {
    version: 'TeensyROM+ v0.8.0.11', built: 'Aug  5 2026, 10:41:55', uid: '19277260', minimal: true,
    machine: null, video: null, boot: null,
  });
  assert.equal(boardKind(info.version), 'tr-plus');
});

test('no answer is all nulls', () => {
  const { raw, ...info } = parseBanner('');
  assert.deepEqual(info, {
    version: null, built: null, uid: null, minimal: false, machine: null, video: null, boot: null,
  });
});

test('the + in the version is what tells the cartridges apart', () => {
  assert.equal(boardKind('TeensyROM+ v0.8.0.11'), 'tr-plus');
  assert.equal(boardKind('TeensyROM v0.8.0.11'), 'tr');
  assert.equal(boardKind(null), null);
});
