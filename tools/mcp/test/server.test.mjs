// SPDX-License-Identifier: MIT
import { spawn } from 'node:child_process';
import path from 'node:path';
import readline from 'node:readline';
import test from 'node:test';
import assert from 'node:assert/strict';

import { startFakeBoard } from './fake-board.mjs';

const SERVER = path.resolve(import.meta.dirname, '../server.mjs');
const NOBODY_LISTENING = '127.0.0.1:1';

function startServer(t, env) {
  const child = spawn(process.execPath, [SERVER], { env: { ...process.env, ...env }, stdio: ['pipe', 'pipe', 'inherit'] });
  t.after(() => child.kill());
  const replies = readline.createInterface({ input: child.stdout })[Symbol.asyncIterator]();
  let id = 0;
  return {
    async request(method, params) {
      id += 1;
      child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', id, method, params })}\n`);
      return JSON.parse((await replies.next()).value);
    },
    notify(method) {
      child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', method })}\n`);
    },
  };
}

test('a client can initialize, list the tools and call one over stdio', async (t) => {
  const board = await startFakeBoard();
  t.after(board.stop);
  const server = startServer(t, { TR_HOST: board.host });

  const init = await server.request('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'test' } });
  assert.equal(init.result.protocolVersion, '2025-06-18');
  assert.deepEqual(init.result.capabilities, { tools: {} });
  server.notify('notifications/initialized');

  const listed = await server.request('tools/list');
  assert.deepEqual(listed.result.tools.map((tool) => tool.name),
    ['status', 'list_dir', 'read_screen', 'push_file', 'launch', 'reset']);
  assert.ok(listed.result.tools.every((tool) => tool.inputSchema.type === 'object' && !('run' in tool)));

  const called = await server.request('tools/call', { name: 'status', arguments: {} });
  assert.equal(called.result.isError, false);
  assert.match(called.result.content[0].text, /^image: main/);
});

test('a client asking for a protocol version this server lacks is offered the newest it has', async (t) => {
  const server = startServer(t, { TR_HOST: NOBODY_LISTENING });
  const init = await server.request('initialize', { protocolVersion: '1999-01-01' });
  assert.equal(init.result.protocolVersion, '2025-11-25');
});

test('a board that cannot be reached is a tool error, not a protocol error', async (t) => {
  const server = startServer(t, { TR_HOST: NOBODY_LISTENING });
  const called = await server.request('tools/call', { name: 'status', arguments: {} });
  assert.equal(called.result.isError, true);
  assert.match(called.result.content[0].text, /ECONNREFUSED/);
});

test('an unknown method is a JSON-RPC error', async (t) => {
  const server = startServer(t, { TR_HOST: NOBODY_LISTENING });
  const reply = await server.request('resources/list');
  assert.equal(reply.error.code, -32601);
});
