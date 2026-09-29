---
name: teensyrom-build
description: How to build and flash TeensyROM firmware from this repo. Covers the C64 menu programs (npm run build:c64), the combined dual-boot firmware .hex for a TeensyROM or a TeensyROM+ (npm run build:tr / build:tr-plus), writing it to one or several boards over USB (npm run flash, tools/flash-firmware.mjs), and the tests and CI around them. Use whenever the user wants to build, compile, rebuild, flash, program or update firmware, make a .hex, find out what needs rebuilding after a change, check which build a board runs, or fix a failing build, for either cartridge type.
---

# Building TeensyROM

Two toolchains and a flasher, run in this order. Node is the only prerequisite for most
of it (`.nvmrc` pins the version), and the repo takes no npm dependencies.

| Step | Command | Produces | How-to |
|---|---|---|---|
| 1. C64 programs | `npm run build:c64` | C headers in `Source/Teensy/TRMenuFiles/ROMs/` | [c64-build.md](references/c64-build.md) |
| 2. Firmware | `npm run build:tr` or `npm run build:tr-plus` | `build/firmware/TeensyROM[+]_<version>_full.hex` | [firmware-build.md](references/firmware-build.md) |
| 3. Flash | `npm run flash` | that image running on a board, checked | [flash.md](references/flash.md) |
| Tests and CI | `npm test` | | [tests-and-ci.md](references/tests-and-ci.md) |

Why the build works this way (the two toolchains, the dual-boot image, the pinned tool
versions) is in [docs/Architecture/Build-System.md](/docs/Architecture/Build-System.md).
Read it before changing the build itself.

## What needs rebuilding

| You changed | Run |
|---|---|
| Anything under `Source/C64/`: source, a shared include (several projects include `c64defs.i`, `CommonDefs.i` and `Menu_Regs.i`), or a `.prg`/`.bin` some project converts | step 1 (all projects unless you know only one uses the file), then step 2 |
| `Source/Teensy/MinimalBoot/Common/Menu_Regs.h` (shared with the C64 side) | step 1 (all projects), then step 2 |
| Anything else under `Source/Teensy/` | step 2 |
| Nothing: you want a board on this commit's firmware | step 2 with `--force`, unless `build/firmware` already holds this commit's image (its build date is the commit's time, see [flash.md](references/flash.md)); then step 3 |
| The build tools under `tools/` | `npm test` |

Step 2 builds one cartridge type per run. Build both when both kinds of board need it.

## Easy to get wrong

- **TR and TR+ images are not interchangeable.** `--target tr` is the TeensyROM
  (Fab 0.2/0.3), `--target tr-plus` the TeensyROM+ (Fab 0.4). Both build cleanly. The
  flasher refuses the wrong one before writing; the SD-card updater refuses it only after
  the file has been carried over.
- **The build date names a commit, not a build.** It is the HEAD commit's time, so a build
  with uncommitted edits carries the previous commit's date, and rebuilding a commit gives
  the same hex. Commit first if the date on the board has to prove what it runs.
- **Skipping step 1 is silent.** The firmware embeds whatever headers are in the working
  tree, so a C64 change that was not rebuilt ships the old program.
- **Old images keep their name.** An image is named after the firmware version, not the
  commit, so `build/firmware` can hold an older build under the current name, and the
  flasher takes the newest file of either type. Check the build date it prints.

## On real hardware (for agents)

- Flashing changes a physical board. Say which board gets which image, and wait for the
  person's go-ahead; if they want to watch, wait for them.
- Start with `npm run flash -- --check`. It identifies every attached board and writes
  nothing.
- With more than one Teensy attached, never call `teensy_post_compile -port=...` yourself:
  with `-port` alone it falls back to auto-search and can write the other board. The
  flasher passes the board's whole address.
- Stop anything that holds a board's serial port (a TeensyROM app or API, a serial
  monitor) before flashing; a board it holds cannot answer and cannot be identified.
- TeensyROM settings survive a USB flash.

## Options

`npm run flash -- --help` and `npm run build:c64 -- --help` list their options.
`tools/build-firmware.mjs` has no `--help`; [firmware-build.md](references/firmware-build.md)
lists its options, and the top of the file explains them.
