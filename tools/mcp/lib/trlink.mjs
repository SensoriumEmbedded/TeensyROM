// SPDX-License-Identifier: MIT
//
// The commands a TeensyROM answers, over a ByteStream (see transport.mjs).
// The main image answers all of them. The minimal image answers reset, launch,
// version and the firmware check, and fails the rest with "Busy!". Reading C64
// memory also needs a TR+ (Fab 0.4).

import {
  DRIVES, IMAGES, TOKENS, boardReply, checksum, fromBoard, pathField, toBoard,
} from './protocol.mjs';
import { findSerialPort, openSerial, openTcp } from './transport.mjs';

const REPLY_BYTES = 2;
const REPLY_MS = 5000;
const DRAIN_MS = 300;
const FW_CHECK_MS = 3000;
const POST_DATA_MS = 180000;
const LISTING_MS = 120000;
const TEXT_REPLY = { firstMs: 5000, idleMs: 300, totalMs: 30000 };
const FAIL_TEXT = { firstMs: 1000, idleMs: 100, totalMs: 2000 };

export class BoardError extends Error {}

export function driveNumber(name) {
  if (name in DRIVES) return DRIVES[name];
  throw new BoardError(`drive '${name}': use ${Object.keys(DRIVES).join(', ')}`);
}

export class Link {
  #stream;

  constructor(stream) {
    this.#stream = stream;
  }

  close() {
    this.#stream.close();
  }

  async fwCheck() {
    await this.#send(toBoard(TOKENS.FW_CHECK));
    const replies = Object.keys(IMAGES).map((token) => boardReply(Number(token)));
    const seen = await this.#stream.readUntilAny(replies, FW_CHECK_MS);
    return seen === null ? null : IMAGES[fromBoard(seen.subarray(-REPLY_BYTES))];
  }

  async version() {
    await this.#send(toBoard(TOKENS.VERSION_INFO));
    await this.#ack('version');
    return (await this.#stream.readIdle(TEXT_REPLY)).toString('latin1');
  }

  async reset() {
    await this.#send(toBoard(TOKENS.RESET_C64));
    return (await this.#stream.readIdle(TEXT_REPLY)).toString('latin1').trim();
  }

  async launch(path, drive) {
    await this.#send(toBoard(TOKENS.LAUNCH_FILE));
    await this.#ack('launch');
    await this.#stream.write(Buffer.concat([Buffer.of(drive), pathField(path)]));
    await this.#ack(`launch ${path}`);
  }

  async readMemory(address, length) {
    await this.#send(Buffer.concat([toBoard(TOKENS.READ_C64_MEM), toBoard(address), toBoard(length)]));
    await this.#ack(`read of $${hex(address)}`);
    const data = await this.#stream.read(length, REPLY_MS);
    if (data.length !== length) {
      throw new BoardError(`read of $${hex(address)}: got ${data.length} of ${length} bytes`);
    }
    return data;
  }

  async deleteFile(path, drive) {
    await this.#send(toBoard(TOKENS.DELETE_FILE));
    await this.#ack('delete');
    await this.#stream.write(Buffer.concat([Buffer.of(drive), pathField(path)]));
    await this.#ack(`delete ${path}`);
  }

  async postFile(data, path, drive) {
    const header = Buffer.concat([toBoard(data.length, 4), toBoard(checksum(data)), Buffer.of(drive), pathField(path)]);
    await this.#send(toBoard(TOKENS.POST_FILE));
    await this.#ack('post');
    await this.#stream.write(header);
    await this.#ack(`post ${path}`);
    await this.#stream.write(data);
    await this.#ack(`post ${path} data`, POST_DATA_MS);
  }

  async listDirectory(path, drive, { skip, take }) {
    await this.#send(toBoard(TOKENS.GET_DIR_NDJSON));
    await this.#ack('directory');
    await this.#stream.write(Buffer.concat([Buffer.of(drive), toBoard(skip), toBoard(take), pathField(path)]));
    await this.#ack(`directory ${path}`);
    await this.#expect(TOKENS.DIR_START, `directory ${path}`);
    const listing = await this.#stream.readUntil(boardReply(TOKENS.DIR_END), LISTING_MS);
    if (listing === null) throw new BoardError(`the listing of ${path} stopped before its end marker`);
    return listing.subarray(0, -REPLY_BYTES).toString('latin1').split(/\r?\n/)
      .filter((line) => line.trim()).map((line) => JSON.parse(line));
  }

  async #send(data) {
    await this.#stream.drain(DRAIN_MS);
    await this.#stream.write(data);
  }

  async #ack(what, timeoutMs = REPLY_MS) {
    await this.#expect(TOKENS.ACK, what, timeoutMs);
  }

  async #expect(token, what, timeoutMs = REPLY_MS) {
    const reply = await this.#stream.read(REPLY_BYTES, timeoutMs);
    if (reply.length < REPLY_BYTES) throw new BoardError(`${what}: no reply from the board`);
    const value = fromBoard(reply);
    if (value === token) return;
    if (value === TOKENS.FAIL) {
      const text = (await this.#stream.readIdle(FAIL_TEXT)).toString('latin1').trim();
      throw new BoardError(`${what}: the board refused it${text ? `: ${text}` : ''}`);
    }
    throw new BoardError(`${what}: unexpected reply 0x${value.toString(16)}`);
  }
}

function hex(value) {
  return value.toString(16).toUpperCase().padStart(4, '0');
}

export function linkTarget(env) {
  if (env.TR_HOST) {
    const [host, port] = env.TR_HOST.split(':');
    return { kind: 'tcp', host, port: port ? Number(port) : undefined };
  }
  const device = env.TR_PORT || findSerialPort();
  if (!device) {
    throw new BoardError('no TeensyROM found: set TR_HOST to its address, or TR_PORT to its serial port');
  }
  return { kind: 'serial', device };
}

export async function openLink(target) {
  const stream = target.kind === 'tcp' ? await openTcp(target.host, target.port) : await openSerial(target.device);
  return new Link(stream);
}
