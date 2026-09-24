import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

import { blankComments } from '../../../tools/lib/source-text.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const SOURCE_EXTENSIONS = new Set(['.ino', '.c', '.cpp', '.h']);
const SKIPPED_DIRECTORIES = new Set(['tests', 'TRMenuFiles']);

const MESSAGE_FORMATTERS = /\b(SendMsgPrintfln|SendMsgPrintf|SendStrPrintfln)\s*\(\s*(\S)/g;
// The return type is matched loosely on purpose. Pinning it to `void` meant that giving
// SendMsgPrintfln a bool return -- to say whether the C64 actually read the message --
// stopped this from recognising its own definitions and prototypes, and the scan reported
// all five of them as call sites with a non-literal format. What makes a line a
// declaration here is the type-then-name shape, not which type it happens to be.
const FORMATTER_DECLARATION = /^\s*(?:FLASHMEM\s+|extern\s+)*(?:void|bool)\s+Send(?:Msg|Str)/;

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

// Every call to `name` in the tree, minus its own declarations: in C, `void` followed
// by the name opens a definition or a prototype and never a call, so the call's own
// left context is what tells the two apart. Scoped to that context rather than to the
// whole line on purpose. A line-anchored pattern -- `^[^\n]*NAME\(`, or the leading
// whitespace the FullPathToSelected scan used -- yields at most one match per line and
// swallows the rest of the line with it, so a call sharing a line with a prototype or
// with a bounded call went unexamined, and `^\s+` also missed `if (x) NAME(...)` and
// any call at column zero. `\s+` here spans newlines, so a definition that carries its
// return type on the line above is excluded too.
const callsTo = (name) => new RegExp(String.raw`(?<!\bvoid\s+)\b${name}\([^;]*\);`, 'g');

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

  // The definition lives in the tree too, and `[^;]*` runs from its parameter list to
  // the first semicolon in its body -- so left to itself the pattern matches the
  // definition, and whether that looks like an unbounded call depends on what its first
  // statement happens to end in. It ended in `];` until the body started with
  // `MenuItemSel();`, at which point the definition began reporting itself. callsTo
  // drops it by the `void` in front of its name, which no call can have.
  //
  // Bounded: this reads the size argument's spelling, not the buffer it belongs to, so
  // `sizeof` of the wrong variable passes. The destination-to-size pairing is not what
  // this gate proves.
  assert.deepEqual(scanTree(callsTo('GetCurrentFilePathName'),
                            (call) => !/,\s*sizeof \w+\);$/.test(call)), []);
});

