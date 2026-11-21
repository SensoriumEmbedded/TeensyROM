import assert from 'node:assert/strict';

// Every USB1 register access in usb.c and usb_serial.c goes through the firmware's
// usb1_regs_in_phi2_window() (MinimalBoot/Common/ISRs.c), which holds it until it can
// finish before the next Phi2 rise. USB1_* in imxrt.h expand IMXRT_USB1 at each use, so
// redefining it after the includes reaches every access in the file, and each access
// calls the function again. ENDPTPRIME, which can take far longer than the others, goes
// through usb1_prime_in_phi2_window() instead. Applied to the build's private copy of the
// core only.

const INCLUDE_ANCHOR = '#include "debug/printf.h"\n';

const REDIRECT = INCLUDE_ANCHOR +
  'IMXRT_REGISTER32_t *usb1_regs_in_phi2_window(void);\n' +
  'IMXRT_REGISTER32_t *usb1_prime_in_phi2_window(void);\n' +
  '#undef IMXRT_USB1\n' +
  '#define IMXRT_USB1 (*usb1_regs_in_phi2_window())\n' +
  '#undef USB1_ENDPTPRIME\n' +
  '#define USB1_ENDPTPRIME ((*usb1_prime_in_phi2_window()).offset1B0)\n';

// Writing 0 to an ENDPTPRIME bit has no effect, so these read-modify-writes are plain
// writes that also pay for one more slow access.
const PRIME_RMW = 'USB1_ENDPTPRIME |= ';
const PRIME_RMW_COUNT = { 'usb.c': 6, 'usb_serial.c': 0 };

export function patchUsbRegisterWindow(source, file) {
  assert.equal(source.split(INCLUDE_ANCHOR).length, 2,
    'Expected Teensy core text not found, or found more than once: ' + INCLUDE_ANCHOR);
  assert.equal(source.split(PRIME_RMW).length - 1, PRIME_RMW_COUNT[file],
    `Expected ${PRIME_RMW_COUNT[file]} "${PRIME_RMW}" in ${file}`);
  return source.replace(INCLUDE_ANCHOR, REDIRECT).replaceAll(PRIME_RMW, 'USB1_ENDPTPRIME = ');
}

export const USB_REGISTER_WINDOW_FILES = Object.keys(PRIME_RMW_COUNT);
