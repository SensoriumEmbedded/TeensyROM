// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { createTools } from '../lib/tools.mjs';
import { linkTarget, openLink } from '../lib/trlink.mjs';
import { startFakeBoard } from './fake-board.mjs';

async function boardWithTools(t, overrides, readFile) {
  const board = await startFakeBoard(overrides);
  t.after(board.stop);
  const connect = () => openLink(linkTarget({ TR_HOST: board.host }));
  const tools = Object.fromEntries(createTools({ connect, readFile }).map((tool) => [tool.name, tool.run]));
  return { board, tools };
}

test('status names the image, the build, the machine and the menu state', async (t) => {
  const { tools } = await boardWithTools(t);
  assert.equal(await tools.status({}), [
    'image: main', 'firmware: TeensyROM+ v0.8.0.13', 'built: Oct 30 2026, 10:00:00', 'board: tr-plus',
    'machine: C64 NTSC', 'menu: complete', 'dma: available',
  ].join('\n'));
});

test('the minimal image has no DMA', async (t) => {
  const { tools } = await boardWithTools(t, { image: 'minimal', banner: '\n  FW: TeensyROM+ v0.8.0.13(minimal)\r\n' });
  assert.match(await tools.status({}), /^image: minimal\n[\s\S]*dma: not available$/);
});

test('read_screen finds the screen through the VIC registers and decodes it', async (t) => {
  const { board, tools } = await boardWithTools(t);
  board.memory[0xd018] = 0x17;
  board.memory[0xdd00] = 0x97;
  board.memory.set([0x08, 0x09, 0x20, 0x54, 0x52], 0x0400);
  const lines = (await tools.read_screen({})).split('\n');
  assert.equal(lines[0], 'screen at $400, charset lower/uppercase');
  assert.equal(lines[1], 'hi TR');
  assert.equal(lines.length, 26);
});

test('push_file replaces a file the board already has', async (t) => {
  const { board, tools } = await boardWithTools(
    t, { files: new Map([['/a.prg', Buffer.from('old')]]) }, async () => Buffer.from('new data'));
  assert.equal(await tools.push_file({ local_path: 'a.prg', remote_path: '/a.prg' }),
    'pushed a.prg to sd:/a.prg (8 bytes)');
  assert.equal(board.files.get('/a.prg').toString(), 'new data');
});

test('push_file writes a file that was not there', async (t) => {
  const { board, tools } = await boardWithTools(t, {}, async () => Buffer.from('x'));
  await tools.push_file({ local_path: 'b.prg', remote_path: '/b.prg' });
  assert.equal(board.files.get('/b.prg').toString(), 'x');
});

test('list_dir lists entries and says how to get the next page', async (t) => {
  const listing = [
    { type: 'dir', name: 'games' }, { type: 'file', name: 'a.prg', size: 10 }, { type: 'file', name: 'b.sid', size: 20 },
  ];
  const { tools } = await boardWithTools(t, { listing });
  assert.equal(await tools.list_dir({ take: 2 }), 'games/\na.prg  (10 bytes)\n(first 2 from entry 0; ask again with skip=2)');
  assert.equal(await tools.list_dir({ skip: 2 }), 'b.sid  (20 bytes)');
});

test('launch sends the drive and path', async (t) => {
  const { board, tools } = await boardWithTools(t);
  assert.equal(await tools.launch({ path: '/games/a.crt', drive: 'usb' }), 'launched usb:/games/a.crt');
  assert.deepEqual(board.launched, [{ drive: 0, path: '/games/a.crt' }]);
});

test('reset returns what the board said', async (t) => {
  const { tools } = await boardWithTools(t);
  assert.equal(await tools.reset({}), 'Reset cmd received');
});

test("a refusal carries the board's own words", async (t) => {
  const { tools } = await boardWithTools(t, { busy: true });
  await assert.rejects(tools.list_dir({}), /directory: the board refused it: Busy!/);
});

test('an unknown drive is refused before anything is sent', async (t) => {
  const { tools } = await boardWithTools(t);
  await assert.rejects(tools.launch({ path: '/a.prg', drive: 'tape' }), /drive 'tape': use usb, sd, teensy/);
});
