// SPDX-License-Identifier: MIT
//
// Minimal serial access to an attached TeensyROM, with no dependencies.
//
// Node has no portable way to read a character device with a deadline, so each
// exchange runs in a short-lived child process that the parent kills on
// timeout. That also means a wedged board can never hang the caller.
import { spawnSync, execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

import { defaultArduinoDataDir } from './toolchain.mjs';

// A Teensyduino tool from the newest installed teensy-tools, or null when it is
// not installed. Windows builds carry an .exe suffix.
export function teensyTool(name) {
  const base = path.join(defaultArduinoDataDir(), 'packages/teensy/tools/teensy-tools');
  if (!fs.existsSync(base)) return null;
  const file = process.platform === 'win32' ? `${name}.exe` : name;
  for (const version of fs.readdirSync(base).sort().reverse()) {
    const tool = path.join(base, version, file);
    if (fs.existsSync(tool)) return tool;
  }
  return null;
}

// The Teensyduino tool that knows which USB devices are Teensys. Optional: we
// fall back to scanning /dev when it is not installed.
function teensyPortsTool() {
  return teensyTool('teensy_ports');
}

// Every attached board, in teensy_ports order, as { port, bootloader, location,
// label }. `bootloader` means it is sitting in HalfKay with no serial device, so
// `port` is null. `location` is its USB location (usb:...) and `label` the rest
// of its teensy_ports line; together they are what teensy_post_compile needs to
// pick one board out of several. Both are null on the /dev fallback.
export function findBoards() {
  const tool = teensyPortsTool();
  if (tool) {
    let listing = '';
    try { listing = execFileSync(tool, ['-L'], { encoding: 'utf8', timeout: 10000 }); } catch { listing = ''; }
    const boards = parsePortListing(listing);
    if (boards.length) return boards;
  }
  // macOS names the CDC device cu.usbmodem*; Linux names it ttyACM*.
  const devices = fs.existsSync('/dev')
    ? fs.readdirSync('/dev').filter((name) => name.startsWith('cu.usbmodem') || name.startsWith('ttyACM')).sort()
    : [];
  return devices.map((name) => ({ port: `/dev/${name}`, bootloader: false, location: null, label: null }));
}

// One board per line of `teensy_ports -L`, e.g.
//   usb:80000/1/0/5/3/3 COM12 (Teensy 4.1) Serial+MIDI
// macOS and Linux list a /dev path where Windows lists a COM port.
export function parsePortListing(listing) {
  const boards = [];
  for (const line of listing.split(/\r?\n/).map((l) => l.trim()).filter(Boolean)) {
    const location = line.split(/\s+/)[0];
    const label = line.slice(location.length).trim();
    if (/Bootloader/i.test(line)) { boards.push({ port: null, bootloader: true, location, label }); continue; }
    const match = line.match(/(\/dev\/\S+)/) ?? line.match(/\b(COM\d+)\b/);
    if (match) boards.push({ port: match[1], bootloader: false, location, label });
  }
  return boards;
}

// Writes `send` to the port, then collects whatever arrives until `ms` elapses.
// Returns the raw bytes as a latin1 string; '' means the board said nothing.
export function exchange(port, send, ms = 3000) {
  if (process.platform === 'win32') return exchangeWindows(port, send, ms);
  const child = `
    import fs from 'node:fs';
    const fd = fs.openSync(${JSON.stringify(port)}, 'r+');
    const send = Buffer.from(${JSON.stringify(send ?? '')}, 'latin1');
    if (send.length) { setTimeout(() => fs.writeSync(fd, send), 250); }
    const stream = fs.createReadStream(null, { fd });
    const chunks = [];
    stream.on('data', (chunk) => chunks.push(chunk));
    stream.on('error', () => {});
    setTimeout(() => {
      process.stdout.write(Buffer.concat(chunks).toString('latin1'));
      process.exit(0);
    }, ${Math.max(ms, 300)});
  `;
  // `stty raw` keeps the tty from swallowing or translating bytes; the board
  // ignores the baud rate, it is a USB CDC device. BSD stty names the device
  // with -f and GNU coreutils with -F, and either can be first on PATH.
  for (const select of ['-f', '-F']) {
    try { execFileSync('stty', [select, port, 'raw', '115200', '-echo'], { timeout: 5000, stdio: 'ignore' }); break; }
    catch { /* try the other spelling */ }
  }
  const result = spawnSync(process.execPath, ['--input-type=module', '-e', child],
    { encoding: 'utf8', timeout: ms + 8000 });
  return result.stdout ?? '';
}

// Windows has no stty, but the COM port opens as a file (\\.\COMn) and the
// same child-and-deadline approach works, with two changes the Windows serial
// driver forces. The request goes out before anything reads: a synchronous
// read waiting on the handle holds up a write queued behind it. And the child
// reads one byte at a time and passes each straight to stdout, because a read
// blocks until a byte comes and the child only ever ends by being killed, so
// whatever it has passed on by then is the answer.
function exchangeWindows(port, send, ms) {
  if (!/^COM\d+$/i.test(port)) return '';
  const child = `
    import fs from 'node:fs';
    const fd = fs.openSync(${JSON.stringify(`\\\\.\\${port}`)}, 'r+');
    const send = Buffer.from(${JSON.stringify(send ?? '')}, 'latin1');
    if (send.length) fs.writeSync(fd, send);
    const one = Buffer.alloc(1);
    const nap = new Int32Array(new SharedArrayBuffer(4));
    for (;;) {
      // A previous user of the port can leave its read timeouts set so a read
      // returns nothing at once instead of waiting; nap then, rather than spin.
      if (fs.readSync(fd, one, 0, 1, null) === 1) fs.writeSync(1, one);
      else Atomics.wait(nap, 0, 0, 5);
    }`;
  // The allowance on top of ms covers the child's own start-up, so the port is
  // listened to for about as long as on POSIX.
  const result = spawnSync(process.execPath, ['--input-type=module', '-e', child],
    { encoding: 'latin1', timeout: Math.max(ms, 300) + 200 });
  return result.stdout ?? '';
}

// TeensyROM answers 'dv' (VersionInfoToken) with a banner that names the build
// and the chip ("UID: 14470230", burned in at the factory, so it tells two
// boards of the same type apart). Returns { version, built, uid, minimal, raw }
// with nulls for anything that did not appear. `minimal` means MinimalBoot answered
// ("FW: TeensyROM v0.8.0.11(minimal)"), as it does for a moment after every
// reset and write before it hands over to the full firmware; the build it
// names is MinimalBoot's own.
export function identify(port, ms = 3000) {
  return parseBanner(exchange(port, '\x64\x76', ms));
}

export function parseBanner(raw) {
  const version = raw.match(/TeensyROM\+? v[\d.]+/)?.[0] ?? null;
  const built = raw.match(/([A-Z][a-z]{2} [ \d]\d \d{4}), (\d\d:\d\d:\d\d)/);
  const uid = raw.match(/UID:\s*(\d+)/)?.[1] ?? null;
  return { version, built: built ? `${built[1]}, ${built[2]}` : null, uid, minimal: /\(minimal\)/.test(raw), raw };
}

// Which cartridge a banner belongs to. The '+' is the whole distinction, and
// it is the thing that decides whether a given hex may be written at all.
export function boardKind(version) {
  if (!version) return null;
  return version.includes('TeensyROM+') ? 'tr-plus' : 'tr';
}
