// SPDX-License-Identifier: MIT
//
// Node port of Source/C64/bin2header.py, which is bin2header 0.3.1 (MIT, Copyright (c)
// 2017-2022 Jordan Irwin <antumdeluge@gmail.com>, https://github.com/AntumDeluge/bin2header).
// Only the part the TeensyROM build used is ported: 8-bit words, 12 bytes per line, LF line
// endings, and the optional type modifier (-t "PROGMEM "). The output is byte-identical to the
// script's for those inputs; bin2header.test.mjs checks that against the headers in the repo.
//
// The script wrote its output in text mode, so on Windows every line ended in CRLF; git's
// `* text=auto` (see .gitattributes) stores headers with LF either way, so LF is what we write.

export const BYTES_PER_LINE = 12;

// The script's "unusable in a C identifier" list. It replaces these, plus "." in the array
// name, with "_". It leaves "." alone in the output *file* name, so `SettingsMenu.prg`
// becomes `SettingsMenu.prg.h` but the array is `SettingsMenu_prg`.
const BAD_CHARS = /[\\+\-* ]/g;

// "cia tod.prg" -> "cia_tod_prg"
export function headerSymbolName(fileName) {
  let name = fileName.replace(BAD_CHARS, '_').replaceAll('.', '_');
  if (/^\p{N}/u.test(name)) name = `_${name}`;
  return name;
}

// The file bin2header.py writes for an input file: "cia tod.prg" -> "cia_tod.prg.h"
export function headerFileName(fileName) {
  return `${fileName.replace(BAD_CHARS, '_')}.h`;
}

// bytes: Buffer/Uint8Array. name: the input's file name (e.g. "SettingsMenu.prg"); both the
// array name and the include guard come from it. typemod: text placed before `static`,
// e.g. "PROGMEM " (with its trailing space).
export function binToHeader(bytes, { name, typemod = '' }) {
  const symbol = headerSymbolName(name);
  const guard = `${symbol.toUpperCase()}_H`;

  const rows = [];
  for (let i = 0; i < bytes.length; i += BYTES_PER_LINE) {
    const row = [];
    for (let j = i; j < Math.min(i + BYTES_PER_LINE, bytes.length); j++) {
      row.push(`0x${bytes[j].toString(16).padStart(2, '0')}`);
    }
    rows.push(`\t${row.join(', ')}`);
  }

  return [
    `#ifndef ${guard}`,
    `#define ${guard}`,
    '',
    `${typemod}static const unsigned char ${symbol}[] = {`,
    ...(rows.length ? [rows.join(',\n')] : []),
    '};',
    '',
    `#endif /* ${guard} */`,
    '',
  ].join('\n');
}
