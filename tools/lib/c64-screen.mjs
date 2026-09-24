// SPDX-License-Identifier: MIT
//
// Finds the one screen-editor trap the C64 menu text keeps falling into: a row that fills
// all 40 columns, followed by an explicit ChrReturn.
//
// Writing the character in column 39 makes the editor advance to the next row on its own,
// and it links the two rows into one logical line. A ChrReturn after that moves to the line
// *after* the logical one, so the row the wrap landed on is left blank. The text reads fine
// in the source and has a hole in it on the screen, which is why this is only ever found by
// reading a board -- bc43dfb was.
//
// This measures rows, not source lines. A row is whatever is emitted between two returns,
// and it can be built from several !tx directives, so "keep each !tx under 40 columns" is
// not the same rule and does not catch the same cases. Scanning per source line is what let
// the two full-width banners below go unnoticed.
//
// The model is only the part of CHROUT that moves the cursor: printable characters advance
// and wrap at 40, control codes do not (except CRSR left/right), ChrClear and ChrHome park
// it at column 0, and a quote flips the editor into quote mode where control codes are
// printed as reverse glyphs and do advance. Colour and reverse-video codes are controls, so
// they cost no columns -- which is the whole reason the visible width of an !tx line cannot
// be read off its character count.
import fs from 'node:fs';
import path from 'node:path';

export const COLUMNS = 40;

// ACME `NAME = expr` lines, across every file given, so an expression like `EscArgSpaces+2`
// or `OptionColor = ChrYellow` resolves. Repeated passes because the definitions are spread
// over several files and refer to each other; four is well past the deepest chain in tree.
export function loadSymbols(files) {
  const symbols = new Map();
  const define = /^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+?)\s*(?:;.*)?$/;
  for (let pass = 0; pass < 4; pass++) {
    for (const file of files) {
      for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
        const match = define.exec(line);
        if (!match || symbols.has(match[1])) continue;
        const value = evaluate(match[2], symbols);
        if (value !== null) symbols.set(match[1], value);
      }
    }
  }
  return symbols;
}

// ACME expressions, limited to what the message tables actually use: names, decimal, $hex,
// and + - * / between them. Anything else returns null and the caller reports it rather than
// guessing a value -- a symbol read as 0 would silently terminate a message mid-scan.
function evaluate(expr, symbols) {
  const text = expr.trim();
  if (!/^[A-Za-z0-9_$+\-*/() ]+$/.test(text)) return null;
  const resolved = text
    .replace(/\$([0-9a-fA-F]+)/g, (_, hex) => String(parseInt(hex, 16)))
    .replace(/[A-Za-z_][A-Za-z0-9_]*/g, (name) => (symbols.has(name) ? String(symbols.get(name)) : 'NaN'));
  if (resolved.includes('NaN')) return null;
  let value;
  try {
    value = Function(`"use strict";return (${resolved});`)();
  } catch {
    return null;
  }
  return Number.isFinite(value) ? value : null;
}

// ACME's `!ct pet` mapping, for the two ranges it moves: ASCII lowercase becomes PETSCII
// uppercase and ASCII uppercase becomes the shifted graphics range. Neither range overlaps
// the control codes, so for cursor purposes only the ranges matter, not the exact values.
function petscii(ch) {
  const code = ch.charCodeAt(0);
  if (code >= 0x61 && code <= 0x7a) return code - 0x20;
  if (code >= 0x41 && code <= 0x5a) return code + 0x80;
  return code;
}

// One !tx argument list, split into quoted strings and expressions. Backslash escapes are
// consumed the way ACME consumes them, so `\"` is one drawn character -- TRExtPortCheck.asm
// has a line that measures 41 as typed and 39 on the screen because of exactly that.
function splitArguments(text) {
  const parts = [];
  for (let i = 0; i < text.length;) {
    const ch = text[i];
    if (ch === '"') {
      let j = i + 1;
      let literal = '';
      while (j < text.length && text[j] !== '"') {
        if (text[j] === '\\' && j + 1 < text.length) { literal += text[j + 1]; j += 2; continue; }
        literal += text[j];
        j += 1;
      }
      parts.push({ string: literal });
      i = j + 1;
    } else if (ch === ',' || /\s/.test(ch)) {
      i += 1;
    } else {
      let j = i;
      while (j < text.length && text[j] !== ',' && text[j] !== '"') j += 1;
      const expr = text.slice(i, j).trim();
      if (expr) parts.push({ expr });
      i = j;
    }
  }
  return parts;
}

const RETURN = 0x0d;
const SHIFT_RETURN = 0x8d;
const QUOTE = 0x22;
const CRSR_RIGHT = 0x1d;
const CRSR_LEFT = 0x9d;
const HOME = 0x13;
const CLEAR = 0x93;

const isControl = (b) => (b >= 0x00 && b <= 0x1f) || (b >= 0x80 && b <= 0x9f);

