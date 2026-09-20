---
name: add-menu-program
description: Steps for adding a C64 program/binary to TeensyROM's built-in menu (converting a .prg/.crt/.bin into a PROGMEM header via tools/bin2header.mjs, dropping it into Source/Teensy/TRMenuFiles/, and wiring it into MainMenuItems.h). Use this whenever the user wants to add a program, game, demo, cart, SID, or picture file to the TeensyROM menu, or asks to "add X to the menu" / "add a new ROM" for this project.
---

# Adding a program to TeensyROM's built-in menu

TeensyROM embeds its menu content as `PROGMEM` byte arrays compiled directly into the
firmware. Adding a file means: convert it to a header, drop the header where the build
expects it, then wire two things into `MainMenuItems.h`.

## 0. Get what you need up front

Ask for whatever isn't given:
- The source file (a `.prg` in the common case; could be `.crt`/`.bin`/`.sid`/`.kla` etc.)
- Which existing menu directory it goes in (see the mapping below — if the user names a
  directory that doesn't obviously match, check the table at the bottom of
  `Source/Teensy/MainMenuItems.h` rather than guessing)
- The display name to show in the TeensyROM menu (this is free text the user chooses —
  often quite different from the filename, don't just reuse the filename)
- Target directory for the header file, if not the default (`Source/Teensy/TRMenuFiles/ROMs/`)

## 1. Convert the binary to a header

```
node tools/bin2header.mjs -t PROGMEM "<path-to-file>"
```

This writes `<file>.h` next to the source (e.g. `MyProg.prg` → `MyProg.prg.h`) unless `-o`
is given. The array name (`-n`) defaults to the filename with `.` replaced by `_` — this
is the identifier you'll reference in step 3, so don't rename it after the fact without
also updating the reference.

**Gotcha — check the first character of the filename before running.** If it starts with a
digit, the default array name would start with `_`, which collides with C's reserved-
identifier rules. The existing convention in this codebase (see `a586220ast_Diagnostics.h`,
`a781220_Dead_Test.h`) is to override with `-n "a<name>"` instead — a leading letter, not an
underscore.

**Verify PROGMEM actually landed correctly.** Open the generated `.h` and check the array
declaration line reads exactly:
```c
PROGMEM static const unsigned char <hname>[] = {
```
`tools/bin2header.mjs` adds the space after `-t PROGMEM` for you (the old Python script did
not, and produced `PROGMEMstatic const...`), so this check is a sanity check rather than a
known trap.

## 2. Place the header

Move/copy the generated `.h` into `Source/Teensy/TRMenuFiles/ROMs/` (the default for
programs/carts/binaries) unless the user specified a different subfolder — the codebase
also uses `TRMenuFiles/Text_PETSCII/`, `TRMenuFiles/SIDs/`, and `TRMenuFiles/Pics/` for
those respective content types.

## 3. Wire it into MainMenuItems.h

Two edits, both in `Source/Teensy/MainMenuItems.h`:

**a) Add the `#include`**, grouped with the other includes of the same content type near
the top of the file:
```c
#include "TRMenuFiles/ROMs/<file>.h"
```

**b) Add a row at the bottom of the target directory's array.** Each menu directory is a
`StructMenuItem dir<Name>[]` array. Find the right one from the human-readable name using
the top-level directory table near the end of the file (around line ~313), which maps
display names to array identifiers, e.g.:
```c
rtDirectory, IOH_None, (char*)"/Utilities", (uint8_t*)dirUtilities, sizeof(dirUtilities),
```
tells you "Utilities" is `dirUtilities`. Append a new row at the bottom of that array,
matching its existing column alignment:
```c
rtFilePrg  , IOH_None         , (char*)"<Display Name>"           , (uint8_t*)<hname>    , sizeof(<hname>) ,
```
- `rtFilePrg` is correct for a plain `.prg`; use `rtFileCrt`/`rtFileSID`/`rtFileKla`/
  `rtBin16k`/`rtBin8kHi`/`rtBin8kLo` etc. to match the source format — check a sibling entry
  in the same array for the type actually used for that format.
- `IOH_None` is correct unless the program needs a specific IO handler active while it runs
  (e.g. `IOH_Swiftlink` for a terminal program, `IOH_TR_BASIC` for BASIC-with-TR-commands) —
  if unsure, ask, or check what a similar existing entry uses.
- `<hname>` must be the exact array name bin2header generated (or the `-n` override), used
  identically for both the pointer cast and the `sizeof()`.

That's the whole change — `sizeof(dir<Name>)` in the top-level directory table is computed
automatically, so nothing else needs to be touched for a program going into an existing
directory. (Adding a *new* top-level directory, rather than a program inside an existing
one, would also need a new row in that top-level table — out of scope for this checklist
unless asked.)
