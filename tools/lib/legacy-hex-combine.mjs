// Literal port of HexCombineUtil/HexCombine.exe's algorithm (source supplied by the user,
// 2026-09-13): concatenate every line of the MinBoot hex except its last two, then every
// line of the Main hex except its last two, then MinBoot's last two lines verbatim (its
// "start linear address" record, i.e. the boot vector, plus the EOF record). No address
// parsing or validation -- it trusts the two inputs don't overlap, which the lower/upper
// linker split guarantees.
//
// This exists to let this build run end-to-end without a Windows/.NET host, and to give
// Phase 4 something to diff HexCombine.exe's real output against. It is NOT yet the
// "matches byte for byte on real image pairs" proof the plan calls for before HexCombine.exe
// is removed from the build path -- that comparison still has to happen on Windows.
export function legacyCombineHex(minBootHexText, mainHexText) {
  const minLines = minBootHexText.split(/\r\n|\n/).filter((l, i, a) => !(i === a.length - 1 && l === ''));
  const mainLines = mainHexText.split(/\r\n|\n/).filter((l, i, a) => !(i === a.length - 1 && l === ''));
  const lines = [
    ...minLines.slice(0, -2),
    ...mainLines.slice(0, -2),
    minLines[minLines.length - 2],
    minLines[minLines.length - 1],
  ];
  return lines.join('\r\n') + '\r\n';
}
