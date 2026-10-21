import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

import { blankComments } from '../../../tools/lib/source-text.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const SOURCE_EXTENSIONS = new Set(['.ino', '.c', '.cpp', '.h']);
const SKIPPED_DIRECTORIES = new Set(['tests', 'TRMenuFiles']);

const MESSAGE_FORMATTERS = /\b(SendMsgPrintfln|SendMsgPrintf|SendStrPrintfln)\s*\(\s*(\S)/g;
const FORMATTER_DECLARATION = /^\s*(?:FLASHMEM\s+|extern\s+)*(?:void|bool)\s+Send(?:Msg|Str)/;

function firmwareSources(dir = FIRMWARE_ROOT) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(entry => {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) return SKIPPED_DIRECTORIES.has(entry.name) ? [] : firmwareSources(full);
    return SOURCE_EXTENSIONS.has(path.extname(entry.name)) ? [full] : [];
  });
}

// Comments are blanked rather than cut so the offsets still name lines.
export function nonLiteralFormats(source, label) {
  const lineOf = (index) => source.slice(0, index).split('\n').length;
  const lineAt = (index) => source.split('\n')[lineOf(index) - 1];

  return [...blankComments(source).matchAll(MESSAGE_FORMATTERS)]
    .filter(({ 2: firstArg }) => firstArg !== '"')
    .filter(({ index }) => !FORMATTER_DECLARATION.test(lineAt(index)))
    .map(({ 1: name, index }) => `${label}:${lineOf(index)} ${name}`);
}

const scanSource = (file) =>
  nonLiteralFormats(fs.readFileSync(file, 'utf8'), path.relative(FIRMWARE_ROOT, file));

test('a C64 message format is always a literal, never card- or network-supplied text', () => {
  // vsnprintf bounds the output but still walks the argument list, so a filename
  // reaching the format makes an item name on the card a %n write primitive.
  assert.deepEqual(firmwareSources().flatMap(scanSource), []);
});

test('the scan still sees a call wrapped after its open paren', () => {
  assert.deepEqual(nonLiteralFormats('SendMsgPrintfln(\n   MenuSelCpy.Name);\n', 'x.ino'),
                   ['x.ino:1 SendMsgPrintfln']);
  assert.deepEqual(nonLiteralFormats('SendMsgPrintfln(\n   "%s", MenuSelCpy.Name);\n', 'x.ino'), []);
  assert.deepEqual(nonLiteralFormats('/* SendMsgPrintfln(Name); */\n', 'x.ino'), []);
});

test('no caller throws away whether the current path fit', () => {
  // Callers store this path in EEPROM, an NFC tag or a mount; a truncated one names another file.
  const discarded = firmwareSources().flatMap((file) => {
    const source = blankComments(fs.readFileSync(file, 'utf8'));
    return [...source.matchAll(/^[ \t]*GetCurrentFilePathName\([^;]*\);/gm)]
      .map(({ index }) => `${path.relative(FIRMWARE_ROOT, file)}:${source.slice(0, index).split('\n').length}`);
  });
  assert.deepEqual(discarded, []);
});
