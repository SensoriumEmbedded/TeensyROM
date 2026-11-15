import assert from 'node:assert/strict';

// Teensyduino 1.61.0's usb.c guards its endpoint queues with __disable_irq(), which sets
// PRIMASK and so holds off isrPHI2 as well as the USB interrupt it is meant to keep out.
// Every USB transfer queued -- a reply, a receive buffer handed back, a MIDI packet --
// then delays the PHI2 interrupt by up to the ATDTW loop's 2400 cycles, and a C64 IO
// access in that window reads an undriven bus or has its write latched late or never.
// These patches raise BASEPRI instead, which keeps out everything at priority 32 and
// above (USB, SysTick, timers) and leaves isrPHI2, at 16, running. They are applied to
// the build's private copy of the core, never the installed one; schedule_transfer() is
// static, so a sketch cannot override it.
//
// This holds only while nothing isrPHI2 calls reaches USB: an ISR at 16 that queued a
// transfer could now preempt one in progress.

function replaceOnce(source, before, after) {
  assert.equal(source.split(before).length, 2,
    'Expected Teensy core text not found, or found more than once: ' + before);
  return source.replace(before, after);
}

const MASK_HELPERS = `
// BASEPRI 32: isrPHI2 runs at NVIC priority 16 (Teensy.ino) and must not be held off.
static inline uint32_t usb_mask_raise(void)
{
	uint32_t prior, level = 32;
	__asm__ volatile("mrs %0, basepri" : "=r" (prior));
	__asm__ volatile("msr basepri_max, %0" : : "r" (level) : "memory");
	return prior;
}

static inline void usb_mask_restore(uint32_t prior)
{
	__asm__ volatile("msr basepri, %0" : : "r" (prior) : "memory");
}

`;

export function patchUsbIrqMasking(usbSource) {
  let s = usbSource;
  s = replaceOnce(s, '\nvoid usb_start_sof_interrupts(int interface)\n{\n\t__disable_irq();\n',
    MASK_HELPERS + 'void usb_start_sof_interrupts(int interface)\n{\n\tuint32_t usb_prior_mask = usb_mask_raise();\n');
  s = replaceOnce(s, '\t\tUSB1_USBINTR = intr | USB_USBINTR_SRE;\n\t}\n\t__enable_irq();\n',
    '\t\tUSB1_USBINTR = intr | USB_USBINTR_SRE;\n\t}\n\tusb_mask_restore(usb_prior_mask);\n');
  s = replaceOnce(s, '\t__disable_irq();\n\t//digitalWriteFast(1, HIGH);\n',
    '\tuint32_t usb_prior_mask = usb_mask_raise();\n\t//digitalWriteFast(1, HIGH);\n');
  // isrPHI2 can now run inside this loop, so a time limit could expire after a single
  // try and the status read below would be trusted without the tripwire holding.
  // 32 tries is about the stock 2400-cycle budget when nothing preempts it.
  s = replaceOnce(s,
    '\t\tuint32_t status, cyccnt=ARM_DWT_CYCCNT;\n' +
    '\t\tdo {\n' +
    '\t\t\tUSB1_USBCMD |= USB_USBCMD_ATDTW;\n' +
    '\t\t\tstatus = USB1_ENDPTSTATUS;\n' +
    '\t\t} while (!(USB1_USBCMD & USB_USBCMD_ATDTW) && (ARM_DWT_CYCCNT - cyccnt < 2400));\n',
    '\t\tuint32_t status, tries = 0;\n' +
    '\t\tdo {\n' +
    '\t\t\tUSB1_USBCMD |= USB_USBCMD_ATDTW;\n' +
    '\t\t\tstatus = USB1_ENDPTSTATUS;\n' +
    '\t\t} while (!(USB1_USBCMD & USB_USBCMD_ATDTW) && ++tries < 32);\n');
  s = replaceOnce(s, '\tendpoint->last_transfer = transfer;\n\t__enable_irq();\n',
    '\tendpoint->last_transfer = transfer;\n\tusb_mask_restore(usb_prior_mask);\n');
  return s;
}
