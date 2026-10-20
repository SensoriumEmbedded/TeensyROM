// SPDX-License-Identifier: MIT
// The TR block device's registers and disk image backend, held to docs/TR_Block_Device.md.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

import { commandExists } from '../../../tools/lib/toolchain.mjs';

const FIRMWARE_ROOT = path.resolve(import.meta.dirname, '..');
const HARNESS_SOURCE = path.join(FIRMWARE_ROOT, 'tests/trblock_harness.c');

const NOT_CI = ['', '0', 'false'];
const inCi = !NOT_CI.includes(process.env.CI ?? '');
const skip = !commandExists('cc') && !inCi && 'cc is not on PATH';

const SECTOR = 256;
const WINDOW = 64;
const PAGES = SECTOR / WINDOW;
const CMD = 0x08, ADDR0 = 0x09, ADDR1 = 0x0a, PAGE = 0x0b, TOKEN = 0x0c, STATUS = 0x0d, ECHO = 0x0e, ID = 0x10;
const WINDOW_AT = 0x40;
const READ = 1, WRITE = 2, FLUSH = 3, INFO = 4;
const BUSY = 0x80, DONE = 0x5a, NO_IMAGE = 0xe1, BAD_ADDRESS = 0xe2, READ_ONLY = 0xe5, BAD_COMMAND = 0xe6;

const zoneSectors = (track) => {
  const zone = (track - 1) % 35 + 1;
  return zone <= 17 ? 21 : zone <= 24 ? 19 : zone <= 30 ? 18 : 17;
};

const FORMATS = {
  d64: { kind: 1, tracks: 35, sectors: zoneSectors },
  d71: { kind: 2, tracks: 70, sectors: zoneSectors },
  d81: { kind: 3, tracks: 80, sectors: () => 40 },
};

const blockNumber = (format, track, sector) => {
  let blocks = sector;
  for (let t = 1; t < track; t++) blocks += format.sectors(t);
  return blocks;
};

const totalBlocks = (format) => blockNumber(format, format.tracks + 1, 0);

// Every byte says which block it is in: the block number's two bytes, then a counter
const sectorBytes = (number) => Buffer.from(Array.from({ length: SECTOR }, (_, i) =>
  i === 0 ? number & 0xff : i === 1 ? (number >> 8) & 0xff : (i + number) & 0xff));

function makeImage(format) {
  return Buffer.concat(Array.from({ length: totalBlocks(format) }, (_, n) => sectorBytes(n)));
}

const hex = (n) => n.toString(16);
const write = (offset, value) => `w ${hex(offset)} ${hex(value)}\n`;
const read = (offset) => `r ${hex(offset)}\n`;

// A command as the driver sends it: operands, token, then CMD
const command = (cmd, { addr0 = 0, addr1 = 0, page = 0, token = 1 } = {}) =>
  write(ADDR0, addr0) + write(ADDR1, addr1) + write(PAGE, page) + write(TOKEN, token) + write(CMD, cmd);

const readBlock = (addr0, addr1, token = 1) =>
  command(READ, { addr0, addr1, token }) + 'svc\n' +
  Array.from({ length: PAGES }, (_, p) => write(PAGE, p) + Array.from({ length: WINDOW }, (_, i) => read(WINDOW_AT + i)).join('')).join('');

const fillWindow = (bytes) => Array.from({ length: PAGES }, (_, p) =>
  write(PAGE, p) + Array.from({ length: WINDOW }, (_, i) => write(WINDOW_AT + i, bytes[p * WINDOW + i])).join('')).join('');

let harnessPath;
function harness() {
  if (harnessPath) return harnessPath;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'trblock-'));
  harnessPath = path.join(dir, 'harness');
  const built = spawnSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-o', harnessPath, HARNESS_SOURCE], { encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return harnessPath;
}

