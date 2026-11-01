// SPDX-License-Identifier: MIT
import { PassThrough } from 'node:stream';
import test from 'node:test';
import assert from 'node:assert/strict';

import { ByteStream } from '../lib/transport.mjs';

const FIRST_MS = 2000;
const IDLE_MS = 50;

function pipe() {
  const input = new PassThrough();
  return { input, stream: new ByteStream(input, new PassThrough(), () => {}) };
}

test('text that arrived with the reply before it is returned once the line goes quiet', async () => {
  const { input, stream } = pipe();
  input.write(Buffer.from('\xcc\x64banner', 'latin1'));
  await stream.read(2, FIRST_MS);
  const started = Date.now();
  const text = await stream.readIdle({ firstMs: FIRST_MS, idleMs: IDLE_MS, totalMs: FIRST_MS });
  assert.equal(text.toString(), 'banner');
  assert.ok(Date.now() - started < FIRST_MS / 2, `took ${Date.now() - started} ms`);
});

test('a read collects chunks until it has the count asked for', async () => {
  const { input, stream } = pipe();
  setTimeout(() => input.write('ab'), 10);
  setTimeout(() => input.write('cd'), 20);
  assert.equal((await stream.read(3, FIRST_MS)).toString(), 'abc');
  assert.equal((await stream.read(1, FIRST_MS)).toString(), 'd');
});

test('readUntilAny stops at whichever marker comes first', async () => {
  const { input, stream } = pipe();
  input.write('noise-B-then-A');
  assert.equal((await stream.readUntilAny([Buffer.from('A'), Buffer.from('B')], FIRST_MS)).toString(), 'noise-B');
});

test('a read that times out returns what it has', async () => {
  const { input, stream } = pipe();
  input.write('x');
  assert.equal((await stream.read(2, IDLE_MS)).toString(), 'x');
});
