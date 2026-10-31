// SPDX-License-Identifier: MIT
//
// The MCP tools. Each call opens its own connection and closes it, so other
// TeensyROM apps can reach the board between calls: it takes one TCP client
// at a time, and a serial port opens in one place only.

import fs from 'node:fs/promises';

import { boardKind, parseBanner } from './banner.mjs';
import { CHARSETS, CIA2_PORT_A, SCREEN_BYTES, VIC_MEMPTR, charsetIn, screenAddress, screenRows } from './screen.mjs';
import { driveNumber } from './trlink.mjs';

const DRIVE_PROPERTY = {
  type: 'string',
  enum: ['sd', 'usb', 'teensy'],
  default: 'sd',
  description: "Where the file is: 'sd' (SD card), 'usb' (USB drive) or 'teensy' (the built-in menu).",
};
const DEFAULT_TAKE = 200;
const MAX_TAKE = 1000;

const NOTHING_ANSWERED = 'Nothing answered the firmware check. An extension image runs with USB off and '
  + 'answers nothing; otherwise the board is hung or not connected.';

function describeStatus(image, banner) {
  const lines = [`image: ${image}`];
  const fields = {
    firmware: banner.version,
    built: banner.built,
    board: boardKind(banner.version),
    machine: banner.machine && `${banner.machine} ${banner.video}`,
    menu: banner.boot,
  };
  for (const [name, value] of Object.entries(fields)) if (value) lines.push(`${name}: ${value}`);
  lines.push(`dma: ${image === 'main' && boardKind(banner.version) === 'tr-plus' ? 'available' : 'not available'}`);
  return lines.join('\n');
}

function describeEntry(entry) {
  return entry.type === 'dir' ? `${entry.name}/` : `${entry.name}  (${entry.size} bytes)`;
}

async function status(link) {
  const image = await link.fwCheck();
  if (!image) return NOTHING_ANSWERED;
  return describeStatus(image, parseBanner(await link.version()));
}

async function listDir(link, { path = '/', drive = 'sd', skip = 0, take = DEFAULT_TAKE }) {
  const count = Math.min(take, MAX_TAKE);
  const entries = await link.listDirectory(path, driveNumber(drive), { skip, take: count });
  if (entries.length === 0) return `${path} is empty${skip ? ` past entry ${skip}` : ''}`;
  const more = entries.length === count ? `\n(first ${count} from entry ${skip}; ask again with skip=${skip + count})` : '';
  return entries.map(describeEntry).join('\n') + more;
}

async function readScreen(link) {
  const [memptr] = await link.readMemory(VIC_MEMPTR, 1);
  const [cia2PortA] = await link.readMemory(CIA2_PORT_A, 1);
  const address = screenAddress(memptr, cia2PortA);
  const charset = charsetIn(memptr, cia2PortA);
  const rows = screenRows(await link.readMemory(address, SCREEN_BYTES), charset);
  const note = charset === CHARSETS.custom ? ' (decoded as uppercase/graphics)' : '';
  const header = `screen at $${address.toString(16).toUpperCase()}, charset ${charset}${note}`;
  return [header, ...rows.map((row) => row.trimEnd())].join('\n');
}

async function pushFile(link, { local_path: localPath, remote_path: remotePath, drive = 'sd' }, readFile) {
  const data = await readFile(localPath);
  const number = driveNumber(drive);
  // The firmware will not overwrite a file, and a missing one fails the delete.
  await link.deleteFile(remotePath, number).catch(() => {});
  await link.postFile(data, remotePath, number);
  return `pushed ${localPath} to ${drive}:${remotePath} (${data.length} bytes)`;
}

async function launch(link, { path, drive = 'sd' }) {
  await link.launch(path, driveNumber(drive));
  return `launched ${drive}:${path}`;
}

async function reset(link) {
  return (await link.reset()) || 'reset sent';
}

const DEFINITIONS = [
  {
    name: 'status',
    description: 'Which TeensyROM image is running (main, minimal, or none answering), its firmware version, '
      + 'board type (tr or tr-plus), the host machine (C64/C128, NTSC/PAL), whether the menu has finished '
      + "booting, and whether DMA memory access is available. Call this first, and after 'reset' until "
      + "'menu: complete'.",
    inputSchema: { type: 'object', properties: {} },
    annotations: { readOnlyHint: true, openWorldHint: true },
    run: status,
  },
  {
    name: 'list_dir',
    description: "List a directory on the TeensyROM's SD card, USB drive or built-in menu. Needs the main "
      + 'image with the C64 at the TeensyROM menu; otherwise the board answers Busy!.',
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', default: '/', description: 'Directory to list, starting with /.' },
        drive: DRIVE_PROPERTY,
        skip: { type: 'integer', minimum: 0, default: 0, description: 'Entries to skip, for paging.' },
        take: { type: 'integer', minimum: 1, maximum: MAX_TAKE, default: DEFAULT_TAKE },
      },
    },
    annotations: { readOnlyHint: true, openWorldHint: true },
    run: listDir,
  },
  {
    name: 'read_screen',
    description: "The C64's 40-column text screen as 25 lines of text, read by DMA. Needs a TR+ running "
      + 'its main image. Graphics characters read as ".". It does not read the C128 80-column screen. '
      + 'Avoid it while a C128 runs at 2 MHz (GEOS 128 in 80 columns): a DMA read can hang the machine.',
    inputSchema: { type: 'object', properties: {} },
    annotations: { readOnlyHint: true, openWorldHint: true },
    run: readScreen,
  },
  {
    name: 'push_file',
    description: "Copy a file from this computer to the TeensyROM's SD card or USB drive, replacing any file "
      + 'already at that path. Missing directories are created. Needs the main image with the C64 at the menu.',
    inputSchema: {
      type: 'object',
      properties: {
        local_path: { type: 'string', description: 'Path of the file on this computer.' },
        remote_path: { type: 'string', description: 'Destination path on the cartridge, starting with /.' },
        drive: { ...DRIVE_PROPERTY, enum: ['sd', 'usb'] },
      },
      required: ['local_path', 'remote_path'],
    },
    annotations: { destructiveHint: true, idempotentHint: true, openWorldHint: true },
    run: pushFile,
  },
  {
    name: 'launch',
    description: 'Launch a file (PRG, CRT, SID, D64 and the other types the menu runs) on the C64, from the '
      + "SD card, USB drive or built-in menu. Ends whatever the C64 was running. A large CRT hands over to the "
      + "minimal image, which answers only status, launch and reset.",
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', description: 'Path of the file, starting with /.' },
        drive: DRIVE_PROPERTY,
      },
      required: ['path'],
    },
    annotations: { destructiveHint: false, openWorldHint: true },
    run: launch,
  },
  {
    name: 'reset',
    description: "Reset the C64 to the TeensyROM menu, ending whatever it was running, and bring the board "
      + "back to its main image. The menu takes from under a second to tens of seconds to boot: call 'status' "
      + "until it reports 'menu: complete' before using the other tools.",
    inputSchema: { type: 'object', properties: {} },
    annotations: { destructiveHint: false, openWorldHint: true },
    run: reset,
  },
];

export function createTools({ connect, readFile = fs.readFile }) {
  return DEFINITIONS.map((definition) => ({
    ...definition,
    run: async (args) => {
      const link = await connect();
      try {
        return await definition.run(link, args, readFile);
      } finally {
        link.close();
      }
    },
  }));
}
