# Build System

Two independent toolchains, run in a fixed order. Canonical instructions (prefer these over this doc for exact commands/paths): [Source/BuildInfo.md](/Source/BuildInfo.md) and [Source/C64/README.md](/Source/C64/README.md) for the C64 side; for the Teensy side, `npm run build:tr` / `npm run build:tr-plus` (see [Dual-boot linking](#dual-boot-linking-toolsbuild-firmwaremjs) below).

## Build order (matters)

1. **C64 side first** — `npm run build:c64` (`tools/build-c64.mjs`, optionally `--project <name>`) assembles all 6502 sources and writes the generated headers into `Source/Teensy/TRMenuFiles/ROMs/`.
2. **Teensy firmware second** — Arduino IDE / arduino-cli build, which embeds those headers as compiled-in byte arrays.

Skipping step 1 after a C64-side change means the Teensy build silently uses stale menu/settings/utility code. The firmware build does not check, but CI does for every project but `TRCustomBasicCommands`, which KickAssembler builds and CI installs no JRE for: `tools/build-c64.test.mjs` reassembles the rest and byte-compares the result against the committed headers.

## C64 side

- Toolchain: ACME cross-assembler 0.97 (all sub-projects except `TRCustomBasicCommands`, which uses KickAssembler + Java JRE 1.8)
- `tools/lib/bin2header.mjs` (a Node port of bin2header) converts each `.prg`/`.bin` to a C header; `node tools/bin2header.mjs` does the same for a single file
- What gets built is the manifest `tools/c64-projects.json`; tools are found via `ACME` / `KICKASS_JAR` / `JAVA_HOME`, then `PATH`, then a pinned, checksummed download into `tools/.cache/` (ACME on Windows/macOS, KickAssembler everywhere; Java is never downloaded). Nothing is edited per machine.
- See [C64-Software.md](C64-Software.md) for the sub-project list

## Teensy side

- Arduino IDE 2.x + Teensyduino, board "Teensy 4.1", Optimize "Faster", CPU Speed "600 MHz", USB Type "Serial + MIDI" — useful for interactive single-image dev/debug builds and direct IDE upload.
- **Known-bad toolchain version: Teensyduino 1.62.0** — its GCC 15.2.1 bump (from 11.3.1) causes intermittent SD-read stalls with 2 PSRAM chips installed. Current pinned/recommended version is **1.61.0** (as of FW 0.8, 2026-08-02). Root cause confirmed to be the toolchain, not TeensyROM source — do not "fix" this by changing source code.
- Alternative: generate a `.hex` and flash via SD/USB drive instead of direct IDE upload (needed since the Teensy USB power trace is severed during assembly, and TR must be C64-powered to program directly)

## Dual-boot linking (`tools/build-firmware.mjs`)

The canonical way to produce a shippable, combined `TeensyROM(+)_<ver>_full.hex` — containing the full firmware and the MinimalBoot image -- and, for `--target tr-plus`, the extension host image at `0x60280000` as well (see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware) for why MinimalBoot exists) — is the zero-dependency Node builder at `tools/build-firmware.mjs`, run via `npm run build:tr` (plain TeensyROM) or `npm run build:tr-plus` (TeensyROM+). This is what `.github/workflows/build.yml` runs on every push and tag. It downloads a version-pinned, SHA256-checked `arduino-cli` if not already on `PATH`, builds MinimalBoot, builds the main image, builds the extension host image for `--target tr-plus`, and combines them into one hex. Does **not** flash automatically — use Teensy Loader, or flash via SD/USB drive, afterward.

The program in the extension slot is not fixed. `--host-sketch <dir>` overlays that directory's files onto the MinimalBoot sketch in place of `Source/Teensy/VMBoot` — which is how the stock host is built too, so a third-party host is the same build with a different top-level `.ino`. `Source/Teensy/ExampleHost` is a complete one (`npm run build:example-host`), and [Extension-Hosts.md](Extension-Hosts.md) covers what a host owes the loader. The flag is refused where there is no extension slot to build into (`--target tr`, `--no-extensions`, `--skip-extension-build`) rather than ignored: a flag that silently did nothing there would ship the stock host under the caller's own name. A `--host-sketch` build lands under `TeensyROM+_<ver>_<sketch-dir>_full.hex`, not the shipping name, for the same reason `--no-extensions` lands under `_noext`: the slot holds a program this repo did not write.

Guardrail worth knowing about: before building a plain TR image, the builder checks `Fab04FeatureCtl.h` for an active `#define Fab04_Features` and, by default, throws rather than continue — this exists specifically to prevent accidentally building a TR+ image mislabeled as plain TR. Comment out the define yourself, build `--target tr-plus` instead, or pass `--yes` to have the builder comment it out and continue (no interactive y/N prompt, unlike the old PowerShell script — this has to be decided up front on the command line).

Every image the builder compiles gets `-DFNET_CFG_TLS=0` appended to `build.flags.defs` — MinimalBoot and the main image, plus the extension host image on `--target tr-plus`. FNET defaults that to 2 (`fnet_user_config.h` in the installed Teensy core, behind an `#ifndef`), which links mbedTLS — SSL state machine, X.509 parser, bignum, test certificates — into MinimalBoot and the main image, costing roughly 210 K of flash in each. The flag compiles out NativeEthernet's TLS API along with it, server side included (`EthernetServer(port, tls)`, `begin(port, tls)`, `setSRVCert`/`setSRVKey`), so restoring TLS means dropping the flag rather than changing a call. After linking, the builder re-reads the image with `nm` and fails the build if an mbedTLS symbol came back. The flag lives in the builder, so an Arduino IDE build of the same tree still links mbedTLS and comes out correspondingly larger.

`--ccache` (macOS/Linux, with `ccache` on `PATH`) sends compiles through ccache. Add it after `--` when going through npm, e.g. `npm run build:tr -- --ccache`. CI builds TR and TR+ as parallel jobs with this on, and turns it off for `Release_v*` tags so released hex files always come from a clean compile. With it on, the build works in a fixed `run-ccache-<target>` directory that each run clears, instead of a new `run-XXXX` one. The directory path is part of ccache's cache key, so a new name every run would never hit the cache.

Pinned versions: arduino-cli (`tools/lib/toolchain.mjs`), the Teensy core (the workflows, `tools/build-firmware.mjs`, and the Teensyduino version stated in `Source/BuildInfo.md`, this doc and `CONTRIBUTING.md`) and the CRC32 library (the workflows and `Source/BuildInfo.md`). `tools/check-pins.mjs` lists every one of these places and fails if they disagree, so a bump has to update them all. Dependabot can't see these pins, so `.github/workflows/check-pins.yml` runs that check weekly and also compares the pins with the latest releases. It keeps an issue open while there are updates, or fails the run when the repo has Issues turned off. A release you've decided against, like Teensy core 1.62.0, goes in that dependency's `rejected` list with the reason, so it stops being reported.

<br>

[Back to Architecture Overview](Overview.md)