test('the path handed to a device-writing item type is built inside its caller\'s buffer', () => {
  // DriveDirPath is 256 bytes and grows through unbounded strcat, so the
  // MaxNamePathLength arithmetic that makes the destination wide enough is not
  // by itself a bound.
  const loader = read('DriveDirLoad.ino');
  assert.match(loader, /void FullPathToSelected\(char \*Path, size_t Size, const char \*Name\)/);
  assert.doesNotMatch(loader, /\bsprintf\(Path,/);

  // The definition lives in this file too, and callsTo drops it by the `void` in front
  // of its name. Scanned across the tree rather than in this file alone: both callers
  // are here today, and a hard-coded file is what lets the next one through in silence.
  assert.deepEqual(scanTree(callsTo('FullPathToSelected'),
                            (call) => !/,\s*sizeof \w+,/.test(call)), []);
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
  assert.match(image, /out\[n\+\+\] = nameByteSafe\(c\) \? \(char\)c : nameSubstitute;/);
  assert.match(image, /snprintf\(out, bytes, "%s", noDescriptor\);/);
  assert.match(image, /snprintf\(out, bytes, "%s", unnamedHost\);/);
  // Through literalBytes, not sizeof: `const char *noDescriptor` would size this from the
  // pointer and silently hand back the char[13]. Every source goes through largest(), so
  // a fourth one cannot be added without widening the buffer with it.
  assert.match(image, /nameBytes =\s*\n?\s*largest\(largest\(sizeof\(VmHostId::name\) \+ 1, literalBytes\(noDescriptor\)\),\s*\n?\s*literalBytes\(unnamedHost\)\);/);
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

test('a descriptor name reaches the C64 as glyphs, never as control codes', () => {
  // The twelve bytes are third-party: vm_host_scan checks magic, ABI, services, both CRCs
  // and the boot words, and identity() checks only the magic, so nothing constrains their
  // content. A C64 executes $00-$1f and $80-$9f rather than drawing them -- $93 clears the
  // screen, $0d ends the line -- and the removal notice carrying those bytes is the one
  // holding "do not power off" across a 45-second erase. Bounding the read by length, as
  // %.*s did, does not bound the bytes.
  const image = read('MinimalBoot/Common/VMBootImage.h');
  assert.match(image, /return !\(c < 0x20 \|\| \(c >= 0x80 && c <= 0x9f\) \|\| c == nameByteQuote\);/);
  // $22 draws and still is not safe: CHROUT toggles quote mode on it, and the next
  // control code is then drawn rather than executed. The name's row ends without a
  // RETURN to clear the flag, so the next one is PrintBanner's ChrClear on `u`.
  assert.match(image, /nameByteQuote = 0x22;/);
  assert.match(image, /nameByteBlank\(unsigned char c\) \{ return c == 0x20 \|\| c == 0xa0; \}/);

  // Substituted, not dropped: an all-control name still renders twelve visible bytes, so
  // the host can be named in a report instead of leaving a blank mid-erase. The behaviour
  // this shape produces is asserted by execution in vm/tests/registry_test.cpp.
  assert.match(image, /if \(!named\) snprintf\(out, bytes, "%s", unnamedHost\);/);
  // Decided over the whole field, not over what fit: a narrow buffer must shorten a name,
  // never turn one into "(unnamed)".
  assert.match(image, /for \(size_t i = 0; i < sizeof id->name; i\+\+\)/);

  // Nothing goes round the filter: no message formatter takes the descriptor field
  // itself. displayName is the only way the name reaches a screen or the serial line.
  // Matched on the field rather than on a bare `name`, which tryLaunch already has as a
  // parameter holding a filename -- that one is not the descriptor and may be printed.
  assert.deepEqual(scanTree(/SendMsg\w*\([^;]*?\b\w*[Ii]d(?:\.|->)name\b/g, () => true), []);
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

// Returns the body of a top-level C function by brace matching from its opening
// brace, so a test can ask what one function does without the next one's text
// leaking in.
function functionBody(source, name) {
  const signature = new RegExp(`^[^\\n]*\\bvoid\\s+${name}\\s*\\([^)]*\\)\\s*$`, 'm');
  const at = source.search(signature);
  assert.notEqual(at, -1, `no definition of ${name}`);
  const open = source.indexOf('{', at);
  assert.notEqual(open, -1, `${name} has no body`);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(open, i + 1);
  }
  return assert.fail(`${name}'s body is unterminated`);
}

test('every rCtlMake*StrWAIT handler leaves the C64 string read selected and rewound', () => {
  // PrintFileName prints every dynamic row in the settings menu. It ends at
  // PrintSerialStringLoaded and selects nothing, so it prints whatever the firmware
  // last pointed ptrSerialString at, from wherever that read stopped. A handler
  // behind an rCtlMake*StrWAIT therefore owes its caller a read pointed at
  // SerialStringBuf and rewound. MakeExtHostStr did not, and the Installed
  // Extensions row printed the tail of the banner's version string -- blank, because
  // the byte one past its terminator happened to be zero. Both pages that used to
  // open-code the select around that gap now call PrintFileName instead, so for all
  // three handlers this is load-bearing rather than defense in depth.
  //
  // The handler set is derived from the dispatch rather than listed here: that is
  // what makes a new rCtlMake* control code whose handler forgets the close fail.
  const dispatch = blankComments(read('MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c'));
  const statusSource = read('MinimalBoot/Common/IO_Handlers/StatusFunctions.c');

  const routes = [...dispatch.matchAll(
    /case\s+(rCtlMake\w*)[^:]*:\s*(?:IO1\[wRegControl\]\s*=\s*Data\s*;\s*)?IO1\[rwRegStatus\]\s*=\s*(rs\w+)\s*;/g,
  )].map(({ 1: control, 2: status }) => ({ control, status }));

  // rCtlMakeInfoStrWAIT, rCtlMakeExtHostStrWAIT, and the rCtlMakeStrWAIT_First..Last
  // range. A dispatch that stops matching is a rename, not a reason to pass.
  assert.equal(routes.length, 3, `expected 3 rCtlMake* routes, found ${routes.length}`);

  // rs code -> handler, from the trailing comment on each StatusFunction[] entry.
  const handlers = new Map([...statusSource.matchAll(/&(\w+)\s*,\s*\/\/\s*(rs\w+)/g)]
    .map(({ 1: fn, 2: status }) => [status, fn]));

  for (const { control, status } of routes) {
    const handler = handlers.get(status);
    assert.ok(handler, `${status} has no StatusFunction[] entry naming its handler`);
    const body = blankComments(functionBody(statusSource, handler));
    assert.match(body, /\bSelectSerialStringBuf\s*\(\s*\)\s*;/,
      `${control} -> ${status} -> ${handler} never calls SelectSerialStringBuf`);
  }

  // And the helper still does both halves. Asserting the call alone would pass on a
  // helper that had quietly stopped rewinding, which is the half a partial read needs.
  const helper = blankComments(functionBody(statusSource, 'SelectSerialStringBuf'));
  assert.match(helper, /\bptrSerialString\s*=\s*SerialStringBuf\s*;/, 'the helper no longer points the read at SerialStringBuf');
  assert.match(helper, /\bStringOffset\s*=\s*0\s*;/, 'the helper no longer rewinds the read');
});
