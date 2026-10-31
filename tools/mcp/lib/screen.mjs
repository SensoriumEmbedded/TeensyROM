// SPDX-License-Identifier: MIT
//
// The VIC-II text screen: where it is in memory, and its screen codes as text.

export const VIC_MEMPTR = 0xd018;
export const CIA2_PORT_A = 0xdd00;
export const SCREEN_COLUMNS = 40;
export const SCREEN_ROWS = 25;
export const SCREEN_BYTES = SCREEN_COLUMNS * SCREEN_ROWS;

const VIC_BANK_BYTES = 0x4000;
const SCREEN_STEP = 0x400;
const CHARSET_STEP = 0x800;
// The VIC sees the character ROM, not RAM, at these offsets of banks 0 and 2.
const ROM_UPPER_GRAPHICS = 0x1000;
const ROM_LOWER_UPPER = 0x1800;
const ROM_BANKS = [0, 2];

export const CHARSETS = {
  lowerUpper: 'lower_upper',
  upperGraphics: 'upper_graphics',
  custom: 'custom',
};

const REVERSE_VIDEO = 0x80;
const CODE_AT = 0;
const CODE_A = 1;
const CODE_Z = 26;
const CODE_SHIFT_A = 65;
const CODE_SHIFT_Z = 90;
const SYMBOLS_AFTER_Z = '[\u00a3]\u2191\u2190';
const PUNCTUATION_FIRST = 0x20;
const PUNCTUATION_LAST = 0x3f;
const NO_LETTER = '.';

export function vicBank(cia2PortA) {
  return 3 - (cia2PortA & 3);
}

export function screenAddress(memptr, cia2PortA) {
  return vicBank(cia2PortA) * VIC_BANK_BYTES + ((memptr >> 4) & 0xf) * SCREEN_STEP;
}

export function charsetIn(memptr, cia2PortA) {
  const offset = ((memptr >> 1) & 7) * CHARSET_STEP;
  if (!ROM_BANKS.includes(vicBank(cia2PortA))) return CHARSETS.custom;
  if (offset === ROM_LOWER_UPPER) return CHARSETS.lowerUpper;
  if (offset === ROM_UPPER_GRAPHICS) return CHARSETS.upperGraphics;
  return CHARSETS.custom;
}

// Screen codes 1-26 and 65-90 are different letters in the two ROM sets.
// Anything with no letter in the set (graphics, line drawing) reads as '.'.
function codeAsText(screenCode, charset) {
  const code = screenCode & ~REVERSE_VIDEO;
  const lowerUpper = charset === CHARSETS.lowerUpper;
  if (code === CODE_AT) return '@';
  if (code >= CODE_A && code <= CODE_Z) {
    return String.fromCharCode(code - CODE_A + (lowerUpper ? 0x61 : 0x41));
  }
  if (code > CODE_Z && code < PUNCTUATION_FIRST) return SYMBOLS_AFTER_Z[code - CODE_Z - 1];
  if (lowerUpper && code >= CODE_SHIFT_A && code <= CODE_SHIFT_Z) {
    return String.fromCharCode(code - CODE_SHIFT_A + 0x41);
  }
  if (code >= PUNCTUATION_FIRST && code <= PUNCTUATION_LAST) return String.fromCharCode(code);
  return NO_LETTER;
}

export function screenRows(screen, charset) {
  const rows = [];
  for (let row = 0; row < SCREEN_ROWS; row += 1) {
    const codes = screen.subarray(row * SCREEN_COLUMNS, (row + 1) * SCREEN_COLUMNS);
    rows.push(Array.from(codes, (code) => codeAsText(code, charset)).join(''));
  }
  return rows;
}
