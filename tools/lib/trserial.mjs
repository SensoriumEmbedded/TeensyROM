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

// Returns { port, bootloader } for the attached board, or null when none is
// found. `bootloader` means it is sitting in HalfKay with no serial device.
// When teensy_ports found it, `location` is its USB location (usb:...), which
// the Windows loader path passes on as -port.
export function findBoard() {
  const tool = teensyPortsTool();
  if (tool) {
    let listing = '';
    try { listing = execFileSync(tool, ['-L'], { encoding: 'utf8', timeout: 10000 }); } catch { listing = ''; }
    for (const line of listing.split('\n')) {
      if (/Bootloader/i.test(line)) return { port: null, bootloader: true };
      // macOS and Linux list a /dev path; Windows lists a COM port.
      const match = line.match(/(\/dev\/\S+)/) ?? line.match(/\b(COM\d+)\b/);
      if (match) return { port: match[1], bootloader: false, location: line.trim().split(/\s+/)[0] };
    }
  }
  // macOS names the CDC device cu.usbmodem*; Linux names it ttyACM*.
  const devices = fs.existsSync('/dev')
    ? fs.readdirSync('/dev').filter((name) => name.startsWith('cu.usbmodem') || name.startsWith('ttyACM')).sort()
    : [];
  return devices.length ? { port: `/dev/${devices[0]}`, bootloader: false } : null;
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

// Windows has no stty, and a COM port cannot be read as a file with a deadline.
// PowerShell's SerialPort can, so the exchange runs there with the same timing
// as above; the bytes travel base64 both ways so no console code page touches them.
function exchangeWindows(port, send, ms) {
  if (!/^COM\d+$/i.test(port)) return '';
  const payload = Buffer.from(send ?? '', 'latin1').toString('base64');
  const script = `
    $sp = New-Object System.IO.Ports.SerialPort '${port}', 115200
    $sp.DtrEnable = $true
    $sp.Open()
    $send = [Convert]::FromBase64String('${payload}')
    Start-Sleep -Milliseconds 250
    if ($send.Length) { $sp.Write($send, 0, $send.Length) }
    Start-Sleep -Milliseconds ${Math.max(ms, 300) - 250}
    $buf = New-Object byte[] $sp.BytesToRead
    [void]$sp.Read($buf, 0, $buf.Length)
    $sp.Close()
    [Convert]::ToBase64String($buf)`;
  const result = spawnSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', script],
    { encoding: 'utf8', timeout: ms + 8000 });
  return Buffer.from((result.stdout ?? '').trim(), 'base64').toString('latin1');
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
