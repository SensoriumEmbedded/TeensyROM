// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {extensionLinkerScript, extensionBootdata, flashResidentLibraryAround, minimalLinkerScript, mainLinkerScript, HOST_CODE_KIB} from './extension-image.mjs';
import {CODE_BASE} from './extension.mjs';

const linkers = path.resolve(import.meta.dirname, '../BootLinkerFiles');
const progmemBlock = (ld) => ld.slice(ld.indexOf('.text.progmem : {'), ld.indexOf('.text.itcm : {'));

test('the stock host is linked at 64 KiB, and 96 KiB moves only its code ceiling', () => {
  const stock = extensionLinkerScript(linkers);
  assert.equal(stock, extensionLinkerScript(linkers, 64));
  assert.match(stock, /ASSERT\(__exidx_end <= 0x10000,/);
  assert.equal(extensionLinkerScript(linkers, 96), stock.replace(
    '__exidx_end <= 0x10000, "Host code exceeds its 64 KiB ITCM budget; a --host-sketch host that needs more can build with --host-code-kib 96"',
    '__exidx_end <= 0x18000, "Host code exceeds its 96 KiB ITCM budget"'));
  assert.match(stock, /_itcm_block_count = 6;/);
  for (const invalid of [0, 32, 65, 128, '64', NaN]) {
    assert.throws(() => extensionLinkerScript(linkers, invalid), /64 or 96/);
  }
});

test('no host code budget reaches into the module code window', () => {
  for (const kib of HOST_CODE_KIB) assert.ok(kib * 1024 <= CODE_BASE, `${kib} KiB`);
});

// ld matches a file-name pattern with fnmatch(pattern, name, 0): '*', '?', '[...]', and
// '\' escaping the next character, inside a bracket or out. This is glibc's reading.
function fnmatchRegExp(pattern) {
  const escape = (c) => c.replace(/[.*+?^${}()|[\]\\/-]/g, '\\$&');
  let source = '';
  for (let i = 0; i < pattern.length; i++) {
    const c = pattern[i];
    if (c === '*') source += '.*';
    else if (c === '?') source += '.';
    else if (c === '\\') source += escape(pattern[++i]);
    else if (c !== '[') source += escape(c);
    else {
      let members = '';
      for (i++; pattern[i] !== ']'; i++) members += escape(pattern[pattern[i] === '\\' ? ++i : i]);
      source += `[${members}]`;
    }
  }
  return new RegExp(`^${source}$`);
}

// libiberty's, which ld links where the host C library has no fnmatch (MinGW),
// transcribed from its fnmatch.c. It differs once a class has matched: skipping the rest of the
// class, it reads '\' as an escape, so '[/\\]' matching '/' skips past the ']'.
function libibertyFnmatch(pattern, name) {
  const match = (p, n) => {
    while (p < pattern.length) {
      let c = pattern[p++];
      if (c === '?') {
        if (n++ >= name.length) return false;
      } else if (c === '\\') {
        if (name[n++] !== pattern[p++]) return false;
      } else if (c === '*') {
        // As libiberty has it, every '?' or '*' after the first steps over a character.
        for (c = pattern[p++]; c === '?' || c === '*'; c = pattern[p++], n++) if (c === '?' && n >= name.length) return false;
        if (c === undefined) return true;
        p--;
        for (let rest = n; rest < name.length; rest++) if (match(p, rest)) return true;
        return false;
      } else if (c === '[') {
        if (n >= name.length) return false;
        const not = pattern[p] === '!' || pattern[p] === '^';
        if (not) p++;
        let matched = false;
        c = pattern[p++];
        for (;;) {
          let start = c, end = c;
          if (c === '\\') { if (p >= pattern.length) return false; start = end = pattern[p++]; }
          if (c === undefined) return false;
          c = pattern[p++];
          if (c === '-' && pattern[p] !== ']') {
            end = pattern[p++];
            if (end === '\\') end = pattern[p++];
            if (end === undefined) return false;
            c = pattern[p++];
          }
          if (name[n] >= start && name[n] <= end) { matched = true; break; }
          if (c === ']') break;
        }
        if (matched) {
          while (c !== ']') {
            if (c === undefined) return false;
            c = pattern[p++];
            if (c === '\\') { if (p >= pattern.length) return false; p++; }
          }
        }
        if (matched === not) return false;
        n++;
      } else if (name[n++] !== c) {
        return false;
      }
    }
    return n === name.length;
  };
  return match(0, 0);
}

// Each line's file pattern, and the files its EXCLUDE_FILE leaves in ITCM.
const flashTextPatterns = (ld) =>
  [...progmemBlock(ld).matchAll(/^\t\t(\S+)\((?:EXCLUDE_FILE\(([^)]*)\) )?\.text\*\)$/gm)]
    .map((match) => ({ file: match[1], staying: match[2]?.split(' ') ?? [] }));
