// SPDX-License-Identifier: MIT
//
// The MCP tools. Each returns an object matching its outputSchema. Each call
// opens its own connection and closes it, so other TeensyROM apps can reach
// the board between calls: it takes one TCP client at a time, and a serial
// port opens in one place only.

import fs from 'node:fs/promises';

import { boardKind, parseBanner } from './banner.mjs';
import { CHARSETS, CIA2_PORT_A, SCREEN_BYTES, VIC_MEMPTR, charsetIn, screenAddress, screenRows } from './screen.mjs';
import { driveNumber } from './trlink.mjs';

const DRIVES = ['sd', 'usb', 'teensy'];
const STORAGE_DRIVES = ['sd', 'usb'];
const DRIVE_PROPERTY = {
  type: 'string',
  enum: DRIVES,
  default: 'sd',
  description: "Where the file is: 'sd' (SD card), 'usb' (USB drive) or 'teensy' (the built-in menu).",
};
const DEFAULT_TAKE = 200;
const MAX_TAKE = 1000;

const nullable = (schema) => ({ ...schema, type: [schema.type, 'null'] });
const objectSchema = (properties) => ({
  type: 'object', properties, required: Object.keys(properties), additionalProperties: false,
});
const NO_INPUT = { type: 'object', properties: {} };

const STATUS_OUTPUT = objectSchema({
  image: nullable({ type: 'string', enum: ['main', 'minimal', null],
    description: 'null: nothing answered (an extension image, which runs with USB off, or no board).' }),
  firmware: nullable({ type: 'string', description: "As the board names it, e.g. 'TeensyROM+ v0.9'." }),
  built: nullable({ type: 'string', description: "Build date, e.g. 'Oct 30 2026, 03:16:02'." }),
  board: nullable({ type: 'string', enum: ['tr', 'tr-plus', null] }),
  machine: nullable({ type: 'string', enum: ['C64', 'C128', null] }),
  video: nullable({ type: 'string', enum: ['NTSC', 'PAL', null] }),
  menu: nullable({ type: 'string', enum: ['complete', 'in progress', null],
    description: "Whether the menu has finished booting; null from the minimal image." }),
  dma: { type: 'boolean', description: 'Whether read_screen and other DMA tools can run.' },
});

const ENTRY = {
  type: 'object',
  properties: {
    type: { type: 'string', enum: ['dir', 'file'] },
    name: { type: 'string' },
    size: { type: 'integer', description: 'Bytes; files only.' },
  },
  required: ['type', 'name'],
};
const LIST_DIR_OUTPUT = objectSchema({
  path: { type: 'string' },
  drive: { type: 'string', enum: DRIVES },
  skip: { type: 'integer' },
  entries: { type: 'array', items: ENTRY, description: 'In the order the board lists them.' },
  next_skip: nullable({ type: 'integer', description: 'Pass as skip for the next page; null when this was the last.' }),
});

const READ_SCREEN_OUTPUT = objectSchema({
  address: { type: 'integer', description: 'Where the screen is in C64 memory.' },
  charset: { type: 'string', enum: Object.values(CHARSETS),
    description: "'custom' (a RAM charset) is decoded as upper_graphics." },
  rows: { type: 'array', items: { type: 'string' }, minItems: 25, maxItems: 25,
    description: 'The 25 rows, trailing spaces removed. A character with no text form reads as ".".' },
});

const PUSH_FILE_OUTPUT = objectSchema({
  drive: { type: 'string', enum: STORAGE_DRIVES },
  remote_path: { type: 'string' },
  bytes: { type: 'integer' },
});

const LAUNCH_OUTPUT = objectSchema({
  drive: { type: 'string', enum: DRIVES },
  path: { type: 'string' },
});

const RESET_OUTPUT = objectSchema({});

const NOTHING_ANSWERED = {
  image: null, firmware: null, built: null, board: null, machine: null, video: null, menu: null, dma: false,
};

async function status(link) {
  const image = await link.fwCheck();
  if (!image) return NOTHING_ANSWERED;
  const banner = parseBanner(await link.version());
  const board = boardKind(banner.version);
  return {
    image,
    firmware: banner.version,
    built: banner.built,
    board,
    machine: banner.machine,
    video: banner.video,
    menu: banner.boot,
    dma: image === 'main' && board === 'tr-plus',
  };
}

function entryOf({ type, name, size }) {
  return type === 'file' ? { type, name, size } : { type, name };
}

