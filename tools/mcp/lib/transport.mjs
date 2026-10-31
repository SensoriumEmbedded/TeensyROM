// SPDX-License-Identifier: MIT
//
// Byte streams to a TeensyROM: TCP to its listener, or its USB serial port.

import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import tty from 'node:tty';

export const TCP_PORT = 2112;
const CONNECT_TIMEOUT_MS = 5000;
// The board drops what arrives while its USB serial is still settling after
// an open; the bench scripts wait the same.
const SERIAL_SETTLE_MS = 400;
const SERIAL_DIR = '/dev';
const SERIAL_NAMES = [/^cu\.usbmodem/, /^ttyACM/];

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

export class ByteStream {
  #buffered = Buffer.alloc(0);
  #ended = false;
  #wake = null;
  #output;
  #close;

  constructor(input, output, close) {
    this.#output = output;
    this.#close = close;
    input.on('data', (chunk) => this.#arrive(chunk));
    for (const event of ['end', 'close', 'error']) input.on(event, () => this.#end());
    output.on('error', () => this.#end());
  }

  get ended() {
    return this.#ended;
  }

  write(data) {
    if (this.#ended) return Promise.reject(new Error('the connection to the board closed'));
    return new Promise((resolve, reject) => {
      this.#output.write(data, (error) => (error ? reject(error) : resolve()));
    });
  }

  async read(count, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (this.#buffered.length < count && !this.#ended && Date.now() < deadline) {
      await this.#arrival(deadline - Date.now());
    }
    return this.#take(Math.min(count, this.#buffered.length));
  }

  readUntil(marker, timeoutMs) {
    return this.readUntilAny([marker], timeoutMs);
  }

  async readUntilAny(markers, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (!this.#endOfFirst(markers) && !this.#ended && Date.now() < deadline) {
      await this.#arrival(deadline - Date.now());
    }
    const end = this.#endOfFirst(markers);
    return end ? this.#take(end) : null;
  }

  async readIdle({ firstMs, idleMs, totalMs }) {
    const limit = Date.now() + totalMs;
    let deadline = Date.now() + firstMs;
    let seen = 0;
    while (!this.#ended) {
      if (this.#buffered.length > seen) {
        seen = this.#buffered.length;
        deadline = Date.now() + idleMs;
      }
      const until = Math.min(deadline, limit);
      if (Date.now() >= until) break;
      await this.#arrival(until - Date.now());
    }
    return this.#take(this.#buffered.length);
  }

  async drain(quietMs) {
    await sleep(quietMs);
    this.#take(this.#buffered.length);
  }

  close() {
    this.#end();
    this.#close();
  }

  #arrive(chunk) {
    this.#buffered = Buffer.concat([this.#buffered, chunk]);
    this.#wake?.();
  }

  #end() {
    this.#ended = true;
    this.#wake?.();
  }

  #endOfFirst(markers) {
    const ends = markers.map((marker) => {
      const at = this.#buffered.indexOf(marker);
      return at < 0 ? Infinity : at + marker.length;
    });
    const first = Math.min(...ends);
    return first === Infinity ? 0 : first;
  }

  #take(count) {
    const taken = this.#buffered.subarray(0, count);
    this.#buffered = this.#buffered.subarray(count);
    return Buffer.from(taken);
  }

  #arrival(ms) {
    return new Promise((resolve) => {
      const done = () => {
        clearTimeout(timer);
        this.#wake = null;
        resolve();
      };
      const timer = setTimeout(done, Math.max(ms, 0));
      this.#wake = done;
    });
  }
}

export function openTcp(host, port = TCP_PORT) {
  return new Promise((resolve, reject) => {
    const socket = net.connect({ host, port });
    const timer = setTimeout(() => fail(new Error(`no answer from ${host}:${port}`)), CONNECT_TIMEOUT_MS);
    function fail(error) {
      clearTimeout(timer);
      socket.destroy();
      reject(error);
    }
    socket.once('error', fail);
    socket.once('connect', () => {
      clearTimeout(timer);
      socket.off('error', fail);
      socket.setNoDelay(true);
      resolve(new ByteStream(socket, socket, () => socket.destroy()));
    });
  });
}

export async function openSerial(device) {
  if (process.platform === 'win32') {
    throw new Error('USB serial is not supported on Windows yet; set TR_HOST to use the TCP listener');
  }
  const readFd = fs.openSync(device, fs.constants.O_RDWR | fs.constants.O_NOCTTY);
  if (!tty.isatty(readFd)) {
    fs.closeSync(readFd);
    throw new Error(`${device} is not a serial port`);
  }
  const writeFd = fs.openSync(device, fs.constants.O_RDWR | fs.constants.O_NOCTTY);
  const input = new tty.ReadStream(readFd);
  input.setRawMode(true);
  const output = new tty.WriteStream(writeFd);
  const stream = new ByteStream(input, output, () => {
    input.destroy();
    output.destroy();
  });
  await sleep(SERIAL_SETTLE_MS);
  return stream;
}

export function findSerialPort(directory = SERIAL_DIR) {
  const names = fs.readdirSync(directory).filter((name) => SERIAL_NAMES.some((pattern) => pattern.test(name)));
  const first = names.sort()[0];
  return first ? path.join(directory, first) : null;
}
