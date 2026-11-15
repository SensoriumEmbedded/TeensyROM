import test from 'node:test';
import assert from 'node:assert/strict';
import { patchUsbIrqMasking } from './usb-irq-mask.mjs';
import { STOCK_USB_C as STOCK } from './usb-irq-mask.fixture.mjs';


const body = (source, signature) => {
  const start = source.indexOf(signature);
  return source.slice(start, source.indexOf('\n}\n', start));
};

test('neither USB section sets PRIMASK, so isrPHI2 is never held off', () => {
  const patched = patchUsbIrqMasking(STOCK);
  for (const fn of ['void usb_start_sof_interrupts(', 'static void schedule_transfer(']) {
    const text = body(patched, fn);
    assert.doesNotMatch(text, /__disable_irq|__enable_irq/, fn);
    assert.match(text, /usb_mask_raise\(\)[\s\S]*usb_mask_restore\(usb_prior_mask\)/, fn);
  }
});

test('the ATDTW tripwire is bounded by tries, not by elapsed cycles', () => {
  const text = body(patchUsbIrqMasking(STOCK), 'static void schedule_transfer(');
  assert.doesNotMatch(text, /ARM_DWT_CYCCNT/);
  assert.match(text, /\+\+tries < 32/);
});

test('the mask leaves priority 16 (isrPHI2) running', () => {
  assert.match(patchUsbIrqMasking(STOCK), /level = 32;[\s\S]*msr basepri_max/);
});

test('a core whose text has drifted fails the build instead of shipping unpatched', () => {
  assert.throws(() => patchUsbIrqMasking(STOCK.replace('cyccnt < 2400', 'cyccnt < 4800')), /not found/);
});
