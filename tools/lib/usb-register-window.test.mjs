import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { patchUsbRegisterWindow } from './usb-register-window.mjs';

const prime = (n) => '\tUSB1_ENDPTPRIME |= epmask;\n'.repeat(n);

test('the redirect lands after the includes and before the first USB1 register use', () => {
  const source = '#include "imxrt.h"\n#include "debug/printf.h"\n\nvoid f(void) { USB1_USBSTS = 1; }\n';
  const patched = patchUsbRegisterWindow(source, 'usb_serial.c');
  const redirect = patched.indexOf('#define IMXRT_USB1 (*usb1_regs_in_phi2_window())');
  assert.ok(redirect > patched.indexOf('#include "debug/printf.h"'));
  assert.ok(redirect < patched.indexOf('USB1_USBSTS'));
  assert.match(patched, /#undef IMXRT_USB1\n#define IMXRT_USB1/);
});

test('ENDPTPRIME goes through its own window, and its read-modify-writes become plain writes', () => {
  const patched = patchUsbRegisterWindow('#include "debug/printf.h"\n' + prime(6), 'usb.c');
  assert.match(patched, /#undef USB1_ENDPTPRIME\n#define USB1_ENDPTPRIME \(\(\*usb1_prime_in_phi2_window\(\)\)\.offset1B0\)/);
  assert.equal(patched.split('\tUSB1_ENDPTPRIME = epmask;\n').length - 1, 6);
  assert.doesNotMatch(patched, /USB1_ENDPTPRIME \|=/);
});

test('the firmware defines both functions the redirect calls, with C linkage', () => {
  const isrs = fs.readFileSync(new URL('../../Source/Teensy/MinimalBoot/Common/ISRs.c', import.meta.url), 'utf8');
  for (const fn of ['usb1_regs_in_phi2_window', 'usb1_prime_in_phi2_window']) {
    assert.match(isrs, new RegExp(`extern "C" FASTRUN IMXRT_REGISTER32_t \\*${fn}\\(\\)`), fn);
  }
});

test('a core whose text has drifted fails the build instead of shipping unguarded', () => {
  assert.throws(() => patchUsbRegisterWindow('#include "imxrt.h"\n', 'usb_serial.c'), /not found/);
  assert.throws(() => patchUsbRegisterWindow('#include "debug/printf.h"\n#include "debug/printf.h"\n', 'usb_serial.c'),
    /more than once/);
  assert.throws(() => patchUsbRegisterWindow('#include "debug/printf.h"\n' + prime(5), 'usb.c'), /Expected 6/);
});