// Under each reading, glibc's then libiberty's.
const placedInFlash = (patterns, objectPath) =>
  [(pattern) => fnmatchRegExp(pattern).test(objectPath), (pattern) => libibertyFnmatch(pattern, objectPath)]
    .map((matches) => patterns.some(({ file, staying }) => matches(file) && !staying.some(matches)));

// ld gives an input section to the first output section that names it, so a library
// pattern outside .text.progmem, or after it, leaves that code in ITCM.
test('the SD card libraries run from flash but for the SDIO driver, on either path separator', () => {
  const patterns = flashTextPatterns(extensionLinkerScript(linkers));
  for (const library of ['SdFat', 'SD', 'SPI']) {
    for (const objectPath of [`/b/ext/libraries/${library}/src/x.cpp.o`, `C:\\b\\ext\\libraries\\${library}\\src\\x.cpp.o`]) {
      assert.deepEqual(placedInFlash(patterns, objectPath), [true, true], objectPath);
    }
  }
  for (const objectPath of ['/b/ext/libraries/SdFatX/x.cpp.o', '/b/ext/libraries/SPIFlash/x.cpp.o',
    'C:\\b\\ext\\sketch\\VMBoot.ino.cpp.o', '/b/ext/sketch/VMBoot.ino.cpp.o', 'core.a',
    // SdioCard::readData masks interrupts, so the SDIO driver stays in ITCM.
    '/b/ext/libraries/SdFat/SdCard/SdioTeensy.cpp.o', 'C:\\b\\ext\\libraries\\SdFat\\SdCard\\SdioTeensy.cpp.o']) {
    assert.deepEqual(placedInFlash(patterns, objectPath), [false, false], objectPath);
  }
});

test('the two readings part on the order of a class, which is why the escaped \\ comes first', () => {
  assert.deepEqual([libibertyFnmatch('*[/\\\\]x', '/a/x'), fnmatchRegExp('*[/\\\\]x').test('/a/x')], [false, true]);
  assert.deepEqual([libibertyFnmatch('*[\\\\/]x', '/a/x'), fnmatchRegExp('*[\\\\/]x').test('/a/x')], [true, true]);
  // And on a run of stars, each after the first of which libiberty spends on a character.
  assert.deepEqual([libibertyFnmatch('a**b', 'ab'), libibertyFnmatch('a**b', 'axb'), fnmatchRegExp('a**b').test('ab')],
    [false, true, true]);
});

test('a CRLF checkout of the stock linker files edits the same as an LF one', () => {
  const crlf = fs.mkdtempSync(path.join(os.tmpdir(), 'crlf-linkers-'));
  try {
    for (const name of fs.readdirSync(linkers)) {
      fs.writeFileSync(path.join(crlf, name), fs.readFileSync(path.join(linkers, name), 'utf8').replace(/\r?\n/g, '\r\n'));
    }
    for (const edit of [extensionLinkerScript, extensionBootdata, minimalLinkerScript, mainLinkerScript]) {
      assert.equal(edit(crlf), edit(linkers), edit.name);
    }
  } finally {
    fs.rmSync(crlf, { recursive: true, force: true });
  }
});

test('a build directory inside a flash-resident library is named, since its sketch would match', () => {
  const patterns = flashTextPatterns(extensionLinkerScript(linkers));
  for (const [dir, library] of [['/x/libraries/SD/out', 'SD'], ['/x/libraries/SdFat/b', 'SdFat'], ['/x/libraries/SPI', 'SPI'],
                                ['/x/libraries/SDX/out', undefined], ['/x/libs/SD/out', undefined], ['/x/out', undefined]]) {
    assert.equal(flashResidentLibraryAround(dir), library, dir);
    const inFlash = library !== undefined;
    assert.deepEqual(placedInFlash(patterns, path.join(dir, 'extension/sketch/VMBoot.ino.cpp.o')), [inFlash, inFlash], dir);
  }
});
