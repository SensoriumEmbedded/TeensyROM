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

test('GEOS 128, listed on every machine, starts only once the menu has reported a C128', () => {
  // Its boot ROM cannot run on a C64, where the menu still lists it
  const source = blankComments(fs.readFileSync(path.join(FIRMWARE_ROOT, 'GeosLauncher.ino'), 'utf8'));
  assert.match(functionBody(source, 'LaunchGeos'),
    /^\{[^{}]*?\bif\s*\(\s*Launcher->Machine\s*==\s*GeosC128\s*&&\s*!\s*HostIsC128\s*\(\s*\)\s*\)\s*\{[^}]*\breturn\s+false\s*;/);
});
