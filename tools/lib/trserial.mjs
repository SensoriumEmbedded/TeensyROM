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

// The Teensyduino tool that knows which USB devices are Teensys. Optional: we
// fall back to scanning /dev when it is not installed.
function teensyPortsTool() {
  const base = path.join(process.env.HOME ?? '', 'Library/Arduino15/packages/teensy/tools/teensy-tools');
  if (!fs.existsSync(base)) return null;
  for (const version of fs.readdirSync(base).sort().reverse()) {
    const tool = path.join(base, version, 'teensy_ports');
    if (fs.existsSync(tool)) return tool;
  }
  return null;
}

// Returns { port, bootloader } for the attached board, or null when none is
// found. `bootloader` means it is sitting in HalfKay with no serial device.
export function findBoard() {
  const tool = teensyPortsTool();
  if (tool) {
    let listing = '';
    try { listing = execFileSync(tool, ['-L'], { encoding: 'utf8', timeout: 10000 }); } catch { listing = ''; }
    for (const line of listing.split('\n')) {
      if (/Bootloader/i.test(line)) return { port: null, bootloader: true };
      const match = line.match(/(\/dev\/\S+)/);
      if (match) return { port: match[1], bootloader: false };
    }
  }
  const devices = fs.existsSync('/dev')
    ? fs.readdirSync('/dev').filter((name) => name.startsWith('cu.usbmodem')).sort()
    : [];
  return devices.length ? { port: `/dev/${devices[0]}`, bootloader: false } : null;
}

// Writes `send` to the port, then collects whatever arrives until `ms` elapses.
// Returns the raw bytes as a latin1 string; '' means the board said nothing.
export function exchange(port, send, ms = 3000) {
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

// TeensyROM answers 'dv' (VersionInfoToken) with a banner that names the build.
// Returns { version, built, raw } with nulls for anything that did not appear.
export function identify(port, ms = 3000) {
  const raw = exchange(port, '\x64\x76', ms);
  const version = raw.match(/TeensyROM\+? v[\d.]+/)?.[0] ?? null;
  const built = raw.match(/([A-Z][a-z]{2} [ \d]\d \d{4}), (\d\d:\d\d:\d\d)/);
  return { version, built: built ? `${built[1]}, ${built[2]}` : null, raw };
}

// Which cartridge a banner belongs to. The '+' is the whole distinction, and
// it is the thing that decides whether a given hex may be written at all.
export function boardKind(version) {
  if (!version) return null;
  return version.includes('TeensyROM+') ? 'tr-plus' : 'tr';
}
