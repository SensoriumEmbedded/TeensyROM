import test from 'node:test';
import assert from 'node:assert/strict';
import { masksInFlash, FLASH_MASKS_ALLOWED } from './flash-masks.mjs';

// Shaped like `objdump -d -C` on the 0.9.0.1 TR+ main image, where AbortDMAUnless's mask had
// been inlined into WaitForDMAState and ran from flash.
const DISASSEMBLY = `
Disassembly of section .text.code:

600616a8 <WaitForDMAState(unsigned char, unsigned long)>:
600616a8:\tb538      \tpush\t{r3, r4, r5, lr}
600616ec:\tb672      \tcpsid\ti
600616ee:\t4b0c      \tldr\tr3, [pc, #48]\t; (60061720 <WaitForDMAState(unsigned char, unsigned long)+0x78>)
600616f8:\tb662      \tcpsie\ti
60061720:\t20001234 \t.word\t0x20001234

60075814 <_reboot_Teensyduino_>:
60075820:\tb672      \tcpsid\ti

6006fc88 <SampleWatchCommand()>:
6006fcd8:\tb672      \tcpsid\ti
6006fd72:\tb672      \tcpsid\ti

Disassembly of section .text.itcm:

00000020 <ReachedOrStartDisable(unsigned char)>:
  20:\tb672      \tcpsid\ti
  2a:\tb662      \tcpsie\ti
`;

test('names each function that masks from flash once, at its first cpsid', () => {
  assert.deepEqual(masksInFlash(DISASSEMBLY), [
    ['WaitForDMAState(unsigned char, unsigned long)', 0x600616ec],
    ['SampleWatchCommand()', 0x6006fcd8],
  ]);
});

test('reads CRLF output the same', () => {
  assert.deepEqual(masksInFlash(DISASSEMBLY.replace(/\n/g, '\r\n')).map(([name]) => name),
    ['WaitForDMAState(unsigned char, unsigned long)', 'SampleWatchCommand()']);
});

test('a mask in ITCM, an unmask, or the word cpsid in a comment is not a finding', () => {
  const itcmOnly = DISASSEMBLY.split('\n').filter((line) => !/^6/.test(line)).join('\n');
  assert.deepEqual(masksInFlash(itcmOnly), []);
  assert.deepEqual(masksInFlash('60061000 <f()>:\n60061000:\tb662      \tcpsie\ti\n' +
    '60061002:\t4b0c      \tldr\tr3, [pc, #48]\t; cpsid i\n'), []);
});

test('cpsid f masks isrPHI2 as well', () => {
  assert.deepEqual(masksInFlash('60061000 <f()>:\n60061004:\tb671      \tcpsid\tf\n'), [['f()', 0x60061004]]);
});

test('every allowed function is skipped', () => {
  const allowedOnly = FLASH_MASKS_ALLOWED.map(([name], i) =>
    `${(0x60070000 + i * 16).toString(16)} <${name}>:\n${(0x60070004 + i * 16).toString(16)}:\tb672      \tcpsid\ti`).join('\n');
  assert.deepEqual(masksInFlash(allowedOnly), []);
});
