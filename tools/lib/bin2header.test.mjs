// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { binToHeader, headerFileName, headerSymbolName } from './bin2header.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = (rel) => fs.readFileSync(path.join(root, rel));

test('names follow bin2header.py: bad characters and dots become underscores in the array name only', () => {
  assert.equal(headerSymbolName('SettingsMenu.prg'), 'SettingsMenu_prg');
  assert.equal(headerSymbolName('cia tod.prg'), 'cia_tod_prg');
  assert.equal(headerSymbolName('a-b+c*d\\e.bin'), 'a_b_c_d_e_bin');
  assert.equal(headerSymbolName('1541.prg'), '_1541_prg');
  assert.equal(headerFileName('SettingsMenu.prg'), 'SettingsMenu.prg.h');
  assert.equal(headerFileName('cia tod.prg'), 'cia_tod.prg.h');
});

test('a partial last row has no trailing comma, and PROGMEM goes before static', () => {
  const bytes = Buffer.from([0x01, 0x08, 0x0b, 0x08, 0xea, 0x07, 0x9e, 0x32, 0x39, 0x36, 0x33, 0x00, 0xff]);
  assert.equal(
    binToHeader(bytes, { name: 'x.prg', typemod: 'PROGMEM ' }),
    [
      '#ifndef X_PRG_H',
      '#define X_PRG_H',
      '',
      'PROGMEM static const unsigned char x_prg[] = {',
      '\t0x01, 0x08, 0x0b, 0x08, 0xea, 0x07, 0x9e, 0x32, 0x39, 0x36, 0x33, 0x00,',
      '\t0xff',
      '};',
      '',
      '#endif /* X_PRG_H */',
      '',
    ].join('\n'),
  );
});

test('an exact multiple of 12 ends without a trailing comma', () => {
  const out = binToHeader(Buffer.alloc(24, 0xab), { name: 'x.bin' });
  assert.match(out, /0xab,\n\t0xab(, 0xab){11}\n};/);
});

test('an empty file is an empty array', () => {
  assert.equal(
    binToHeader(Buffer.alloc(0), { name: 'e.bin' }),
    '#ifndef E_BIN_H\n#define E_BIN_H\n\nstatic const unsigned char e_bin[] = {\n};\n\n#endif /* E_BIN_H */\n',
  );
});

// The BASIC programs are converted, not assembled, so the committed headers in ROMs/ are
// exactly what bin2header.py made from the .prg files committed beside them.
for (const [prg, header] of [
  ['cia tod.prg', 'cia_tod.prg.h'],
  ['DMACheck.prg', 'DMACheck.prg.h'],
  ['empty.prg', 'empty.prg.h'],
  ['Load8Run.prg', 'Load8Run.prg.h'],
  ['shclock_tr.prg', 'shclock_tr.prg.h'],
  ['SID check.prg', 'SID_check.prg.h'],
]) {
  test(`matches the committed header for ${prg}`, () => {
    assert.equal(headerFileName(prg), header);
    const expected = read(`Source/Teensy/TRMenuFiles/ROMs/${header}`).toString('utf8').replaceAll('\r\n', '\n');
    assert.equal(binToHeader(read(`Source/C64/BASIC/${prg}`), { name: prg, typemod: 'PROGMEM ' }), expected);
  });
}

// The command-line wrapper, which the add-menu-program workflow uses for third-party files.
import { spawnSync } from 'node:child_process';
import os from 'node:os';

const cli = (...args) => spawnSync(process.execPath, [path.join(root, 'tools/bin2header.mjs'), ...args], { encoding: 'utf8' });

test('the CLI writes <file>.h beside the input, with -t normalised and -n honoured', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'b2h-'));
  try {
    const input = path.join(dir, '586220 ast.prg');
    fs.writeFileSync(input, Buffer.from([1, 2, 3]));
    const r = cli('-t', 'PROGMEM', '-n', 'a586220ast_Diagnostics', input);
    assert.equal(r.status, 0, r.stderr);
    const out = fs.readFileSync(path.join(dir, '586220_ast.prg.h'), 'utf8');
    assert.match(out, /^#ifndef A586220AST_DIAGNOSTICS_H$/m);
    assert.match(out, /^PROGMEM static const unsigned char a586220ast_Diagnostics\[\] = \{$/m);
    assert.match(out, /^\t0x01, 0x02, 0x03$/m);

    const explicit = path.join(dir, 'custom.h');
    assert.equal(cli('-o', explicit, input).status, 0);
    assert.ok(fs.existsSync(explicit));
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test('the CLI reports a missing file and bad usage', () => {
  assert.match(cli('/no/such/file.prg').stderr, /does not exist/);
  assert.equal(cli().status, 1);
  assert.match(cli('--bogus', 'x').stderr, /Unknown option --bogus/);
});
