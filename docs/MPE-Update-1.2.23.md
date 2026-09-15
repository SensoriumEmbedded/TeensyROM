# MPE 1.2.23 update for TeensyROM 0.8.0.8

This integration starts from Travis's upstream commit `80ba6378` and keeps
TeensyROM version **0.8.0.8**. The independent MPE host advances to **1.2.23**
for **NESVM 1.2.0** and **DoomVM 1.2.1**, available in the
[public 1.2.23-r2 release](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/releases/tag/v1.2.23-r2).
The release's GUI firmware is a separate build; this integration retains
Travis's text menu, settings, ordinary cartridges, and large-CRT SD bank swapping.

## Build and identify the output

Install Node.js 24 or later, Arduino CLI, Teensyduino core **1.61.0** and its
GNU Arm **11.3.1** toolchain, plus the existing TeensyROM library dependencies.
From the repository root, substitute the paths to your installed SDK and libraries:

```powershell
npm run build:mpe -- --arduino-cli "C:/Tools/arduino-cli.exe" --arduino-data "C:/Arduino15" --arduino-user "C:/Arduino" --out "C:/MPE-build"
```

The first `--` forwards npm arguments. This invokes
`node mpe/tools/build.mjs --mode mpe`. Use a short output path on Windows.
The builder stages source and the core in an isolated directory, leaving
the installed SDK unchanged. It produces three linked images and combines them:

| Image | Flash start | Purpose |
| --- | --- | --- |
| Minimal | `0x60000000` | Startup and one-shot VM dispatch |
| Main | `0x60060000` | Travis's text interface and ordinary cartridges |
| MPE host | `0x60280000` | Complete MPE runtime with Prism+ support |

For this base, the combined artifact is
`TeensyROM+_0.8.0.8_MPE-1.2.23_full.hex`. The output directory's `latest.json`
points to the run directory, combined artifact, SHA-256, library identity,
input hashes, and per-image layout. Use the combined HEX for firmware updates;
the individual MPE image is not a complete firmware.

The existing `npm run build:tr` and `npm run build:tr-plus` remain on
`tools/build-firmware.mjs`, the two-image builder. Its header's anticipated
MPE integration does not make it the three-image command. The default
`npm run build` entry is unchanged.

## Runtime and source boundaries

The supplied [host library](../mpe/library/README.md) contains the complete
MPE runtime, including original Prism and new Prism+ services. A small public
wrapper calls `mpeHostSetup()` and `mpeHostLoop()`; the ordinary firmware is
compiled from this repository's public source. Do not also compile the
historical source-based MPE host into the third image.

The new MHS Prism+ implementation remains private. Its
[integration permission](../mpe/library/LICENSE-HOST-INTEGRATION.txt) permits
linking the unchanged covered contributions into your own firmware and
distributing that combination. Public interface/glue is MIT-licensed;
earlier MIT grants and third-party rights remain intact. Keep the licenses,
component manifest, and matching relinking materials with distributions.
The library's `Relink-SDK.zip` supplies corresponding library source and
objects, with instructions to rebuild those libraries and relink the host.
The matching [minimal/text-main library source bundle](../mpe/review/firmware-1.2.23-library-sources.zip)
supplies the libraries and build provenance for the other two images;
their application source and builder remain in this repository. Include
both sets of materials when distributing the combined firmware.
See [third-party notices](../THIRD-PARTY-NOTICES.md).

Install complete VM packages as described in [VM usage](MPE_VM_Usage.md).
Direct SD routes are `.nes` to NESVM, `.gb` / `.gbc` / `.gc` to GBVM, and
`.gg` to GGVM. A valid compatible package is required; failed preflight for
known console extensions cannot fall through to PRG loading. NESVM 1.2.0
uses the updated Prism+ host services; DoomVM 1.2.1 uses its own display modes.
The GUI's self-contained MGC1 `.MPE` launch flow is **not enabled here**.

## Checks and remaining acceptance

Travis's `FXUtil.cpp` / `.h` rollback at `80ba6378` is retained. Build-time
HEX checksum, partition, flash bounds, and self-update headroom checks guard
the generated combination, whose data records are at most 16 bytes. The MPE
build also checks board identity, library identity and source drift. These
checks validate the generated artifact; they do not change the legacy updater.

Run the focused tests with a C++17 host compiler and the matching packages
extracted under one SD root containing `VMS/NESVM` and `VMS/DOOMVM`:

```powershell
node --test mpe/tools/build-identity.test.mjs mpe/tools/hex.test.mjs mpe/tests/sync-host.test.mjs
node mpe/tests/direct-console-launch.mjs --packages "C:/MPE-SD" --cxx "C:/Tools/mingw64/bin/g++.exe"
node mpe/tools/verify.mjs --build "C:/MPE-build/latest.json" --packages "C:/MPE-SD" --cxx "C:/Tools/mingw64/bin/g++.exe"
```

The 14 September 2026 three-image build and verifier passed, including
221 unchanged protected upstream files, library/linked-entry checks,
10 text-button scenarios, 40 launch-route checks, two released-package
preflights, and 512 PAL/NTSC DMA byte cases. Stock and stock-plus outputs
matched their upstream-baseline builds byte for byte. The combined MPE
firmware SHA-256 is:

```
2cf0a6b144730bbee92d04bbd812b58163faaafa5acc1ce415d7a70794321156
```

The focused native harness exercises the actual registry and launch helpers,
current NES/Doom engine and client files, malformed and missing packages,
ordinary CRT fallback, and PAL/NTSC DMA timing selection. Build reports,
native tests and relinking checks are separate from physical acceptance.
The historical renderer source tests check compatibility; they do not execute
the compiled Prism+ implementation.
PAL and NTSC hardware checks remain required for gameplay, display/audio,
return-to-menu behavior, firmware self-update, and ordinary/large cartridges.
