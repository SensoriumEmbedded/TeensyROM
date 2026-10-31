// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { createTools } from '../lib/tools.mjs';
import { linkTarget, openLink } from '../lib/trlink.mjs';
import { startFakeBoard } from './fake-board.mjs';
import { schemaProblems } from './schema.mjs';

function checkedAgainstSchema(tool) {
  return async (args) => {
    const result = await tool.run(args);
    assert.deepEqual(schemaProblems(result, tool.outputSchema), [], `${tool.name} output`);
    return result;
  };
}

async function boardWithTools(t, overrides, readFile) {
  const board = await startFakeBoard(overrides);
  t.after(board.stop);
  const connect = () => openLink(linkTarget({ TR_HOST: board.host }));
  const tools = Object.fromEntries(createTools({ connect, readFile }).map((tool) => [tool.name, checkedAgainstSchema(tool)]));
  return { board, tools };
}

test('status names the image, the build, the machine and the menu state', async (t) => {
  const { tools } = await boardWithTools(t);
  assert.deepEqual(await tools.status({}), {
    image: 'main', firmware: 'TeensyROM+ v0.8.0.13', built: 'Oct 30 2026, 10:00:00', board: 'tr-plus',
    machine: 'C64', video: 'NTSC', menu: 'complete', dma: true,
  });
});

test('the minimal image has no DMA and no menu state', async (t) => {
  const { tools } = await boardWithTools(t, { image: 'minimal', banner: '\n  FW: TeensyROM+ v0.8.0.13(minimal)\r\n' });
  const result = await tools.status({});
  assert.equal(result.image, 'minimal');
  assert.equal(result.menu, null);
  assert.equal(result.dma, false);
});

test('a board that answers nothing has every field null and no DMA', async (t) => {
  const { tools } = await boardWithTools(t, { silent: true });
  assert.deepEqual(await tools.status({}), {
    image: null, firmware: null, built: null, board: null, machine: null, video: null, menu: null, dma: false,
  });
});

test('read_screen finds the screen through the VIC registers and decodes it', async (t) => {
  const { board, tools } = await boardWithTools(t);
  board.memory[0xd018] = 0x17;
  board.memory[0xdd00] = 0x97;
  board.memory.set([0x08, 0x09, 0x20, 0x54, 0x52], 0x0400);
  const { address, charset, rows } = await tools.read_screen({});
  assert.equal(address, 0x0400);
  assert.equal(charset, 'lower_upper');
  assert.equal(rows[0], 'hi TR');
  assert.equal(rows[1], '');
});

test('push_file replaces a file the board already has', async (t) => {
  const { board, tools } = await boardWithTools(
    t, { files: new Map([['/a.prg', Buffer.from('old')]]) }, async () => Buffer.from('new data'));
  assert.deepEqual(await tools.push_file({ local_path: 'a.prg', remote_path: '/a.prg' }),
    { drive: 'sd', remote_path: '/a.prg', bytes: 8 });
  assert.equal(board.files.get('/a.prg').toString(), 'new data');
});

test('push_file writes a file that was not there', async (t) => {
  const { board, tools } = await boardWithTools(t, {}, async () => Buffer.from('x'));
  await tools.push_file({ local_path: 'b.prg', remote_path: '/b.prg' });
  assert.equal(board.files.get('/b.prg').toString(), 'x');
});

test('list_dir returns entries and the skip for the next page', async (t) => {
  const listing = [
    { type: 'dir', name: 'games' }, { type: 'file', name: 'a.prg', size: 10 }, { type: 'file', name: 'b.sid', size: 20 },
  ];
  const { tools } = await boardWithTools(t, { listing });
  assert.deepEqual(await tools.list_dir({ take: 2 }), {
    path: '/', drive: 'sd', skip: 0, entries: listing.slice(0, 2), next_skip: 2,
  });
  assert.deepEqual(await tools.list_dir({ skip: 2 }), {
    path: '/', drive: 'sd', skip: 2, entries: listing.slice(2), next_skip: null,
  });
});

test('launch sends the drive and path', async (t) => {
  const { board, tools } = await boardWithTools(t);
  assert.deepEqual(await tools.launch({ path: '/games/a.crt', drive: 'usb' }), { drive: 'usb', path: '/games/a.crt' });
  assert.deepEqual(board.launched, [{ drive: 0, path: '/games/a.crt' }]);
});

test('reset returns once the board has the command', async (t) => {
  const { tools } = await boardWithTools(t);
  assert.deepEqual(await tools.reset({}), {});
});

test("a refusal carries the board's own words", async (t) => {
  const { tools } = await boardWithTools(t, { busy: true });
  await assert.rejects(tools.list_dir({}), /directory: the board refused it: Busy!/);
});

test('a refusal at the menu carries only the line the board printed', async (t) => {
  const { tools } = await boardWithTools(t);
  await assert.rejects(tools.list_dir({ path: '/nope' }), {
    message: 'directory /nope: the board refused it: Directory not found. (Error 5)',
  });
});

test('an unknown drive is refused before anything is sent', async (t) => {
  const { tools } = await boardWithTools(t);
  await assert.rejects(tools.launch({ path: '/a.prg', drive: 'tape' }), /drive 'tape': use usb, sd, teensy/);
});

test('a path the board cannot hold whole is refused before it is sent', async (t) => {
  const { board, tools } = await boardWithTools(t, {}, async () => Buffer.from('x'));
  await assert.rejects(tools.launch({ path: `/${'a'.repeat(357)}` }), /at most 357 characters/);
  await assert.rejects(tools.launch({ path: '/a.prg\0e' }), /NUL/);
  await assert.rejects(tools.list_dir({ path: `/${'a'.repeat(357)}` }), /at most 357 characters/);
  await assert.rejects(tools.push_file({ local_path: 'x', remote_path: '/a\0e' }), /NUL/);
  await assert.rejects(tools.launch({ path: '/aĀŤǮ' }), /Latin-1/);
  await tools.launch({ path: `/${'a'.repeat(356)}` });
  assert.deepEqual(board.launched.map(({ path }) => path.length), [357]);
});
