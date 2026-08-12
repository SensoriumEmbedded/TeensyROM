# TeensyROM Architecture Overview

Reference documentation for AI assistants and contributors ramping up on this codebase. Dense and structural by design — see linked docs for user-facing feature explanations, and [Constraints.md](Constraints.md) for hard rules not to violate.

TeensyROM is a Teensy 4.1-based multi-function cartridge for the Commodore 64/128: ROM (CRT) emulator, instant PRG loader, MIDI/SID interface, Ethernet/BBS bridge, NFC launcher, and remote-control target. Two hardware variants share one firmware codebase: **TR** (original, PCB v0.3, DMA-line-assert only) and **TR+** (PCB v0.4, adds true bus-mastering DMA enabling Kernal Replacement, a real 512KB REU, freezer cartridge emulation, and remote DMA memory access). TR+ is gated in firmware by `Fab04_Features` in `Source/Teensy/MinimalBoot/Common/Fab04FeatureCtl.h`.

## Repo layout

| Path | Contents |
|---|---|
| `Source/Teensy/` | Teensy microcontroller firmware — C/C++, Arduino/Teensyduino build. See [Teensy-Firmware.md](Teensy-Firmware.md). |
| `Source/C64/` | Programs that run ON the C64 (menu, settings, utilities) — 6502 assembly. See [C64-Software.md](C64-Software.md). |
| `Source/BuildInfo.md` | Canonical build instructions/tool versions for both sides. |
| `PCB/` | Eagle PCB (`.sch`/`.brd`) schematic/PCB source, per hardware version (`v0.3` public/self-buildable under `v0.3/EaglePCB/`, `v0.4`/TR+ not yet public). |
| `docs/` | User-facing feature docs (usage guides, protocol reference) — flat, no subfolders except this one. |
| `docs/Architecture/` | This doc set — structural/AI-reference material, not user-facing. |
| `bin/TeensyROM/` | Released firmware binaries + `FW_Release_History.md`. |
| `3D_Print_Case/`, `media/` | Enclosure files and images, not code. |

## The two toolchains, and how they connect

TeensyROM is built in two independent passes that feed into each other in one direction only:

1. **C64 side builds first.** 6502 assembly sources under `Source/C64/*/source/` are cross-assembled (ACME, or KickAssembler for `TRCustomBasicCommands`) into raw binaries, then converted by `bin2header.py` into C headers (`static const unsigned char ..._prg[]`) and copied into `Source/Teensy/TRMenuFiles/ROMs/`.
2. **Teensy firmware builds second**, embedding those generated headers directly as byte arrays — the on-screen menu, settings pages, and bundled utility programs are compiled-in C64 machine code, not generated at runtime.

This means a change to any C64-side `.asm` requires rebuilding that sub-project (or running `BuildAllC64.bat`) *before* rebuilding the Teensy firmware, or the change won't be picked up. Full details: [Build-System.md](Build-System.md).

C64 code and Teensy firmware also talk to each other **at runtime** two different ways — a low-level memory-mapped cartridge register protocol, and (separately) an external host-facing USB/Ethernet protocol. See [Comms-Protocol.md](Comms-Protocol.md).

## Doc set

- [Teensy-Firmware.md](Teensy-Firmware.md) — module map, entry point, IO_Handlers pattern, MinimalBoot
- [C64-Software.md](C64-Software.md) — MainMenuCRT, SettingsMenu, sub-programs, build pipeline
- [Comms-Protocol.md](Comms-Protocol.md) — cartridge register protocol + link to external host protocol
- [Build-System.md](Build-System.md) — toolchains, versions, dual-boot linking, known gotchas
- [Constraints.md](Constraints.md) — hard rules: ISR hot path, memory budgets, toolchain pins
- [Known-Issues.md](Known-Issues.md) — scoped, deferred findings from architecture walkthroughs, with designed (not yet implemented) fixes

<br>

[Back to main ReadMe](/README.md)