function run(kind, script, mode) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'trblock-run-'));
  const image = path.join(dir, 'in.img'), out = path.join(dir, 'out.img');
  fs.writeFileSync(image, makeImage(FORMATS[kind]));
  const ran = spawnSync(harness(), [image, out, ...(mode ? [mode] : [])], { input: script, encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  const lines = ran.stdout.split('\n').filter(Boolean);
  return { lines, image: fs.readFileSync(out), original: fs.readFileSync(image) };
}

const bytesOf = (lines) => Buffer.from(lines.map((l) => parseInt(l, 16)));
const statusOf = (script, kind = 'd81', mode) => run(kind, script + read(STATUS), mode).lines.at(-1);

test('the id is TRB at 0x10, and nothing else answers outside the registers and window', { skip }, () => {
  const { lines } = run('d81', [0x10, 0x11, 0x12, 0x13, 0x0f, 0x08, 0x09, 0x0b, 0x0c, 0x3f, 0x80].map(read).join(''));
  assert.deepEqual(lines, ['54', '52', '42', 'open', 'open', 'open', 'open', 'open', 'open', 'open', 'open']);
});

test('status reads 00 before any command', { skip }, () => {
  assert.equal(run('d81', read(STATUS) + read(ECHO)).lines.join(), '00,00');
});

test('status is busy from the CMD write until the main loop services it, then done with the token', { skip }, () => {
  const { lines } = run('d81', command(READ, { addr0: 40, addr1: 0, token: 0x37 }) + read(STATUS) + read(ECHO) + 'svc\n' + read(STATUS) + read(ECHO) + 'svc\n');
  assert.deepEqual(lines, ['80', '00', '1', '5a', '37', '0']);
});

for (const [kind, format] of Object.entries(FORMATS)) {
  test(`${kind}: every track's first and last sector read back as that block, in four pages`, { skip }, () => {
    for (let track = 1; track <= format.tracks; track++) {
      for (const sector of [0, format.sectors(track) - 1]) {
        const { lines } = run(kind, readBlock(track, sector));
        const got = bytesOf(lines.slice(1));
        assert.deepEqual(got, sectorBytes(blockNumber(format, track, sector)), `${kind} ${track}/${sector}`);
      }
    }
  });

  test(`${kind}: the sector after a track's last, track 0 and the track past the end fail with bad address`, { skip }, () => {
    const cases = [[0, 0], [format.tracks + 1, 0], [1, format.sectors(1)], [format.tracks, format.sectors(format.tracks)], [255, 255]];
    for (const [track, sector] of cases) {
      assert.equal(statusOf(command(READ, { addr0: track, addr1: sector }) + 'svc\n', kind), hex(BAD_ADDRESS), `${track}/${sector}`);
    }
  });

  test(`${kind}: info says the kind, that it is writable and the block count`, { skip }, () => {
    const blocks = totalBlocks(format);
    const { lines } = run(kind, command(INFO) + 'svc\n' + Array.from({ length: 5 }, (_, i) => read(WINDOW_AT + i)).join(''));
    assert.deepEqual(bytesOf(lines.slice(1)), Buffer.from([format.kind, 1, blocks & 0xff, (blocks >> 8) & 0xff, blocks >> 16]));
  });
}

test('a write reaches exactly its block in the image and leaves every other byte alone', { skip }, () => {
  const format = FORMATS.d81;
  const payload = Buffer.from(Array.from({ length: SECTOR }, (_, i) => (i * 7 + 3) & 0xff));
  const { image, original } = run('d81', fillWindow(payload) + command(WRITE, { addr0: 79, addr1: 5 }) + 'svc\n');
  const at = blockNumber(format, 79, 5) * SECTOR;
  assert.deepEqual(image.subarray(at, at + SECTOR), payload);
  assert.deepEqual(Buffer.concat([image.subarray(0, at), image.subarray(at + SECTOR)]),
                   Buffer.concat([original.subarray(0, at), original.subarray(at + SECTOR)]));
});

test('a write then a read of the same block returns what was written, whatever came before', { skip }, () => {
  const payload = Buffer.from(Array.from({ length: SECTOR }, (_, i) => 255 - i));
  const { lines } = run('d64', fillWindow(payload) + command(WRITE, { addr0: 18, addr1: 1 }) + 'svc\n' + readBlock(18, 1, 2));
  assert.deepEqual(bytesOf(lines.slice(2)), payload);
});

test('a failed write changes nothing in the image', { skip }, () => {
  const payload = Buffer.alloc(SECTOR, 0xee);
  const bad = run('d64', fillWindow(payload) + command(WRITE, { addr0: 36, addr1: 0 }) + 'svc\n');
  assert.deepEqual(bad.image, bad.original);
  const readOnly = run('d81', fillWindow(payload) + command(WRITE, { addr0: 1, addr1: 0 }) + 'svc\n' + read(STATUS), 'ro');
  assert.equal(readOnly.lines.at(-1), hex(READ_ONLY));
  assert.deepEqual(readOnly.image, readOnly.original);
});

test('a read-only image still reads, and info clears the writable flag', { skip }, () => {
  const { lines } = run('d81', command(INFO) + 'svc\n' + read(WINDOW_AT + 1) + readBlock(1, 0, 2).split('\n').slice(0, 2).join('\n') + '\n' + read(STATUS), 'ro');
  assert.equal(lines[1], '00');
  assert.equal(lines.at(-1), '5a');
});

test('with no image mounted every command fails with no image, and status never claims done', { skip }, () => {
  for (const cmd of [READ, WRITE, FLUSH, INFO]) {
    assert.equal(statusOf(command(cmd, { addr0: 1 }) + 'svc\n', 'd81', 'nobackend'), hex(NO_IMAGE), `cmd ${cmd}`);
  }
});

test('an image that is no known size mounts as no image rather than being guessed at', { skip }, () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'trblock-odd-'));
  const image = path.join(dir, 'in.img'), out = path.join(dir, 'out.img');
  fs.writeFileSync(image, makeImage(FORMATS.d64).subarray(0, SECTOR * 100));
  for (const cmd of [READ, WRITE, INFO]) {
    const ran = spawnSync(harness(), [image, out], { input: command(cmd, { addr0: 1 }) + 'svc\n' + read(STATUS), encoding: 'utf8' });
    assert.equal(ran.stdout.trim().split('\n').at(-1), hex(NO_IMAGE), `cmd ${cmd}`);
  }
});

