import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const SOURCE_EXTENSIONS = new Set(['.ino', '.c', '.cpp', '.h']);
const SKIPPED_DIRECTORIES = new Set(['tests', 'TRMenuFiles']);

const MESSAGE_FORMATTERS = /\b(SendMsgPrintfln|SendMsgPrintf|SendStrPrintfln)\s*\(\s*(\S)/g;
const FORMATTER_DECLARATION = /^\s*(?:FLASHMEM\s+|extern\s+)*void\s+Send(?:Msg|Str)/;

const read = name => fs.readFileSync(path.join(FIRMWARE_ROOT, name), 'utf8');

function firmwareSources(dir = FIRMWARE_ROOT) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(entry => {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) return SKIPPED_DIRECTORIES.has(entry.name) ? [] : firmwareSources(full);
    return SOURCE_EXTENSIONS.has(path.extname(entry.name)) ? [full] : [];
  });
}

function nonLiteralFormats(file) {
  return fs.readFileSync(file, 'utf8').split('\n').flatMap((line, index) => {
    if (FORMATTER_DECLARATION.test(line) || line.trimStart().startsWith('//')) return [];
    return [...line.matchAll(MESSAGE_FORMATTERS)]
      .filter(([, , firstArg]) => firstArg !== '"')
      .map(([, name]) => `${path.relative(FIRMWARE_ROOT, file)}:${index + 1} ${name}`);
  });
}

test('a C64 message format is always a literal, never card- or network-supplied text', () => {
  // vsnprintf bounds the output but still walks the argument list, so a filename
  // reaching the format makes an item name on the card a %n write primitive.
  assert.deepEqual(firmwareSources().flatMap(nonLiteralFormats), []);
});

test('the current path and filename is built inside the buffer the caller owns', () => {
  const handler = read('MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c');
  assert.match(handler, /void GetCurrentFilePathName\(char\* FilePathName, size_t Size\)/);
  assert.doesNotMatch(handler, /\bsprintf\(FilePathName/);

  const callers = ['MeatloafComm.ino', 'MinimalBoot/Common/IO_Handlers/StatusFunctions.c'];
  for (const caller of callers) {
    for (const [call] of read(caller).matchAll(/GetCurrentFilePathName\([^;]*\);/g)) {
      assert.match(call, /,\s*sizeof \w+\);$/, `${caller}: ${call}`);
    }
  }
});

test('the default SID record stays inside the block that holds it', () => {
  // Source byte, path, name -- packed into MaxPathLength and written to an
  // EEPROM slot of exactly that size, so both fields have to fit together.
  const loader = read('DriveDirLoad.ino');
  assert.doesNotMatch(loader, /strcpy\(LatestSIDLoaded/);
  assert.match(loader, /snprintf\(LatestSIDLoaded \+ 1, MaxPathLength - 2, "%s", Path\);/);
  assert.match(loader, /snprintf\(LatestSIDLoaded \+ NameOffset, MaxPathLength - NameOffset, "%s", Name\);/);

  const status = read('MinimalBoot/Common/IO_Handlers/StatusFunctions.c');
  for (const [reader] of status.matchAll(/EEPreadNBuf\(eepAdDefaultSID[^\n]*\n[^\n]*/g)) {
    assert.match(reader, /TerminateSIDRecord\(/, reader);
  }
});
