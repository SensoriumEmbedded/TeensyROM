import assert from 'node:assert/strict';

// Every USB1 register access in usb.c and usb_serial.c goes through the firmware's
// usb1_regs_in_phi2_window() (MinimalBoot/Common/ISRs.c), which holds it until it can
// finish before the next Phi2 rise. USB1_* in imxrt.h expand IMXRT_USB1 at each use, so
// redefining it after the includes reaches every access in the file, and each access
// calls the function again. Applied to the build's private copy of the core only.

const INCLUDE_ANCHOR = '#include "debug/printf.h"\n';

const REDIRECT = INCLUDE_ANCHOR +
  'IMXRT_REGISTER32_t *usb1_regs_in_phi2_window(void);\n' +
  '#undef IMXRT_USB1\n' +
  '#define IMXRT_USB1 (*usb1_regs_in_phi2_window())\n';

export function patchUsbRegisterWindow(source) {
  assert.equal(source.split(INCLUDE_ANCHOR).length, 2,
    'Expected Teensy core text not found, or found more than once: ' + INCLUDE_ANCHOR);
  return source.replace(INCLUDE_ANCHOR, REDIRECT);
}

export const USB_REGISTER_WINDOW_FILES = ['usb.c', 'usb_serial.c'];
