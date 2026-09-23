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

export const readSource = (file) => withoutComments(fs.readFileSync(file, 'utf8'));
