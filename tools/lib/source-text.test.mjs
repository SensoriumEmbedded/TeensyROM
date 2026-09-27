// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';

import { blankComments, definesMacro, withoutComments } from './source-text.mjs';

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

// Every spelling below was run through `cc -E` to confirm which ones actually define the
// macro. A guard that reads a narrower shape than the compiler does reports a file as clean
// and then compiles it differently -- for tools/build-firmware.mjs's Fab04_Features guard,
// that is a TR+ image shipped under the plain-TR filename, exit 0.
test('a define is recognised in every spelling the preprocessor honours', () => {
  for (const source of [
    '#define Fab04_Features',
    '   #define Fab04_Features    //enables all Fab 0.4 build options',
    '#  define Fab04_Features',
    '#\tdefine Fab04_Features',
    '/*c*/ #define Fab04_Features',
    'int x;\n/* two\n   lines */ #define Fab04_Features',
    '#define \\\n  Fab04_Features',
  ]) {
    assert.equal(definesMacro(source, 'Fab04_Features'), true, JSON.stringify(source));
  }
});

test('a define that is only mentioned, or only nearly named, is not recognised', () => {
  for (const source of [
    ' // #define Fab04_Features    //enables all Fab 0.4 build options',
    '/* #define Fab04_Features */',
    '/* leading\n   #define Fab04_Features\n */',
    '#define Fab04_FeaturesExtra',
    '#undef Fab04_Features',
    '// pass --yes to have the builder comment out #define Fab04_Features for you',
  ]) {
    assert.equal(definesMacro(source, 'Fab04_Features'), false, JSON.stringify(source));
  }
});
