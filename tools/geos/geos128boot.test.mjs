// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { commandExists } from '../lib/toolchain.mjs';
import { header } from './geos128boot.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const committedHeader = path.join(root, 'Source/Teensy/GEOS/Geos128Boot.h');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('acme') && !process.env.ACME && !inCi && 'acme is not on PATH';

test('the committed GEOS 128 boot ROM header matches its source', { skip }, () => {
  const committed = fs.readFileSync(committedHeader, 'utf8').replaceAll('\r\n', '\n');
  assert.equal(header(), committed, 'rerun node tools/geos/geos128boot.mjs');
});

test('the boot ROM is a C128 function ROM whose vectors stay in this ROM', { skip }, () => {
  const bytes = (name) => [...header().match(new RegExp(`${name}\\[\\d+\\] PROGMEM = \\{([^}]*)\\}`))[1].matchAll(/0x[0-9a-f]+/g)].map((m) => parseInt(m[0], 16));
  const roml = bytes('Geos128BootRoml');
  const [rti, nmi, nmiHigh, , , irq, irqHigh] = bytes('Geos128BootVectors');
  assert.deepEqual(roml.slice(7, 10), [0x43, 0x42, 0x4d], '"CBM" at $8007');
  assert.equal(rti, 0x40);
  assert.equal(nmi | (nmiHigh << 8), 0xfff9, 'NMI returns at once');
  assert.equal(roml[(irq | (irqHigh << 8)) - 0x8000], 0x78, 'IRQ restarts the cold start, at its SEI');
});