async function listDir(link, { path = '/', drive = 'sd', skip = 0, take = DEFAULT_TAKE }) {
  const count = Math.min(take, MAX_TAKE);
  const entries = await link.listDirectory(path, driveNumber(drive), { skip, take: count });
  return { path, drive, skip, entries: entries.map(entryOf), next_skip: entries.length === count ? skip + count : null };
}

async function readScreen(link) {
  const [memptr] = await link.readMemory(VIC_MEMPTR, 1);
  const [cia2PortA] = await link.readMemory(CIA2_PORT_A, 1);
  const address = screenAddress(memptr, cia2PortA);
  const charset = charsetIn(memptr, cia2PortA);
  const rows = screenRows(await link.readMemory(address, SCREEN_BYTES), charset);
  return { address, charset, rows: rows.map((row) => row.trimEnd()) };
}

async function pushFile(link, { local_path: localPath, remote_path: remotePath, drive = 'sd' }, readFile) {
  const data = await readFile(localPath);
  const number = driveNumber(drive);
  // The firmware will not overwrite a file, and a missing one fails the delete.
  await link.deleteFile(remotePath, number).catch(() => {});
  await link.postFile(data, remotePath, number);
  return { drive, remote_path: remotePath, bytes: data.length };
}

async function launch(link, { path, drive = 'sd' }) {
  await link.launch(path, driveNumber(drive));
  return { drive, path };
}

async function reset(link) {
  await link.reset();
  return {};
}

const DEFINITIONS = [
  {
    name: 'status',
    description: 'Which TeensyROM image is running, its firmware and board type, the host machine, whether '
      + "the menu has finished booting, and whether DMA tools can run. Call this first, and after 'reset' "
      + "until menu is 'complete'.",
    inputSchema: NO_INPUT,
    outputSchema: STATUS_OUTPUT,
    annotations: { readOnlyHint: true, openWorldHint: true },
    run: status,
  },
  {
    name: 'list_dir',
    description: "List a directory on the TeensyROM's SD card, USB drive or built-in menu, a page at a time. "
      + 'Needs the main image with the C64 at the TeensyROM menu; otherwise the board answers Busy!.',
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', default: '/', description: 'Directory to list, starting with /.' },
        drive: DRIVE_PROPERTY,
        skip: { type: 'integer', minimum: 0, default: 0, description: 'Entries to skip, for paging.' },
        take: { type: 'integer', minimum: 1, maximum: MAX_TAKE, default: DEFAULT_TAKE },
      },
    },
    outputSchema: LIST_DIR_OUTPUT,
    annotations: { readOnlyHint: true, openWorldHint: true },
    run: listDir,
  },
  {
    name: 'read_screen',
    description: "The C64's 40-column text screen as 25 rows of text, read by DMA. Needs a TR+ running its "
      + 'main image. It does not read the C128 80-column screen. Avoid it while a C128 runs at 2 MHz '
      + '(GEOS 128 in 80 columns): a DMA read can hang the machine.',
    inputSchema: NO_INPUT,
    outputSchema: READ_SCREEN_OUTPUT,
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
        drive: { ...DRIVE_PROPERTY, enum: STORAGE_DRIVES },
      },
      required: ['local_path', 'remote_path'],
    },
    outputSchema: PUSH_FILE_OUTPUT,
    annotations: { destructiveHint: true, idempotentHint: true, openWorldHint: true },
    run: pushFile,
  },
  {
    name: 'launch',
    description: 'Launch a file (PRG, CRT, SID, D64 and the other types the menu runs) on the C64, from the '
      + 'SD card, USB drive or built-in menu. Ends whatever the C64 was running. A large CRT hands over to the '
      + 'minimal image, which answers only status, launch and reset.',
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', description: 'Path of the file, starting with /.' },
        drive: DRIVE_PROPERTY,
      },
      required: ['path'],
    },
    outputSchema: LAUNCH_OUTPUT,
    annotations: { destructiveHint: false, openWorldHint: true },
    run: launch,
  },
  {
    name: 'reset',
    description: 'Reset the C64 to the TeensyROM menu, ending whatever it was running, and bring the board '
      + "back to its main image. The menu takes from under a second to tens of seconds to boot: call 'status' "
      + "until menu is 'complete' before using the other tools.",
    inputSchema: NO_INPUT,
    outputSchema: RESET_OUTPUT,
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
