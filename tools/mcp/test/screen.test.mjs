// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { CHARSETS, SCREEN_BYTES, charsetIn, screenAddress, screenRows } from '../lib/screen.mjs';

const POWER_ON_MEMPTR = 0x15;
const MENU_MEMPTR = 0x17;
const BANK_0 = 0x97;
const BANK_3 = 0x94;

test('the power-on screen is at $0400 in uppercase/graphics', () => {
  assert.equal(screenAddress(POWER_ON_MEMPTR, BANK_0), 0x0400);
  assert.equal(charsetIn(POWER_ON_MEMPTR, BANK_0), CHARSETS.upperGraphics);
});

test('the TeensyROM menu is at $0400 in lower/uppercase', () => {
  assert.equal(screenAddress(MENU_MEMPTR, BANK_0), 0x0400);
  assert.equal(charsetIn(MENU_MEMPTR, BANK_0), CHARSETS.lowerUpper);
});

test('a screen in VIC bank 3 is found there, and its charset can only be RAM', () => {
  assert.equal(screenAddress(0x25, BANK_3), 0xc800);
  assert.equal(charsetIn(0x25, BANK_3), CHARSETS.custom);
});

test('screen codes read as the letters of the selected set', () => {
  const screen = Buffer.alloc(SCREEN_BYTES, 0x20);
  screen.set([0x08, 0x09, 0x20, 0x48, 0x49, 0x2e, 0x31, 0x81, 0x66], 0);
  assert.equal(screenRows(screen, CHARSETS.lowerUpper)[0].trimEnd(), 'hi HI.1a.');
  assert.equal(screenRows(screen, CHARSETS.upperGraphics)[0].trimEnd(), 'HI ...1A.');
  assert.equal(screenRows(screen, CHARSETS.upperGraphics).length, 25);
});
