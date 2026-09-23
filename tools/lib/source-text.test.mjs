// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { blankComments, withoutComments } from './source-text.mjs';

const DECLARATION = /VM_HOST_SLOT_BASE = (0x[0-9a-fA-F]+)u?/;
const INCLUDE = /^[ \t]*#[ \t]*include[ \t]*([<"][^>"\n]*[>"])/gm;

test('a comment naming a constant cannot stand in for the declaration', () => {
  const source = [
    '// historical: VM_HOST_SLOT_BASE = 0x60280000u before the move',
    'enum : uint32_t { VM_HOST_SLOT_BASE = 0x60990000u };',
  ].join('\n');

  assert.equal(source.match(DECLARATION)[1], '0x60280000', 'the raw text matches the comment first');
  assert.equal(withoutComments(source).match(DECLARATION)[1], '0x60990000');
});

test('a block comment spanning lines is removed whole', () => {
  const source = '/* VM_HOST_SLOT_BASE = 0x60280000u\n   and more */\nVM_HOST_SLOT_BASE = 0x60990000u;';

  assert.equal(withoutComments(source).match(DECLARATION)[1], '0x60990000');
});

test('a continued include directive reads as one line', () => {
  const source = '#include \\\n  "Common_Defs.h"\n';

  assert.deepEqual([...source.matchAll(INCLUDE)], [], 'the raw text hides it across the newline');
  assert.deepEqual([...withoutComments(source).matchAll(INCLUDE)].map(([, taken]) => taken),
    ['"Common_Defs.h"']);
});

test('code outside comments is left alone', () => {
  const source = 'enum : uint32_t { VM_HOST_SLOT_BASE = 0x60280000u };\n#include <stdint.h>\n';

  assert.equal(withoutComments(source), source);
});

test('blanking a comment keeps every line and column after it', () => {
  const source = '/* two\n   lines */ VM_HOST_SLOT_BASE = 0x60990000u; // trailing\nnext\n';
  const blanked = blankComments(source);

  assert.equal(blanked.split('\n').length, source.split('\n').length);
  assert.equal(blanked.indexOf('VM_HOST_SLOT_BASE'), source.indexOf('VM_HOST_SLOT_BASE'));
  assert.equal(blanked.match(DECLARATION)[1], '0x60990000');
  assert.doesNotMatch(blanked, /trailing|lines/);
});