// Walks one message's byte stream and reports every ChrReturn issued while the cursor sits
// at column 0 *because the previous character filled the row*. `anchored` says whether the
// column was known rather than assumed: a message can be printed part-way along a row, and
// only a return, ChrClear or ChrHome earlier in the same message proves where column 0 was.
function scanMessage(stream) {
  let column = 0;
  let filled = false;
  let quoted = false;
  let anchored = false;
  const hits = [];
  for (const { byte, line } of stream) {
    if (byte === RETURN || byte === SHIFT_RETURN) {
      if (filled) hits.push({ line, anchored });
      column = 0; filled = false; quoted = false; anchored = true;
      continue;
    }
    if (isControl(byte) && !quoted) {
      if (byte === CRSR_RIGHT) column += 1;
      else if (byte === CRSR_LEFT) column = Math.max(0, column - 1);
      else if (byte === HOME || byte === CLEAR) { column = 0; anchored = true; }
      filled = false;
      continue;
    }
    if (byte === QUOTE) quoted = !quoted;
    column += 1;
    if (column >= COLUMNS) { column = 0; filled = true; } else { filled = false; }
  }
  return hits;
}

const TEXT_DIRECTIVE = /^\s*!(?:tx|text)\s+(.*?)\s*$/;
// A label on its own does not end a message; anything else that assembles does, because the
// cursor position stops being knowable once code runs between the two halves.
const LABEL_ONLY = /^\s*[A-Za-z_@+\-][A-Za-z0-9_]*:?\s*$/;

/**
 * Scan one ACME source for rows that fill all 40 columns and are then given a ChrReturn.
 * Returns { hits, unresolved }: hits carry the file and the line of the offending return,
 * unresolved names every expression that could not be evaluated, so a scan that understood
 * less than it looked like it did is visible rather than quietly clean.
 */
export function scanSource(file, text, symbols) {
  const escC = symbols.get('EscC') ?? 0x01;
  const argMask = symbols.get('EscArgMask') ?? 0xc0;
  const argSpaces = symbols.get('EscArgSpaces') ?? 0x80;
  const lines = text.split('\n');
  const hits = [];
  const unresolved = new Set();
  let stream = [];
  const flush = () => {
    for (const hit of scanMessage(stream)) hits.push({ file, ...hit, source: lines[hit.line - 1].trim() });
    stream = [];
  };

  lines.forEach((raw, index) => {
    const lineNumber = index + 1;
    const code = raw.trim().startsWith(';') ? '' : raw.split(';')[0];
    const directive = TEXT_DIRECTIVE.exec(code);
    if (!directive) {
      if (code.trim() && !LABEL_ONLY.test(code)) flush();
      return;
    }
    const parts = splitArguments(directive[1]);
    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      if (part.string !== undefined) {
        for (const ch of part.string) stream.push({ byte: petscii(ch), line: lineNumber });
        continue;
      }
      const value = evaluate(part.expr, symbols);
      if (value === null) { unresolved.add(part.expr); continue; }
      const byte = value & 0xff;
      if (byte === 0) { flush(); break; }          // the string terminator PrintString stops on
      if (byte !== escC) { stream.push({ byte, line: lineNumber }); continue; }
      // EscC takes one argument byte, which PrintString consumes: a colour reference draws
      // nothing, EscArgSpaces draws the number of spaces in its low bits.
      const next = parts[i + 1];
      if (!next || next.expr === undefined) continue;
      const arg = evaluate(next.expr, symbols);
      i += 1;
      if (arg === null) { unresolved.add(next.expr); continue; }
      if (((arg & 0xff) & argMask) === argSpaces) {
        for (let n = 0; n < ((arg & 0xff) & ~argMask & 0xff); n++) stream.push({ byte: 0x20, line: lineNumber });
      }
    }
  });
  flush();
  return { hits, unresolved: [...unresolved] };
}

const ACME_EXTENSIONS = new Set(['.asm', '.s', '.i', '.a']);

/** Every ACME source under `dir`, including the .s/.i/.a files a *.asm sweep would miss. */
export function acmeSources(dir) {
  const found = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true, recursive: true })) {
    if (!entry.isFile() || !ACME_EXTENSIONS.has(path.extname(entry.name))) continue;
    found.push(path.join(entry.parentPath ?? entry.path, entry.name));
  }
  return found.sort();
}

/** Scan a whole tree of ACME sources with one shared symbol table. */
export function scanTree(dir) {
  const files = acmeSources(dir);
  const symbols = loadSymbols(files);
  const hits = [];
  const unresolved = new Set();
  for (const file of files) {
    const result = scanSource(path.relative(dir, file), fs.readFileSync(file, 'utf8'), symbols);
    hits.push(...result.hits);
    for (const name of result.unresolved) unresolved.add(name);
  }
  return { hits, unresolved: [...unresolved].sort() };
}
