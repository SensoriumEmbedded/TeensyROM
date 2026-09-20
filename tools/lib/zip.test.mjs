// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { extractZip, listZip } from './zip.mjs';

// Builds a zip in memory: entries are { name, data, deflate?, mode?, crc? }.
function makeZip(entries) {
  const parts = [];
  const central = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name);
    const raw = Buffer.from(e.data ?? '');
    const packed = e.deflate ? zlib.deflateRawSync(raw) : raw;
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(e.deflate ? 8 : 0, 8);
    local.writeUInt32LE(e.crc ?? zlib.crc32(raw), 14);
    local.writeUInt32LE(packed.length, 18);
    local.writeUInt32LE(raw.length, 22);
    local.writeUInt16LE(name.length, 26);
    parts.push(local, name, packed);

    const cd = Buffer.alloc(46);
    cd.writeUInt32LE(0x02014b50, 0);
    cd[5] = 3; // made on Unix
    cd.writeUInt16LE(e.deflate ? 8 : 0, 10);
    cd.writeUInt32LE(e.crc ?? zlib.crc32(raw), 16);
    cd.writeUInt32LE(packed.length, 20);
    cd.writeUInt32LE(raw.length, 24);
    cd.writeUInt16LE(name.length, 28);
    cd.writeUInt32LE(((e.mode ?? 0o100644) << 16) >>> 0, 38);
    cd.writeUInt32LE(offset, 42);
    central.push(cd, name);
    offset += local.length + name.length + packed.length;
  }
  const centralBuf = Buffer.concat(central);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(centralBuf.length, 12);
  eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...parts, centralBuf, eocd]);
}

function withTmp(fn) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zip-test-'));
  try {
    return fn(dir);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

test('extracts stored and deflated entries, directories, and the executable bit', () => {
  withTmp((dir) => {
    const zipPath = path.join(dir, 'a.zip');
    fs.writeFileSync(zipPath, makeZip([
      { name: 'tool/' , mode: 0o040755 },
      { name: 'tool/run', data: '#!/bin/sh\necho hi\n', mode: 0o100755 },
      { name: 'tool/lib/data.txt', data: 'x'.repeat(5000), deflate: true },
    ]));
    const out = path.join(dir, 'out');
    extractZip(zipPath, out);
    assert.equal(fs.readFileSync(path.join(out, 'tool/lib/data.txt'), 'utf8'), 'x'.repeat(5000));
    assert.equal(fs.readFileSync(path.join(out, 'tool/run'), 'utf8'), '#!/bin/sh\necho hi\n');
    if (process.platform !== 'win32') assert.ok(fs.statSync(path.join(out, 'tool/run')).mode & 0o100);
  });
});

test('the filter picks entries by name', () => {
  withTmp((dir) => {
    const zipPath = path.join(dir, 'a.zip');
    fs.writeFileSync(zipPath, makeZip([{ name: 'KickAss.jar', data: 'jar' }, { name: 'docs/big.pdf', data: 'pdf' }]));
    const out = path.join(dir, 'out');
    extractZip(zipPath, out, (name) => name === 'KickAss.jar');
    assert.deepEqual(fs.readdirSync(out), ['KickAss.jar']);
  });
});

test('refuses entries that would escape the destination', () => {
  withTmp((dir) => {
    const zipPath = path.join(dir, 'evil.zip');
    fs.writeFileSync(zipPath, makeZip([{ name: '../escaped.txt', data: 'nope' }]));
    assert.throws(() => extractZip(zipPath, path.join(dir, 'out')), /outside the destination/);
    assert.equal(fs.existsSync(path.join(dir, 'escaped.txt')), false);
  });
});

test('a CRC mismatch is an error', () => {
  withTmp((dir) => {
    const zipPath = path.join(dir, 'bad.zip');
    fs.writeFileSync(zipPath, makeZip([{ name: 'f', data: 'hello', crc: 1234 }]));
    assert.throws(() => extractZip(zipPath, path.join(dir, 'out')), /corrupt/);
  });
});

test('something that is not a zip is an error', () => {
  assert.throws(() => listZip(Buffer.from('definitely not a zip file, just text'.repeat(3))), /not a zip file/);
});
