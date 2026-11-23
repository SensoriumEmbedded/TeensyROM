// Code that masks interrupts while the C64 runs holds isrPHI2 off until it unmasks, and from
// flash one cache miss inside the mask is longer than a bus cycle, so the C64's access in that
// cycle goes unanswered (Constraints.md, "Thread-mode code that masks interrupts is on the
// ISR's path too"). MASKEDRUN keeps such a function in ITCM, but a small function without it
// lands in flash with no sign in the source once the compiler inlines it into a FLASHMEM
// caller. So build-firmware.mjs disassembles each image's flash and fails on a `cpsid`.
//
// Functions that may mask from flash, matched against `objdump -d -C` function names.
export const FLASH_MASKS_ALLOWED = [
  ['_reboot_Teensyduino_', 'the core restarting the Teensy, after which no bus cycle is served'],
  ['SPIClass::begin()', "the core's SPI setup, called only by SdFat's SPI card driver, which the SDIO card never uses"],
  ['SPIClass::beginTransaction(SPISettings)', "the core's SPI transaction, called only by the same SPI card driver"],
];

export const FLASH_START = 0x60000000;
export const FLASH_END = 0x70000000;

// The functions in `objdump -d -C` output that execute a `cpsid` at a flash address, other
// than the allowed ones: [name, address of its first cpsid], in address order.
export function masksInFlash(disassembly) {
  const allowed = new Set(FLASH_MASKS_ALLOWED.map(([name]) => name));
  const found = new Map();
  let fn = '(no function)';
  for (const line of disassembly.split(/\r?\n/)) {
    const header = line.match(/^[0-9a-f]+ <(.+)>:$/);
    if (header) { fn = header[1]; continue; }
    const insn = line.match(/^\s*([0-9a-f]+):\t[0-9a-f ]+\tcpsid\b/);
    if (!insn) continue;
    const address = parseInt(insn[1], 16);
    if (address < FLASH_START || address >= FLASH_END || allowed.has(fn) || found.has(fn)) continue;
    found.set(fn, address);
  }
  return [...found];
}
