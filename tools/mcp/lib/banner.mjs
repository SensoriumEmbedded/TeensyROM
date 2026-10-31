// SPDX-License-Identifier: MIT
//
// The banner a TeensyROM answers VersionInfoToken with:
//
//   FW: TeensyROM+ v0.8.0.11            "(minimal)" follows when MinimalBoot answers
//       Sep 25 2026, 03:03:51
//   Teensy: 816MHz  62.1C  UID: 14470230
//   C128  NTSC Vid  60 Hz               main image only
//   Boot: complete                      main image only
//
// The UID is burned in at the factory, so it tells two boards of one type apart.

export function parseBanner(raw) {
  const version = raw.match(/TeensyROM\+? v[\d.]+/)?.[0] ?? null;
  const built = raw.match(/([A-Z][a-z]{2} [ \d]\d \d{4}), (\d\d:\d\d:\d\d)/);
  const uid = raw.match(/UID:\s*(\d+)/)?.[1] ?? null;
  const machine = raw.match(/^\s*(C64|C128)\s+(NTSC|PAL)\b/m);
  const boot = raw.match(/Boot: (complete|in progress)/)?.[1] ?? null;
  return {
    version,
    built: built ? `${built[1]}, ${built[2]}` : null,
    uid,
    minimal: /\(minimal\)/.test(raw),
    machine: machine?.[1] ?? null,
    video: machine?.[2] ?? null,
    boot,
    raw,
  };
}

// The '+' is the whole distinction between the cartridges, and it decides
// whether a given hex may be written at all.
export function boardKind(version) {
  if (!version) return null;
  return version.includes('TeensyROM+') ? 'tr-plus' : 'tr';
}
