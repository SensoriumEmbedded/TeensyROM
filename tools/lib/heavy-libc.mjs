// Newlib routines that bring large tables or a lot of code into RAM1 with them. Teensy 4
// keeps .rodata in DTCM and default-placed code in ITCM, so every one of these costs
// stack or ITCM padding for something the firmware can do with a few lines of its own.
// sscanf and pow were removed once (sscanf's chain alone was ~15 KB of ITCM plus ~27 KB
// of JIS tables); build-firmware.mjs fails the minimal and main images if one comes back,
// since nothing else would show it but a smaller RAM1 report.
//
// Each entry is a symbol the routine always links, matched against `nm -C` output.
export const RAM1_HEAVY_LIBC = [
  ['__ssvfscanf_r', 'sscanf/scanf, which also links strtod'],
  ['__jp2uc', "newlib's JIS character tables (~27 KB), via scanf or the isw*() wide-char functions"],
  ['_strtod_l', 'strtod()/atof(), ~5 KB of code'],
  ['__pow_log_data', 'pow(), with ~4 KB of tables'],
  ['__exp_data', 'exp()/exp2()/pow(), with ~2 KB of tables'],
  ['__log_data', 'log(), with ~2 KB of tables'],
  ['__log2_data', 'log2(), with tables'],
  ['__powf_log2_data', 'powf(), with tables'],
  ['__exp2f_data', 'expf()/exp2f()/powf(), with tables'],
  ['__logf_data', 'logf(), with tables'],
  ['__log2f_data', 'log2f(), with tables'],
];

// Entries of RAM1_HEAVY_LIBC whose symbol is defined in the nm listing. A symbol is the
// last field of its line, so `_jp2uc_l` doesn't count as `__jp2uc`.
export function heavyLibcLinked(nmText) {
  const names = new Set(nmText.split(/\r?\n/).map((line) => line.trim().split(/\s+/).pop()));
  return RAM1_HEAVY_LIBC.filter(([symbol]) => names.has(symbol));
}
