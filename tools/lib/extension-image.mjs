// SPDX-License-Identifier: MIT
//
// The third flash image. An extension module needs 96 KiB of ITCM and 192 KiB
// of DTCM at fixed addresses, which the ordinary minimal image cannot give it
// while still holding a megabyte of cartridge. So the loader builds a separate
// image with that memory map and boots into it, and the other two images shrink
// to make room.
//
// Nothing here generates code or rewrites a source file: it edits the stock
// Teensy linker script and bootdata, and every edit must match exactly once.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { MAIN_BASE, VM_BASE, VM_LIMIT } from './hex.mjs';

const read = (p) => fs.readFileSync(p, 'utf8');

// An anchor that has drifted would otherwise drop its replacement silently --
// worst of all for the ASSERTs below, where a miss removes the entire safety
// net without a word. Require exactly one match.
function replaceOnce(source, before, after) {
  assert.equal(source.split(before).length, 2,
    'Expected linker text not found, or found more than once: ' + before);
  return source.replace(before, after);
}

// Reserving the slot costs the other two images flash, so say so out loud.
// Compiled into minimal and main so they can hand off to the extension image.
// Not the host itself -- that is FeatVMHost, third image only.
export const VM_EXTENSIONS_DEFINE = ' -DVM_EXTENSIONS_ENABLED';

export function flashBudget() {
  return {
    minimalKB: 384,
    mainKB: (VM_BASE - MAIN_BASE) / 1024,
    extensionKB: (VM_LIMIT - VM_BASE) / 1024,
    stockMinimalKB: 7936,
    stockMainKB: 7552,
  };
}

export function minimalLinkerScript(linkers) {
  return replaceOnce(read(path.join(linkers, 'imxrt1062_t41.ld.orig')), 'LENGTH = 7936K', 'LENGTH = 384K');
}

export function mainLinkerScript(linkers) {
  return replaceOnce(read(path.join(linkers, 'imxrt1062_t41.ld.upper')), 'LENGTH = 7552K',
    `LENGTH = ${(VM_BASE - MAIN_BASE) / 1024}K`);
}

// The extension image: relocated to its own slot, its ITCM footprint pinned and
// its heap capped, with five ASSERTs that turn a host/module layout regression
// into a link error instead of a hang on hardware.
export function extensionLinkerScript(linkers) {
  let ld = read(path.join(linkers, 'imxrt1062_t41.ld.orig'));
  ld = replaceOnce(ld, 'ORIGIN = 0x60000000, LENGTH = 7936K',
    `ORIGIN = 0x${VM_BASE.toString(16)}, LENGTH = ${(VM_LIMIT - VM_BASE) / 1024}K`);
  // Pin the host to six 32 KiB ITCM blocks, so the module window at 0x18000
  // cannot be pushed around by a change in host code size.
  ld = replaceOnce(ld, '_itcm_block_count = (SIZEOF(.text.itcm) + SIZEOF(.ARM.exidx) + 0x7FFF) >> 15;',
    '_itcm_block_count = 6;');
  // The host keeps a 16 KiB heap immediately after its own bss. Everything from
  // 0x20014000 up belongs to the module.
  ld = replaceOnce(ld, '_heap_start = ADDR(.bss.dma) + SIZEOF(.bss.dma);', '_heap_start = ALIGN(_ebss, 32) + 32;');
  ld = replaceOnce(ld, '_heap_end = ORIGIN(RAM) + LENGTH(RAM);', '_heap_end = _heap_start + 16384;');
  ld = replaceOnce(ld, '_teensy_model_identifier = 0x25;',
    `_teensy_model_identifier = 0x25;
      _vm_data_start = 0x20014000; _vm_data_end = 0x20044000;
      ASSERT(_etext <= 0x18000, "Host code overlaps the module ITCM window")
      ASSERT(_heap_end <= _vm_data_start, "Host heap overlaps the module DTCM window")
      ASSERT(_estack - _vm_data_end >= 49152, "Shared stack below 48 KiB")
      ASSERT(SIZEOF(.bss.dma) == 0, "Host globals overlap the guest RAM2 arena")
      ASSERT(SIZEOF(.bss.extram) == 0, "Host requires PSRAM, which the module map does not reserve")`);
  return ld;
}

export function extensionBootdata(linkers) {
  return replaceOnce(read(path.join(linkers, 'bootdata.c.orig')), '0x60000000,', `0x${VM_BASE.toString(16)},`);
}

// Teensy 4.x has no supported "No USB" build: boards.txt has the menu entry
// commented out, so core 1.61 carries only partial USB_DISABLED support and
// two references to a USB it did not compile are left unguarded. Only the
// extension image builds that way, so both patches are applied to the build's
// private copy of the core and never to the installed one.
//
// The fault handler's reboot-responsiveness loop pumps USB so a crashed sketch
// can still be re-flashed. With no USB there is nothing to pump; the loop still
// counts out its eight seconds and reboots.
export function patchStartupForUsbDisabled(startupSource) {
  return replaceOnce(startupSource, '\n\t\tusb_isr();\n',
    '\n#ifndef USB_DISABLED\n\t\tusb_isr();\n#endif\n');
}

export function patchYieldForUsbDisabled(yieldSource) {
  return replaceOnce(yieldSource, 'if (Serial.available()) serialEvent();',
    '#ifndef USB_DISABLED\n\tif (Serial.available()) serialEvent();\n#endif');
}
