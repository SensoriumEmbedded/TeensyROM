# MPE 1.2.24 update for TeensyROM 0.8.0.8

MPE integration **1.2.24** adds self-contained `.MPE` cartridge launching to
Travis's text interface, using the existing compiled host library **1.2.23**.
It builds on integration commit `f84d3ec1` and retains TeensyROM **0.8.0.8**,
the text menu, settings, ordinary cartridges, large-CRT SD bank swapping,
and Travis's `FXUtil.cpp` / `.h` RAM-saving rollback.

## Install and launch

Use these matching packages from the
[public VM downloads](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/vms):

| Package | Release or package revision | Direct SD content |
| --- | --- | --- |
| NESVM | 1.2.1 | `.nes` |
| GBVM | 1.2.24 | `.gb`, `.gbc`, `.gc` |
| GGVM | 1.2.24 | `.gg` |
| DoomVM | 1.2.1, unchanged | Package launcher or supported content |

Package revision numbers do not change the packages' declared minimum host
requirements. Follow their individual controls and media instructions.
[Matching console developer sources and rebuild instructions](../mpe/review/console-sources/README.md)
accompany this handoff separately from the runtime downloads.
Installed packages keep their launcher and `VMS/` directory together at
the SD root. Direct ROM selection requires the corresponding valid package;
missing, corrupt, ambiguous, and unsupported console packages fail before
the stock unknown-file-as-PRG path. The GUI firmware offered by the public
VM project is a separate firmware choice.

A properly built **MGC1 `.MPE` cartridge** already contains its VM engine,
C64 client and game content. Copy that one file anywhere on SD and select
it from the text browser. It does not need `/VMS`, `launch.vml`, or an
extracted game directory. Legacy MGC1 `.CRT` files are also recognized;
ordinary CRT files retain their existing loader. Renaming an ordinary CRT
to `.MPE` does not create a game container.

The main image validates the container and embedded engine before recording
the exact cartridge path in EEPROM. Minimal boot consumes that request and
enters the independent host, which validates and mounts the container again.
Embedded game files are read-only. Games using host save services write flat
sidecar files beside the cartridge, identified by its filename and package ID.
Keep those files with the original cartridge filename when moving or backing
up saves. USB and virtual disk-image `.MPE` sources are rejected before the
ordinary loader. Complete game compatibility still depends on the embedded
engine and supported game data.

## Build and identify the output

Use Node.js 24 or later, Arduino CLI, Teensyduino core **1.61.0**, its GNU Arm
**11.3.1** toolchain, and the existing TeensyROM library dependencies. From
the repository root, substitute your installed paths:

```powershell
npm run build:mpe -- --arduino-cli "C:/Tools/arduino-cli.exe" --arduino-data "C:/Arduino15" --arduino-user "C:/Arduino" --out "C:/MPE-build"
```

The first `--` forwards npm arguments to `node mpe/tools/build.mjs --mode mpe`.
Keep the output path short on Windows. The isolated build stages source and
core files without modifying the installed SDK. It combines minimal boot at
`0x60000000`, the text main image at `0x60060000`, and MPE at `0x60280000`.
The output is **`TeensyROM+_0.8.0.8_MPE-1.2.24_full.hex`**. The output directory's
`latest.json` identifies its path, SHA-256, source inputs, library version and
image layout. Use the combined HEX; the individual host image is incomplete.

Existing `build`, `build:tr`, and `build:tr-plus` npm entries still invoke the
separate two-image builder. They do not enable this MPE integration.

## Source and redistribution

The public MGC1 parser, text-browser integration and boot dispatch are in
this repository. The unchanged [host library](../mpe/library/README.md)
already contains the runtime, compressed virtual-file and sidecar-save
services, and Prism/Prism+ display support. Its wrapper calls `mpeHostSetup()`
and `mpeHostLoop()`; do not compile the historical source host alongside it.

New Prism+ implementation source remains private. The
[host integration permission](../mpe/library/LICENSE-HOST-INTEGRATION.txt)
permits linking the unchanged covered contributions into your firmware and
distributing the combination. Earlier MIT grants and third-party rights
remain intact. Keep the component manifest, licenses, matching relinking
materials and corresponding library source with the firmware distribution.
The library's `Relink-SDK.zip` covers the independent host; the matching
[1.2.24 minimal/text-main source bundle](../mpe/review/firmware-1.2.24-library-sources.zip)
covers the other two images, including their exact core/library source,
614 compilation recipes, per-image bootdata and linker profiles. All 1,576
ZIP members were hash-verified after extraction; recompiling the restored
`yield.cpp` and each image's bootdata matched the final build objects after
debug information was removed.
Their application source remains in this repository. See
[third-party notices](../THIRD-PARTY-NOTICES.md).

## Verification

The checked [combined firmware](../mpe/review/TeensyROM+_0.8.0.8_MPE-1.2.24_full.hex)
has SHA-256 `b66dbd2c5079621156196ec357a6e6302f896cf7c103711db50b4c87c8ce51e6`.
The [build and verification record](../mpe/review/host-1.2.24-verification.json)
and [test log](../mpe/review/host-1.2.24-tests.log) record the matching inputs,
four real VM package preflights, 42 console route checks, 512 PAL/NTSC DMA
byte cases, 221 protected upstream files and byte-identical stock/stock-plus
comparison builds. The main image retains 26,592 bytes between linked data
and stack, with 1,125,376 bytes of steady-state self-update headroom.

The [console package update](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/blob/main/docs/CONSOLE-STARTUP-UPDATE.md)
describes the NES/GB/GG BASE-before-SID fixes, receiver regressions and source
rebuilds. Use its corrected packages even when launching directly by ROM name.

With a C++17 compiler and matching VM packages under a test SD root:

```powershell
node --test mpe/tools/build-identity.test.mjs mpe/tools/hex.test.mjs mpe/tests/game-cart-launch.test.mjs mpe/tests/startup.test.mjs
node mpe/tests/direct-console-launch.mjs --packages "C:/MPE-SD" --cxx "C:/Tools/mingw64/bin/g++.exe"
node mpe/tools/verify.mjs --build "C:/MPE-build/latest.json" --packages "C:/MPE-SD" --cxx "C:/Tools/mingw64/bin/g++.exe"
```

The `.MPE` native fixture exercises actual text-browser, extension, parser
and minimal-boot functions, including new and legacy suffixes, compression,
CRC failures, EEPROM verification, unsupported sources, missing host-image
recovery, and ordinary 1/2 MiB CRT fallback. Separate host file-service checks
cover compressed/interleaved reads and save write/flush/rename/cold reopen
without `/VMS`. Source compatibility tests do not execute the compiled Prism+
archive, and synthetic launch tests do not emulate complete games.

The builder checks board/library identity, source drift, HEX syntax and
checksums, image partitions, flash bounds and self-update headroom. These
checks protect generated output while preserving the legacy updater.
PAL and NTSC hardware acceptance remains required for gameplay, audio,
display, save persistence, menu/reset behavior, self-update, and ordinary
and large cartridges. The previous build's evidence remains in the
[historical MPE 1.2.23 note](MPE-Update-1.2.23.md).
