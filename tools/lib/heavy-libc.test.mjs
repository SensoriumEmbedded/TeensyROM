import test from 'node:test';
import assert from 'node:assert/strict';
import { heavyLibcLinked, RAM1_HEAVY_LIBC } from './heavy-libc.mjs';

const symbols = (list) => heavyLibcLinked(list).map(([symbol]) => symbol);

test('finds a heavy routine in nm -C output, LF or CRLF', () => {
  const nm = '0002fa54 T __ssvfscanf_r\n20000c10 D __pow_log_data\n00001234 T loop()\n';
  assert.deepEqual(symbols(nm), ['__ssvfscanf_r', '__pow_log_data']);
  assert.deepEqual(symbols(nm.replace(/\n/g, '\r\n')), ['__ssvfscanf_r', '__pow_log_data']);
});

test('a symbol that only contains a listed name is not a match', () => {
  const nm = '00034008 T _jp2uc_l\n00002000 T my__exp_data_helper\n0000abcd t _strtod_lx\n';
  assert.deepEqual(symbols(nm), []);
});

test('a clean listing matches nothing', () => {
  assert.deepEqual(symbols('00000eb0 T M2SOnNoteOn(unsigned char, unsigned char, unsigned char)\n'), []);
});

test('every listed symbol can be matched', () => {
  const nm = RAM1_HEAVY_LIBC.map(([symbol], i) => `${i.toString(16).padStart(8, '0')} R ${symbol}`).join('\n');
  assert.equal(heavyLibcLinked(nm).length, RAM1_HEAVY_LIBC.length);
});
