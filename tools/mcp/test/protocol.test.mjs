// SPDX-License-Identifier: MIT
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import assert from 'node:assert/strict';

import { DRIVES, FIRMWARE_NAMES, TOKENS } from '../lib/protocol.mjs';

const SOURCE = path.resolve(import.meta.dirname, '../../../Source/Teensy');
const DEFINITIONS = [
  /^\s*#define\s+(\w+)\s+(0x[0-9A-Fa-f]+)\b/gm,
  /^\s*const\s+uint\d+_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;/gm,
  /^\s*(\w+)\s*=\s*(\d+)\s*,/gm,
];

function firmwareDefinitions() {
  const found = new Map();
  const files = fs.readdirSync(SOURCE, { recursive: true }).filter((name) => /\.(h|c|cpp|ino)$/.test(name));
  for (const file of files) {
    const text = fs.readFileSync(path.join(SOURCE, file), 'latin1');
    for (const pattern of DEFINITIONS) {
      for (const [, name, value] of text.matchAll(pattern)) {
        if (!found.has(name)) found.set(name, new Set());
        found.get(name).add(Number(value));
      }
    }
  }
  return found;
}

test('every wire value matches the constant the firmware defines for it', () => {
  const firmware = firmwareDefinitions();
  const drifted = Object.entries(FIRMWARE_NAMES)
    .filter(([name, ours]) => !firmware.has(name) || [...firmware.get(name)].some((theirs) => theirs !== ours))
    .map(([name, ours]) => `${name}: ours 0x${ours.toString(16)}, firmware ${[...(firmware.get(name) ?? [])]}`);
  assert.deepEqual(drifted, []);
});

test('every token and drive this server sends is one the firmware check covers', () => {
  const checked = new Set(Object.values(FIRMWARE_NAMES));
  const unchecked = Object.entries({ ...TOKENS, ...DRIVES }).filter(([, value]) => !checked.has(value));
  assert.deepEqual(unchecked, []);
});
