import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

import { blankComments } from '../../../tools/lib/source-text.mjs';

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

// The whole file at once, not line by line: a call wrapped after its open
// paren would otherwise carry its format onto a line the scan never pairs it
// with. Comments are blanked rather than cut so the offsets still name lines.
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

// Every firmware source, not a list of the files that happen to call it today:
// a hard-coded list lets the next call site through in silence.
function scanTree(pattern, offending) {
  return firmwareSources().flatMap((file) => {
    const source = blankComments(fs.readFileSync(file, 'utf8'));
    return [...source.matchAll(pattern)]
      .filter(({ 0: text }) => offending(text))
      .map(({ 0: text, index }) =>
        `${path.relative(FIRMWARE_ROOT, file)}:${source.slice(0, index).split('\n').length} ${text}`);
  });
}

test('a C64 message format is always a literal, never card- or network-supplied text', () => {
  // vsnprintf bounds the output but still walks the argument list, so a filename
  // reaching the format makes an item name on the card a %n write primitive.
  assert.deepEqual(firmwareSources().flatMap(scanSource), []);
});

test('the scan still sees a call wrapped after its open paren', () => {
  // How the fixed DriveDirLoad.ino call would come back: reflowed, not retyped.
  assert.deepEqual(nonLiteralFormats('SendMsgPrintfln(\n   MenuSelCpy.Name);\n', 'x.ino'),
                   ['x.ino:1 SendMsgPrintfln']);
  assert.deepEqual(nonLiteralFormats('SendMsgPrintfln(\n   "%s", MenuSelCpy.Name);\n', 'x.ino'), []);
  assert.deepEqual(nonLiteralFormats('/* SendMsgPrintfln(Name); */\n', 'x.ino'), []);
});

test('the current path and filename is built inside the buffer the caller owns', () => {
  const handler = read('MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c');
  assert.match(handler, /void GetCurrentFilePathName\(char\* FilePathName, size_t Size\)/);
  assert.doesNotMatch(handler, /\bsprintf\(FilePathName/);

  assert.deepEqual(scanTree(/GetCurrentFilePathName\([^;]*\);/g,
                            (call) => !/,\s*sizeof \w+\);$/.test(call)), []);
});

