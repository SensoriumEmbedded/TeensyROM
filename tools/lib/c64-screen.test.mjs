// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { scanSource, scanTree, loadSymbols, acmeSources, COLUMNS } from './c64-screen.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const C64_DIR = path.join(root, 'Source/C64');

const DEFS = [
  '   EscC = $01',
  '   EscArgMask = $c0',
  '   EscArgSpaces = $80',
  '   EscSourcesColor = 5',
  '   ChrReturn = 13',
  '   ChrRvsOn = 18',
  '   ChrClear = 147',
].join('\n');

const symbols = () => {
  const map = new Map();
  for (const line of DEFS.split('\n')) {
    const [, name, value] = /^\s*([A-Za-z_]\w*)\s*=\s*\$?(\w+)$/.exec(line);
    map.set(name, value.length && line.includes('$') ? parseInt(value, 16) : Number(value));
  }
  return map;
};

const scan = (body) => scanSource('test.asm', `${DEFS}\nMsg:\n${body}\n   !tx 0\n`, symbols());

// The row the wrap lands on is blank, so the sentence has a hole in it on the screen while
// reading fine in the source. This is the case bc43dfb was opened for, kept here as the
// control: a scanner that does not flag it is not measuring anything.
test('a row filled to all 40 columns and then given a return is reported', () => {
  const forty = ' the slot bootable. The host image stays';
  assert.equal(forty.length, COLUMNS);
  const { hits, unresolved } = scan(`   !tx ChrReturn\n   !tx EscC,EscSourcesColor, "${forty}", ChrReturn`);
  assert.deepEqual(unresolved, []);
  assert.equal(hits.length, 1);
  assert.equal(hits[0].anchored, true);
});

test('the same row re-wrapped one word shorter is not reported', () => {
  const { hits } = scan('   !tx ChrReturn\n   !tx EscC,EscSourcesColor, " the slot bootable. The host image", ChrReturn');
  assert.deepEqual(hits, []);
});

// The reason this measures rows rather than source lines: each !tx here is well under 40
// columns, and together they fill one. A per-!tx sweep reads this as clean.
test('a row split across two !tx directives is measured as one row', () => {
  const half = '"12345678901234567890"';
  const { hits } = scan(`   !tx ChrReturn\n   !tx ${half}\n   !tx ${half}, ChrReturn`);
  assert.equal(hits.length, 1);
  // Reported against the return that loses the row, which is the second half's line --
  // neither !tx is over 40 on its own, so there is no single line to blame.
  assert.match(hits[0].source, /, ChrReturn$/);
});

test('39 columns is the widest row that survives a following return', () => {
  const under = scan(`   !tx ChrReturn\n   !tx "${'x'.repeat(39)}", ChrReturn`);
  assert.deepEqual(under.hits, []);
  const over = scan(`   !tx ChrReturn\n   !tx "${'x'.repeat(40)}", ChrReturn`);
  assert.equal(over.hits.length, 1);
});

// Colour and reverse-video codes are control codes: CHROUT acts on them without moving the
// cursor, which is why the visible width of an !tx line is not its character count.
test('colour and reverse-video codes cost no columns', () => {
  const { hits } = scan(`   !tx ChrReturn\n   !tx EscC,EscSourcesColor, ChrRvsOn, "${'x'.repeat(39)}", ChrReturn`);
  assert.deepEqual(hits, []);
});

test('EscArgSpaces draws the spaces it asks for', () => {
  const { hits } = scan(`   !tx ChrReturn\n   !tx EscC,EscArgSpaces+8, "${'x'.repeat(32)}", ChrReturn`);
  assert.equal(hits.length, 1);
});

// TRExtPortCheck.asm has a line that measures 41 as typed and 39 drawn for this reason.
test('a backslash-escaped quote is one drawn column', () => {
  const { hits } = scan(`   !tx ChrReturn\n   !tx "${'x'.repeat(37)}\\"\\"", ChrReturn`);
  assert.deepEqual(hits, []);
});

test('an expression it cannot evaluate is reported rather than assumed', () => {
  const { unresolved } = scan('   !tx ChrReturn\n   !tx WhoKnows, "x", ChrReturn');
  assert.deepEqual(unresolved, ['WhoKnows']);
});

// A full-width reverse-video bar is the one place a 40-column row is meant, and both of
// these are drawn under a ruler comment counting to 40. The cost is the row the wrap lands
// on: the returns after the bar each advance a further line, so the text below sits one row
// lower than the same pattern produces on a 39-column banner. Cosmetic, pre-existing, and
// deliberate -- listed so a new one has to be added here on purpose rather than slipping in.
const DELIBERATE_FULL_WIDTH_ROWS = [
  'MIDI2SID/source/M2Ssupport.asm:158',
  'MainMenuCRT/source/TeensyROMC64.asm:155',
];

test('no C64 message row fills all 40 columns except the declared banners', () => {
  const { hits, unresolved } = scanTree(C64_DIR);
  assert.deepEqual(unresolved, [], 'every symbol in the message tables must resolve');
  const found = hits.map((hit) => `${hit.file.split(path.sep).join('/')}:${hit.line}`).sort();
  assert.deepEqual(found, [...DELIBERATE_FULL_WIDTH_ROWS].sort());
});

test('the sweep reaches the .s and .i sources a *.asm glob would miss', () => {
  const files = acmeSources(C64_DIR).map((f) => path.relative(C64_DIR, f));
  assert.ok(files.some((f) => f.endsWith('.s')), 'expected at least one .s source');
  assert.ok(loadSymbols(acmeSources(C64_DIR)).get('EscC') === 0x01, 'EscC must resolve from the tree');
});
