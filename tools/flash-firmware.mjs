#!/usr/bin/env node
// SPDX-License-Identifier: MIT
//
// Flash a built firmware image to an attached TeensyROM over USB.
//
//   node tools/flash-firmware.mjs                     # newest hex in build/firmware
//   node tools/flash-firmware.mjs --hex <path>
//   node tools/flash-firmware.mjs --check             # identify only, write nothing
//
// The point of this tool is the check it does BEFORE writing: a TR and a TR+
// image are both valid, both build cleanly, and are not interchangeable. The
// running firmware refuses a mismatched image at its own updater with
// "Verify file for TeensyROM+: Failed!", but only after you have carried the
// file to an SD card. Here we compare the target string compiled into the hex
// against the banner the attached board reports, and refuse early.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync, execFileSync } from 'node:child_process';
import { decodeHex } from './lib/hex.mjs';
import { findBoard, identify, boardKind } from './lib/trserial.mjs';

// Refusals here are expected outcomes (wrong target, no board, no loader), so
// report them as a message and an exit code rather than a stack trace.
function fail(message) { console.error(`\n${message}`); process.exit(1); }

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv.slice(2);
const flag = (name) => argv.includes(name);
const value = (name) => { const i = argv.indexOf(name); return i < 0 ? null : argv[i + 1]; };

// The strings FlashUpdate.ino compiles in, and which cartridge each belongs to.
const TARGET_IDS = [
  ['fw_t41_teensyromplus_sensorium', 'tr-plus'],
  ['fw_t41_teensyrom_sensorium_v3', 'tr'],
  ['fw_t41_teensyrom_sensorium', 'tr'],      // fab 0.2x, checked last: it is a prefix of v3
];
const LABEL = { 'tr': 'TeensyROM', 'tr-plus': 'TeensyROM+' };

function newestHex() {
  const dir = path.join(root, 'build/firmware');
  if (!fs.existsSync(dir)) return null;
  const hexes = fs.readdirSync(dir).filter((name) => name.endsWith('.hex'))
    .map((name) => path.join(dir, name))
    .sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs);
  return hexes[0] ?? null;
}

// Flattens the hex to its bytes and reports which cartridge it is built for,
// plus the flash regions it covers (a three-image build has a gap, which is
// expected and is why this prints regions rather than one span).
function inspectHex(file) {
  const bytes = decodeHex(fs.readFileSync(file, 'utf8'));
  const addresses = [...bytes.keys()].sort((a, b) => a - b);
  const text = Buffer.from(addresses.map((a) => bytes.get(a))).toString('latin1');
  const target = TARGET_IDS.find(([id]) => text.includes(id));

  const regions = [];
  let start = addresses[0], previous = addresses[0];
  for (const address of addresses.slice(1)) {
    if (address !== previous + 1) { regions.push([start, previous + 1]); start = address; }
    previous = address;
  }
  regions.push([start, previous + 1]);
  return { kind: target?.[1] ?? null, id: target?.[0] ?? null, regions, bytes: addresses.length };
}

const hexPath = value('--hex') ?? newestHex();
if (!hexPath) fail('No hex found in build/firmware — build one first, or pass --hex <path>.');
if (!fs.existsSync(hexPath)) fail(`No such hex: ${hexPath}`);

const image = inspectHex(hexPath);
console.log(`Image: ${path.relative(root, hexPath)}`);
if (!image.kind) {
  fail('This hex carries no TeensyROM target ID. It is not a TR firmware image; refusing to write it.');
}
console.log(`  built for ${LABEL[image.kind]}  (${image.id})`);
for (const [from, to] of image.regions) {
  console.log(`  0x${from.toString(16).padStart(8, '0')} .. 0x${to.toString(16).padStart(8, '0')}` +
    `   ${((to - from) / 1024).toFixed(1)}K`);
}

const board = findBoard();
if (!board) fail('No Teensy found on USB. Is the cartridge plugged in?');

if (board.bootloader) {
  // Already in HalfKay: nothing can be asked of it, so the image's own target
  // is all we have to go on. The board is waiting, so this is still safe.
  console.log('\nBoard: in HalfKay bootloader (cannot be identified until it runs again)');
} else {
  const info = identify(board.port);
  const kind = boardKind(info.version);
  console.log(`\nBoard: ${board.port}`);
  console.log(`  running ${info.version ?? 'unidentified'}${info.built ? `, built ${info.built}` : ''}`);
  if (!kind) {
    console.log('  WARNING: no banner came back, so the target could not be confirmed.');
  } else if (kind !== image.kind) {
    fail(`Refusing to write: this is a ${LABEL[kind]} board and that hex is built for ${LABEL[image.kind]}.\n` +
      `Rebuild with --target ${kind}, or pass the right --hex.`);
  } else {
    console.log(`  target matches the image (${LABEL[kind]})`);
  }
}

if (flag('--check')) { console.log('\n--check: nothing written.'); process.exit(0); }

let loader;
try { loader = execFileSync('command', ['-v', 'teensy_loader_cli'], { shell: true, encoding: 'utf8' }).trim(); }
catch { loader = ''; }
if (!loader) {
  fail('teensy_loader_cli is not installed.\n' +
    '  macOS:  brew install teensy_loader_cli\n' +
    '  Linux:  https://www.pjrc.com/teensy/loader_cli.html');
}

// -w waits for the board to appear in HalfKay. TeensyROM exposes no HID
// rebootor, so -s (soft reboot) does not work on it and the button is the
// reliable way in; teensy_reboot is the alternative but needs the GUI loader.
console.log('\nWriting. Press the program button on the Teensy if it does not start within a few seconds.');
const write = spawnSync(loader, ['--mcu=TEENSY41', '-w', '-v', hexPath], { stdio: 'inherit' });
if (write.status !== 0) fail(`teensy_loader_cli exited ${write.status}`);

// Confirm what is actually running, rather than trusting that the write took.
// The build timestamp is the discriminator: TRVersion often does not change
// between builds, so a matching version number alone proves nothing.
const after = findBoard();
if (!after?.port) {
  console.log('\nWritten. Board has not re-enumerated yet — re-run with --check to confirm.');
  process.exit(0);
}
const info = identify(after.port, 5000);
console.log(`\nWritten. Now running ${info.version ?? 'unidentified'}${info.built ? `, built ${info.built}` : ''}.`);
console.log('Note: a USB write full-erases flash, so the emulated EEPROM (TR settings) is reset.');
