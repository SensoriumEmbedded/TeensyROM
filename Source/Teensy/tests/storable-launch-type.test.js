import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

import { blankComments } from '../../../tools/lib/source-text.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const read = (name) => fs.readFileSync(path.join(FIRMWARE_ROOT, name), 'utf8');

// Everything that writes a file reference into EEPROM or an NFC tag for something later
// to launch without the user present. The autolaunch one is the reason this rule exists:
// DoHostInstall ends in RebootTR, the reboot reads the stored name back, and nothing in
// that loop passes through the menu where the setting could be cleared.
const STORED_REFERENCE_SETTERS = ['SetAutoLaunch', 'HotKeySetLaunch', 'WriteNFCTagCheck'];

// Brace-matched from the opening brace so one function's text cannot carry into the next.
function functionBody(source, name) {
  // Leading whitespace allowed: the predicates live indented inside a header's #ifndef.
  const at = source.search(new RegExp(String.raw`^[ \t]*[\w*][^\n(]*\b${name}\s*\([^)]*\)\s*$`, 'm'));
  assert.notEqual(at, -1, `${name} is not defined where this test looks for it`);
  const open = source.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(open, i + 1);
  }
  assert.fail(`${name}'s body is unbalanced`);
}

test('the predicate names the whole rule, not just the executable half', () => {
  const header = blankComments(read('MinimalBoot/Common/DriveDirLoad.h'));
  const body = functionBody(header, 'IsStorableLaunchType');

  // Both halves have to be there. `>= rtFilePrg` alone admits every device-write type,
  // which is the bug; `!IsDeviceWriteType` alone admits the non-executable types.
  assert.match(body, /ItemType\s*>=\s*rtFilePrg/, 'lost the executable half');
  assert.match(body, /!\s*IsDeviceWriteType\s*\(\s*ItemType\s*\)/, 'lost the device-write half');

  // And the class it excludes is the one that rewrites the Teensy.
  const writeType = functionBody(header, 'IsDeviceWriteType');
  assert.match(writeType, /rtFileHex/);
  assert.match(writeType, /rtFileTRH/);
});

test('every stored-reference setter asks the predicate, not the bare type order', () => {
  const source = blankComments(read('MinimalBoot/Common/IO_Handlers/StatusFunctions.c'));

  const offending = STORED_REFERENCE_SETTERS.flatMap((name) => {
    const body = functionBody(source, name);
    const problems = [];
    if (!/IsStorableLaunchType\s*\(/.test(body)) problems.push(`${name} does not call IsStorableLaunchType`);
    // The test this replaced. It rejects types *below* rtFilePrg, so every type that
    // rewrites the device -- .hex and .trh, both above it -- passed straight through.
    if (/ItemType\s*<\s*rtFilePrg/.test(body)) problems.push(`${name} still tests ItemType < rtFilePrg`);
    return problems;
  });

  assert.deepEqual(offending, []);
});

test('the NFC directory filter shares the predicate rather than spelling it out again', () => {
  const body = functionBody(blankComments(read('nfcScan.ino')), 'nfcReadTagLaunch');
  assert.match(body, /IsStorableLaunchType\s*\(/);
});
