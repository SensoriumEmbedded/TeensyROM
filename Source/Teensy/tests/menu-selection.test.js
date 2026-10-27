import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

import { blankComments } from '../../../tools/lib/source-text.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');

function functionBody(source, name) {
  const at = source.search(new RegExp(String.raw`^[\w*][^\n(]*\b${name}\s*\([^)]*\)\s*$`, 'm'));
  assert.notEqual(at, -1, `${name} is not defined where this test looks for it`);
  const open = source.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(open, i + 1);
  }
  assert.fail(`${name}'s body is unbalanced`);
}

test('a menu change drops the old selection along with the old item count', () => {
  // The status-function dispatch calls UpdateItemTypeReg after the work, so a selection
  // made in a 300-item SD listing would index MenuSource past the end of TeensyROMMenu.
  const source = blankComments(fs.readFileSync(path.join(FIRMWARE_ROOT, 'Teensy.ino'), 'utf8'));
  assert.match(functionBody(source, 'SetMenu'), /\bSelItemFullIdx\s*=\s*0\s*;/);
});

test('a random NFC pick from a TeensyROM directory chooses among the rows the menu lists', () => {
  // On a C64 the menu hides the GEOS 128 row, which a pick over the directory's full size can still land on
  const source = blankComments(fs.readFileSync(path.join(FIRMWARE_ROOT, 'nfcScan.ino'), 'utf8'));
  const body = functionBody(source, 'nfcReadTagLaunch');
  assert.doesNotMatch(body, /\.Size\s*\/\s*sizeof\s*\(\s*StructMenuItem\s*\)/);
  assert.match(body, /\bTeensyDirItemCount\s*\(\s*&\s*TeensyROMMenu\s*\[\s*MenuNum\s*\]\s*\)/);
});
