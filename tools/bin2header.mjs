// SPDX-License-Identifier: MIT
//
// Converts one file into a C header holding it as a byte array: the command-line face of
// tools/lib/bin2header.mjs, for adding a program you didn't assemble here (a .prg, .crt, .sid...)
// to the firmware's menu. It replaces `python Source/C64/bin2header.py`, with the options
// people used:
//
//   node tools/bin2header.mjs [-t <type modifier>] [-n <array name>] [-o <output file>] <file>
//
//   -t   text placed before `static`, normally PROGMEM to keep the array in flash
//   -n   array name (default: the file name with "." and other non-identifier characters
//        replaced by "_"; use this when the file name starts with a digit)
//   -o   output file (default: <file>.h beside the input, with spaces in the name made "_")
//
// Unlike the Python script it also accepts `-t PROGMEM` without the trailing space, which
// the script emitted as `PROGMEMstatic`.
import fs from 'node:fs';
import path from 'node:path';
import { binToHeader, headerFileName } from './lib/bin2header.mjs';

const USAGE = 'Usage: node tools/bin2header.mjs [-t <type modifier>] [-n <array name>] [-o <output file>] <file>';

function parse(argv) {
  const opts = { typemod: '', name: null, output: null, input: null };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const value = () => {
      if (i + 1 >= argv.length) throw new Error(`${arg} needs a value\n${USAGE}`);
      return argv[++i];
    };
    if (arg === '-t' || arg === '--typemod') opts.typemod = value();
    else if (arg === '-n' || arg === '--hname') opts.name = value();
    else if (arg === '-o' || arg === '--output') opts.output = value();
    else if (arg.startsWith('-')) throw new Error(`Unknown option ${arg}\n${USAGE}`);
    else if (opts.input === null) opts.input = arg;
    else throw new Error(`Unexpected extra argument ${arg}\n${USAGE}`);
  }
  if (opts.input === null) throw new Error(`Missing <file>\n${USAGE}`);
  if (opts.typemod && !/\s$/.test(opts.typemod)) opts.typemod += ' ';
  return opts;
}

try {
  const opts = parse(process.argv.slice(2));
  if (!fs.existsSync(opts.input)) throw new Error(`File "${opts.input}" does not exist`);
  const base = path.basename(opts.input);
  const output = opts.output ?? path.join(path.dirname(opts.input), headerFileName(base));
  fs.writeFileSync(output, binToHeader(fs.readFileSync(opts.input), { name: opts.name ?? base, typemod: opts.typemod }));
  console.log(`Wrote ${output}`);
} catch (error) {
  console.error(`error: ${error.message}`);
  process.exit(1);
}
