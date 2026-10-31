// SPDX-License-Identifier: MIT
//
// A TeensyROM main image on a local TCP port, answering the commands the
// server sends as docs/ControlComms.md describes them.

import net from 'node:net';

import { TOKENS, boardReply, checksum } from '../lib/protocol.mjs';
import { ByteStream } from '../lib/transport.mjs';

const WAIT_MS = 2000;

export const BANNER = '\n  FW: TeensyROM+ v0.8.0.13\r\n      Oct 30 2026, 10:00:00\r\n'
  + '  Teensy: 816MHz  55.0C  UID: 19277260\r\n  C64  NTSC Vid  60 Hz\n  Boot: complete\n';

async function readPath(stream) {
  return (await stream.readUntil(Buffer.of(0), WAIT_MS)).subarray(0, -1).toString('latin1');
}

async function readNumber(stream, width) {
  return (await stream.read(width, WAIT_MS)).readUIntBE(0, width);
}

function reply(stream, token) {
  return stream.write(boardReply(token));
}

async function refuse(stream, text) {
  await stream.write(Buffer.concat([boardReply(TOKENS.FAIL), Buffer.from(`${text}\n`)]));
}

function commandsFor(board) {
  return {
    [TOKENS.FW_CHECK]: (stream) => reply(stream, board.image === 'main' ? TOKENS.FW_FULL : TOKENS.FW_MINIMAL),
    [TOKENS.VERSION_INFO]: async (stream) => {
      await reply(stream, TOKENS.ACK);
      await stream.write(Buffer.from(board.banner, 'latin1'));
    },
    [TOKENS.RESET_C64]: (stream) => stream.write(Buffer.from('Reset cmd received\n')),
    [TOKENS.READ_C64_MEM]: async (stream) => {
      const address = await readNumber(stream, 2);
      const length = await readNumber(stream, 2);
      await reply(stream, TOKENS.ACK);
      await stream.write(board.memory.subarray(address, address + length));
    },
    [TOKENS.LAUNCH_FILE]: async (stream) => {
      await reply(stream, TOKENS.ACK);
      const drive = (await stream.read(1, WAIT_MS))[0];
      board.launched.push({ drive, path: await readPath(stream) });
      await reply(stream, TOKENS.ACK);
    },
    [TOKENS.DELETE_FILE]: async (stream) => {
      await reply(stream, TOKENS.ACK);
      await stream.read(1, WAIT_MS);
      const path = await readPath(stream);
      if (!board.files.delete(path)) return refuse(stream, 'File not found');
      return reply(stream, TOKENS.ACK);
    },
    [TOKENS.POST_FILE]: async (stream) => {
      await reply(stream, TOKENS.ACK);
      const length = await readNumber(stream, 4);
      const sum = await readNumber(stream, 2);
      await stream.read(1, WAIT_MS);
      const path = await readPath(stream);
      if (board.files.has(path)) return refuse(stream, 'File exists');
      await reply(stream, TOKENS.ACK);
      const data = await stream.read(length, WAIT_MS);
      if (checksum(data) !== sum) return refuse(stream, 'Checksum mismatch');
      board.files.set(path, data);
      return reply(stream, TOKENS.ACK);
    },
    [TOKENS.GET_DIR_NDJSON]: async (stream) => {
      await reply(stream, TOKENS.ACK);
      await stream.read(1, WAIT_MS);
      const skip = await readNumber(stream, 2);
      const take = await readNumber(stream, 2);
      await readPath(stream);
      await reply(stream, TOKENS.ACK);
      await reply(stream, TOKENS.DIR_START);
      const lines = board.listing.slice(skip, skip + take).map((entry) => `${JSON.stringify(entry)}\r\n`);
      await stream.write(Buffer.from(lines.join('')));
      await reply(stream, TOKENS.DIR_END);
    },
  };
}

async function serve(board, socket) {
  const stream = new ByteStream(socket, socket, () => socket.destroy());
  const commands = commandsFor(board);
  while (!stream.ended) {
    const token = await stream.read(2, 60000);
    if (token.length < 2) break;
    const command = commands[token.readUInt16BE(0)];
    if (board.busy && token.readUInt16BE(0) !== TOKENS.FW_CHECK) await refuse(stream, 'Busy!');
    else if (command) await command(stream);
  }
}

export async function startFakeBoard(overrides = {}) {
  const board = {
    image: 'main',
    banner: BANNER,
    busy: false,
    memory: Buffer.alloc(0x10000, 0x20),
    files: new Map(),
    listing: [],
    launched: [],
    ...overrides,
  };
  const sockets = new Set();
  const server = net.createServer((socket) => {
    sockets.add(socket);
    socket.on('close', () => sockets.delete(socket));
    serve(board, socket);
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  board.host = `127.0.0.1:${server.address().port}`;
  board.stop = () => new Promise((resolve) => {
    server.close(resolve);
    for (const socket of sockets) socket.destroy();
  });
  return board;
}
