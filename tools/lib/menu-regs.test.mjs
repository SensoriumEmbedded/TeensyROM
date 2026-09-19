// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { generateMenuRegsI, MENU_REGS_H, MENU_REGS_I } from './menu-regs.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = (rel) => fs.readFileSync(path.join(root, rel), 'utf8').replaceAll('\r\n', '\n');

// Menu_Regs.i is committed, and both gen_menu_regs_i.py and this port must reproduce it from
// Menu_Regs.h exactly. If Menu_Regs.h changes without a rebuild, this is the test that says so.
test('regenerating from Menu_Regs.h reproduces the committed Menu_Regs.i', () => {
  assert.equal(generateMenuRegsI(read(MENU_REGS_H)), read(MENU_REGS_I));
});

const sample = (region) => `
enum enumIOHandlers
{
    IOH_None,
    IOH_Other,
};
// These need to match C64 Code
${region}
// End C64 matching
`;

test('enum members auto-increment from 0 and keep their comments', () => {
  const out = generateMenuRegsI(sample(`enum RegA
{
    First, // the first
    Second,
    Third = 0x10,
    Fourth,
};`));
  assert.match(out, /^;enum RegA$/m);
  assert.match(out, /^ {3}First = 0 ; the first$/m);
  assert.match(out, /^ {3}Second = 1$/m);
  assert.match(out, /^ {3}Third = 0x10$/m);
  assert.match(out, /^ {3}Fourth = 17$/m);
  assert.match(out, /^ {3}IOH_None = 0 {2};only part of enumIOHandlers/m);
});

test('#define lines become assignments', () => {
  const out = generateMenuRegsI(sample('#define   RegBase   0xDE00  // io1'));
  assert.match(out, /^ {3}RegBase = 0xDE00 ; io1$/m);
});

test('a bare member after a non-literal value is an error, not a guess', () => {
  assert.throws(
    () => generateMenuRegsI(sample('enum E\n{\n    A = SOME_MACRO,\n    B,\n};')),
    /B has no explicit value/,
  );
});

test('IOH_None must stay first and zero', () => {
  const bad = sample('').replace('IOH_None,', 'IOH_None = 3,');
  assert.throws(() => generateMenuRegsI(bad), /expected 0/);
});

test('missing markers are reported', () => {
  assert.throws(() => generateMenuRegsI('enum enumIOHandlers\n{\n IOH_None,\n};\n'), /synced region markers/);
});
