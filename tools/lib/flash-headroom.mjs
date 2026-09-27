// Port of Source/Teensy/tools/Test-FlashHeadroom.ps1.
//
// FlashUpdate.ino / FlashTxx.c update the running firmware by copying the new image into
// a scratch buffer carved out of whatever flash is free above the currently-installed
// firmware, verifying it, then copying it over the live code (see FlashTxx.c
// firmware_buffer_init()). That scratch buffer always has to hold one full spare copy of
// the firmware next to the copy already running, so the long-term-safe ceiling -- assuming
// future builds stay roughly the size of the one currently installed -- is half of the
// flash left after FLASH_RESERVE is set aside at the top:
//
//     usable             = FLASH_SIZE - FLASH_RESERVE
//     steady-state limit = usable / 2
//
// FLASH_SIZE and FLASH_RESERVE_STOCK are read directly from FlashTxx.h and FlashUpdate.ino,
// so this check tracks those constants automatically if they ever change. A TR+ built with
// the extension loader reserves the extension host slot as well, which FlashUpdate.ino
// derives from the slot's size and static_asserts against its address; the slot's size
// comes from hex.mjs here, which verify-extensions.mjs holds to VMHostABI.h.
import fs from 'node:fs';
import { VM_BASE, VM_LIMIT } from './hex.mjs';

// A file that is there but no longer declares the name is refused rather than answered with
// the fallback: that is what a rename looks like, and the fallback would go on checking
// against a reserve the firmware no longer has.
function readDefine(filePath, name, fallback) {
  if (!fs.existsSync(filePath)) return fallback;
  const text = fs.readFileSync(filePath, 'utf8');
  const match = text.match(new RegExp(`^\\s*#define\\s+${name}\\s+\\(?(0x[0-9A-Fa-f]+|\\d+)\\)?`, 'm'));
  if (!match) throw new Error(`${filePath} no longer #defines ${name} as a number`);
  return match[1].startsWith('0x') ? parseInt(match[1], 16) : parseInt(match[1], 10);
}

// Parses the hex file the same way FXUtil.cpp's process_hex_record() does (tracking type
// 02/04 base-address records) to get the true flashed-image span, not just the on-disk
// text size of the .hex file.
function hexImageExtent(hexPath) {
  let base = 0n, min = 0xFFFFFFFFn, max = 0n, lines = 0;
  const text = fs.readFileSync(hexPath, 'utf8');
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line.startsWith(':')) continue;
    const byteCount = parseInt(line.slice(1, 3), 16);
    const addr = parseInt(line.slice(3, 7), 16);
    const recType = parseInt(line.slice(7, 9), 16);
    const data = line.slice(9, 9 + byteCount * 2);
    lines++;
    if (recType === 0) {
      const a = base + BigInt(addr);
      if (a < min) min = a;
      const end = a + BigInt(byteCount);
      if (end > max) max = end;
    } else if (recType === 1) {
      break; // EOF record
    } else if (recType === 2) {
      base = BigInt(parseInt(data, 16)) << 4n; // extended segment address
    } else if (recType === 4) {
      base = BigInt(parseInt(data, 16)) << 16n; // extended linear address
    }
  }
  return { lines, min, max, size: max - min };
}

// root: repo root. hexPath: the combined "_full.hex" to check. extensions: the hex was built
// with the extension loader, whose updater also reserves the host slot.
export function checkFlashHeadroom(root, hexPath, { extensions = false } = {}) {
  const flashTxxH = `${root}/Source/Teensy/Flash/FlashTxx.h`;
  const flashUpdateIno = `${root}/Source/Teensy/FlashUpdate.ino`;

  const flashSize = readDefine(flashTxxH, 'FLASH_SIZE', 0x800000);
  const flashReserve = readDefine(flashUpdateIno, 'FLASH_RESERVE_STOCK', 0x40000) +
    (extensions ? VM_LIMIT - VM_BASE : 0);
  const usable = flashSize - flashReserve;
  const steadyStateMax = Math.floor(usable / 2);

  const extent = hexImageExtent(hexPath);
  const sizeBytes = Number(extent.size);
  const headroomBytes = steadyStateMax - sizeBytes;

  const status = sizeBytes > steadyStateMax ? 'FAIL' : headroomBytes < steadyStateMax * 0.1 ? 'WARN' : 'OK';

  return {
    status,
    hexPath,
    flashSize,
    flashReserve,
    usable,
    steadyStateMax,
    sizeBytes,
    headroomBytes,
    min: extent.min,
    max: extent.max,
    lines: extent.lines,
  };
}

export function formatFlashHeadroom(result) {
  const kb = (n) => (n / 1024).toFixed(1);
  const lines = [
    '=== Flash self-update headroom check ===',
    `  File:  ${result.hexPath}`,
    `  Image: ${result.min.toString(16).padStart(8, '0')} - ${result.max.toString(16).padStart(8, '0')}  (${kb(result.sizeBytes)} K, ${result.lines} lines)`,
    `  Flash: ${kb(result.flashSize)} K total, ${kb(result.flashReserve)} K reserved -> ${kb(result.usable)} K usable`,
    `  Steady-state self-update ceiling: ${kb(result.steadyStateMax)} K (usable / 2)`,
  ];
  if (result.status === 'FAIL') {
    lines.push(`  FAIL: image is ${kb(-result.headroomBytes)} K OVER the steady-state ceiling (${kb(result.sizeBytes)} K vs ${kb(result.steadyStateMax)} K limit)`);
    lines.push('        FlashUpdate.ino\'s scratch buffer may no longer fit next to the running firmware.');
  } else if (result.status === 'WARN') {
    lines.push(`  WARN: only ${kb(result.headroomBytes)} K of headroom left before the self-update ceiling`);
  } else {
    lines.push(`  OK: ${kb(result.headroomBytes)} K of headroom left before the self-update ceiling`);
  }
  return lines.join('\n');
}
