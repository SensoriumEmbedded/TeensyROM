import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { patchUsbRegisterWindow } from './usb-register-window.mjs';

test('the redirect lands after the includes and before the first USB1 register use', () => {
  const source = '#include "imxrt.h"\n#include "debug/printf.h"\n\nvoid f(void) { USB1_USBSTS = 1; }\n';
  const patched = patchUsbRegisterWindow(source);
  const redirect = patched.indexOf('#define IMXRT_USB1 (*usb1_regs_in_phi2_window())');
  assert.ok(redirect > patched.indexOf('#include "debug/printf.h"'));
  assert.ok(redirect < patched.indexOf('USB1_USBSTS'));
  assert.match(patched, /#undef IMXRT_USB1\n#define IMXRT_USB1/);
});

test('the firmware defines the function the redirect calls, with C linkage', () => {
  const isrs = fs.readFileSync(new URL('../../Source/Teensy/MinimalBoot/Common/ISRs.c', import.meta.url), 'utf8');
  assert.match(isrs, /extern "C" FASTRUN IMXRT_REGISTER32_t \*usb1_regs_in_phi2_window\(\)/);
});

test('a core whose text has drifted fails the build instead of shipping unguarded', () => {
  assert.throws(() => patchUsbRegisterWindow('#include "imxrt.h"\n'), /not found/);
  assert.throws(() => patchUsbRegisterWindow('#include "debug/printf.h"\n#include "debug/printf.h"\n'), /more than once/);
});
