// SPDX-License-Identifier: MIT
import fs from 'node:fs';

// Reading a declaration out of a C or C++ source as text, for the drift gates
// that compare one file's constant against another's. A comment that mentions
// a declaration is not that declaration, so comments go before any gate
// matches; line continuations are spliced first, which is the order the
// preprocessor itself uses and is what makes a continued directive one line.
export function withoutComments(source) {
  return source
    .replace(/\\\r?\n/g, ' ')
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/\/\/[^\n]*/g, '');
}

// Comments blanked rather than removed, so every offset and line number in the
// result still matches the file on disk -- for a gate that reports where it
// found something.
export function blankComments(source) {
  return source.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, (text) => text.replace(/[^\n]/g, ' '));
}

// Whether the preprocessor would act on a `#define <name>` in this source. Directives are
// read the way the preprocessor reads them rather than the way they are usually typed:
// continuations are spliced first, then comments are blanked -- the preprocessor's own order,
// and blanking rather than deleting keeps every directive on the line it started on, so `^`
// still means line start.
//
// The spellings this has to carry, each confirmed active by `cc -E`: `#define X`, `#  define X`
// (whitespace is allowed between the hash and the directive) and `/*c*/ #define X` (a comment is
// whitespace by the time directives are executed, so it does not stop the line from starting
// with one). `// #define X` is not one of them. A gate that reads a narrower shape than the
// compiler does passes a file the compiler will treat differently -- and for a build-target
// guard that means shipping the wrong image under the right name.
export function definesMacro(source, name) {
  const spliced = source.replace(/\\\r?\n/g, ' ');
  return new RegExp(String.raw`^[ \t]*#[ \t]*define[ \t]+${name}\b`, 'm').test(blankComments(spliced));
}

export const readSource = (file) => withoutComments(fs.readFileSync(file, 'utf8'));
