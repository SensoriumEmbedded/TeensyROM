// Functions that may mask from flash, matched against `objdump -d -C` function names.
export const FLASH_MASKS_ALLOWED = [
  ['_reboot_Teensyduino_', 'the core restarting the Teensy, after which no bus cycle is served'],
  ['SPIClass::begin()', "the core's SPI setup, called only by SdFat's SPI card driver, which the SDIO card never uses"],
  ['SPIClass::beginTransaction(SPISettings)', "the core's SPI transaction, called only by the same SPI card driver"],
];

export const FLASH_START = 0x60000000;
export const FLASH_END = 0x70000000;

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