test('a command that is not one fails with bad command, and so does zero', { skip }, () => {
  for (const cmd of [0, 5, 0x80, 0xff]) assert.equal(statusOf(command(cmd) + 'svc\n'), hex(BAD_COMMAND), `cmd ${cmd}`);
});

test('flush reaches the backend once per command and reports done', { skip }, () => {
  const { lines } = run('d81', command(FLUSH) + 'svc\n' + read(STATUS) + 'flushes\n' + command(FLUSH, { token: 2 }) + 'svc\nflushes\n');
  assert.deepEqual(lines, ['1', '5a', '1', '1', '2']);
});

test('operands are latched when CMD is written: changing them while the command waits changes nothing', { skip }, () => {
  const start = command(READ, { addr0: 40, addr1: 3, token: 9 });
  const { lines } = run('d81', start + write(ADDR0, 1) + write(ADDR1, 1) + write(TOKEN, 0x44) + 'svc\n' + read(ECHO) + read(WINDOW_AT));
  assert.equal(lines[1], '09');
  assert.equal(parseInt(lines[2], 16), blockNumber(FORMATS.d81, 40, 3) & 0xff);
});

test('a second CMD while one is busy is ignored, and window writes while busy are ignored', { skip }, () => {
  const { lines } = run('d81', command(READ, { addr0: 40, addr1: 3, token: 5 }) + command(READ, { addr0: 1, addr1: 0, token: 6 }) +
                              write(WINDOW_AT, 0x99) + 'svc\nsvc\n' + read(ECHO) + read(WINDOW_AT));
  assert.deepEqual(lines.slice(0, 2), ['1', '0']);
  assert.equal(lines[2], '05');
  assert.equal(parseInt(lines[3], 16), blockNumber(FORMATS.d81, 40, 3) & 0xff);
});

test('a window write while a write command is busy does not change what that command writes', { skip }, () => {
  const payload = Buffer.alloc(SECTOR, 0x11);
  const { image } = run('d81', fillWindow(payload) + command(WRITE, { addr0: 2, addr1: 0 }) + write(PAGE, 0) + write(WINDOW_AT, 0x99) + 'svc\n');
  const at = blockNumber(FORMATS.d81, 2, 0) * SECTOR;
  assert.equal(image[at], 0x11);
});

test('the page register uses only its low two bits', { skip }, () => {
  const { lines } = run('d81', command(READ, { addr0: 1, addr1: 0 }) + 'svc\n' + write(PAGE, 0) + read(WINDOW_AT) + write(PAGE, 4) + read(WINDOW_AT) + write(PAGE, 5) + read(WINDOW_AT));
  assert.equal(lines[1], lines[2]);
  assert.notEqual(lines[1], lines[3]);
});