test('the path handed to a device-writing item type is built inside its caller\'s buffer', () => {
  // DriveDirPath is 256 bytes and grows through unbounded strcat, so the
  // MaxNamePathLength arithmetic that makes the destination wide enough is not
  // by itself a bound.
  const loader = read('DriveDirLoad.ino');
  assert.match(loader, /void FullPathToSelected\(char \*Path, size_t Size, const char \*Name\)/);
  assert.doesNotMatch(loader, /\bsprintf\(Path,/);

  // The definition lives in this file too, so match calls by their leading
  // whitespace -- the definition is preceded by its return type.
  for (const [, call] of loader.matchAll(/^\s+(FullPathToSelected\([^;]*\);)/gm)) {
    assert.match(call, /,\s*sizeof \w+,/, call);
  }
});

test('the default SID record stays inside the block that holds it', () => {
  // Source byte, path, name -- packed into MaxPathLength and written to an
  // EEPROM slot of exactly that size, so both fields have to fit together.
  const loader = read('DriveDirLoad.ino');
  assert.doesNotMatch(loader, /strcpy\(LatestSIDLoaded/);
  assert.match(loader, /snprintf\(LatestSIDLoaded \+ 1, MaxPathLength - 2, "%s", Path\);/);
  assert.match(loader, /snprintf\(LatestSIDLoaded \+ NameOffset, MaxPathLength - NameOffset, "%s", Name\);/);

  assert.deepEqual(scanTree(/EEPreadNBuf\(eepAdDefaultSID[^\n]*\n[^\n]*/g,
                            (reader) => !/TerminateSIDRecord\(/.test(reader)), []);
});

test('the CRT name field is read only as far as the 32 bytes it occupies', () => {
  // LoadFile parses the main header into a CRT_MAIN_HDR_LEN stack buffer, so the
  // name field ends at its last byte and a name filling all 32 carries no terminator.
  for (const parser of ['FileParsers.ino', 'MinimalBoot/Min_DriveDirLoad.ino']) {
    assert.match(read(parser), /SendMsgPrintfln\("Name: %\.32s", \(CRT_Image\+0x20\)\)/);
  }
});

test('the installed host name is rendered in one place, by a buffer wide enough for both sources', () => {
  // The descriptor's name field is a fixed 12 bytes that need not be terminated, and the
  // stand-in for a host that has no descriptor is 15. A buffer re-derived per site from
  // the field alone is char[13], which a strcpy of the placeholder overran by three.
  // Every site takes VmBootImage::nameBytes, which is the wider of the two, and reads the
  // field through a precision that stops at its last byte.
  const image = read('MinimalBoot/Common/VMBootImage.h');
  assert.match(image, /snprintf\(out, bytes, "%\.\*s", \(int\)sizeof id->name, id->name\);/);
  assert.match(image, /snprintf\(out, bytes, "%s", noDescriptor\);/);
  // Through literalBytes, not sizeof: `const char *noDescriptor` would size this from the
  // pointer and silently hand back the char[13].
  assert.match(image, /nameBytes =\s*\n?\s*sizeof\(VmHostId::name\) \+ 1 > literalBytes\(noDescriptor\)/);
  assert.match(image, /template<unsigned N> static constexpr unsigned literalBytes\(const char \(&\)\[N\]\)/);

  // No site re-derives the buffer from the field, and none copies the name by hand.
  assert.deepEqual(scanTree(/char \w+\[\s*sizeof [\w.>:-]*\bname\s*\+\s*1\s*\]/g, () => true), []);
  assert.deepEqual(scanTree(/strcpy\(\s*\w*[Nn]ame\w*\s*,\s*"\(no descriptor\)"/g, () => true), []);

  // The sites that read the descriptor in place rather than through displayName take
  // their precision from the field too. A literal width is right only while it equals
  // sizeof(VmHostId::name): widen the field and these messages clip the host's name
  // while nameBytes and displayName track it, so the refusal a third party reads
  // becomes the one place that disagrees with the field. Truncation, not overrun --
  // which is why it would go unnoticed.
  assert.deepEqual(scanTree(/%\.\d+s[^;]*?\bname\b/g, () => true), []);
});

test('the formatters that write the C64 message buffer are bounded', () => {
  // The scan above keeps card-supplied text out of the format; this keeps the
  // output inside the buffer. Either alone leaves the overflow reachable, and
  // reverting these to sprintf/vsprintf used to leave the suite green.
  const bounded = [
    ['FileParsers.ino', 'vsnprintf(SerialStringBuf, sizeof SerialStringBuf - 2, Fmt, ap)'],
    ['FileParsers.ino', 'vsnprintf(SerialStringBuf, sizeof SerialStringBuf, Fmt, ap)'],
    ['MinimalBoot/Min_DriveDirLoad.ino', 'vsnprintf(SerialStringBuf, sizeof SerialStringBuf, Fmt, ap)'],
    ['MinimalBoot/Common/IO_Handlers/StatusFunctions.c', 'snprintf(SerialStringBuf, sizeof SerialStringBuf,'],
    ['MinimalBoot/Common/IO_Handlers/Swift_ATcommands.c', 'snprintf(Buf, sizeof Buf,'],
  ];
  for (const [file, call] of bounded) assert.ok(read(file).includes(call), `${file}: ${call}`);

  for (const file of ['FileParsers.ino', 'MinimalBoot/Min_DriveDirLoad.ino']) {
    assert.doesNotMatch(read(file), /\bvsprintf\(/, `${file} still formats unbounded`);
  }
});
