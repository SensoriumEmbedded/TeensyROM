# Teensy Firmware

C/C++ firmware for the Teensy 4.1, built via Arduino IDE + Teensyduino. See [Overview.md](Overview.md) for how this relates to the C64-side code, and [Constraints.md](Constraints.md) before touching anything on the hot path.

## Directory map

```
Source/Teensy/
├── Teensy.ino, TeensyROM.h      main firmware entry point + config/budgets
├── BusSnoop.ino, DMAControl.ino ISR-adjacent bus/DMA helpers
├── D64.ino, DriveDirLoad.ino    disk-image / directory-listing loading
├── FileParsers.ino              CRT/PRG parsing into RAM_Image/CrtChips[]
├── FileTransfer.ino             host<->device file transfer support
├── FlashUpdate.ino              self-flashing firmware updater (SD/USB .hex)
├── IOHandlers.ino               IO-handler init/dispatch glue
├── MainMenuItems.h              table of built-in menu ROM/PRG includes
├── RemoteControl.ino            Teensy-initiated IRQ command protocol into C64
├── SerUSBIO.ino                 USB-serial command console/protocol (~1000 lines)
├── ServiceTCP.ino                Ethernet TCP -> same command parser as SerUSBIO
├── MeatloafComm.ino             serial protocol to a "Meatloaf" IEC companion device
├── nfcScan.ino                  NFC tag scanning (PN532)
├── midiDevName.c                MIDI device naming
├── src/PN532/                   vendored PN532 NFC reader library
├── Flash/                       Teensyduino in-app flash-update helper (FlashTxx, FXUtil)
├── MinimalBoot/                 separate minimal-boot sketch, see below
├── TRMenuFiles/                 ROMs/ (embedded C64 .h byte arrays), Pics/, SIDs/, Text_PETSCII/
└── tools/                       PowerShell build scripts, arduino-cli, dual-boot hex linker
```

## Entry point / boot flow

`Teensy.ino` is the main sketch (Arduino compiles the file matching the folder name first). `setup()` (~line 66): overclocks to 816 MHz, configures cartridge-port GPIO direction/buffers, asserts C64 reset, attaches interrupts — `isrExtResetDetect`, `isrButton`, and critically **`isrPHI2` on the PHI2 pin rising edge** (~line 129, the per-bus-cycle hot path, see [Constraints.md](Constraints.md)) — starts the USB host stack, restores EEPROM settings, builds the main menu (`SetUpMainMenuROM()`), and handles auto-launch. `loop()` (~line 220) polls `BtnPressed` and dispatches to `IOHandler[CurrentIOHandler]->PollingHndlr()`.

`MinimalBoot/MinimalBoot.ino` has a near-identical `setup()`/`loop()` shape (see below).

## Key subsystems

| Subsystem | Where |
|---|---|
| ROM/CRT/PRG parsing | `FileParsers.ino`, `D64.ino`, `DriveDirLoad.ino` |
| Cartridge/IO emulation | `MinimalBoot/Common/IO_Handlers/*.c` (below) + dispatch in `IOHandlers.ino` |
| Menu system (what the C64 sees) | `MainMenuItems.h` + `TRMenuFiles/ROMs/TeensyROMC64.h` — the latter is **compiled 6502 machine code** (`TeensyROMC64_bin[]`), not firmware source; produced by the C64-side build, see [C64-Software.md](C64-Software.md) |
| External comms (companion PC/mobile apps) | `SerUSBIO.ino` (USB serial, token protocol), `ServiceTCP.ino` (Ethernet, same parser) — see [docs/ControlComms.md](/docs/ControlComms.md) |
| Comms into the C64 | `RemoteControl.ino` (`DoC64IRQ()`, IRQ-based command handshake), on-cartridge register protocol — see [Comms-Protocol.md](Comms-Protocol.md) |
| IEC companion device | `MeatloafComm.ino` — chunked transfer with CRC32 ack |
| SD card | Arduino `<SD.h>`, used throughout `DriveDirLoad.ino`/`FileParsers.ino`/`IOH_REU.c` |
| PSRAM | Optional backing store selectable in `IOH_REU.c` (`USE_PSRAM` / `USE_RAM12` / `USE_SD`), mapped at fixed address `0x70000000` (`pPSRAM`) |
| RTC | `Teensy3Clock.get()/.set()` (Teensy 4.1 built-in RTC via TimeLib), in `IOH_TeensyROM.c` / `IOH_TR_BASIC.c` |
| NFC | `nfcScan.ino` + `src/PN532/`, over `USBHostSerial` |
| MIDI | `IOH_MIDI.c`, `midiDevName.c`, handler-swap in `IOHandlers.ino` (`SetMIDIHandlersNULL`) |

## The IO_Handlers pattern

Each cartridge/IO type is one `.c` file under `MinimalBoot/Common/IO_Handlers/` (e.g. `IOH_REU.c`, `IOH_EasyFlash.c`, `IOH_GMod2.c`, `IOH_ActionReplay.c`, `IOH_MIDI.c` — roughly 25 total). Each defines a `stcIOHandlers` struct instance with a name plus a function-pointer table: `InitHndlr`, `IO1Hndlr`, `IO2Hndlr`, `ROMLHndlr`, `ROMHHndlr`, `PollingHndlr`, `CycleHndlr` (struct in `MinimalBoot/Common/IOHandlers.h:60-70`). All handler files are `#include`d into `IOHandlers.h` and registered in one dispatch array, `IOHandler[]` (`IOHandlers.h:105-147`), indexed by `enum enumIOHandlers`. `CurrentIOHandler` selects the active entry; `IOHandlerInit()` (`IOHandlers.ino:36`) swaps handlers. The array compiles conditionally per feature flag (`Fab04_REU`, `Fab04_Freezers`, `Fab04_KernalReplace`, `MinimumBuild`).

`isrPHI2` (the per-cycle ISR, see [Constraints.md](Constraints.md)) reads the decoded chip-select lines each bus cycle and dispatches straight into the *active* handler's `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` — this is the actual emulation mechanism, so a new cartridge/IO type is added by writing a new handler file, not by touching the ISR itself.

## MinimalBoot vs full firmware

`MinimalBoot/` is a separate, self-contained Arduino sketch (`MinimalBoot.ino`) sharing most logic with the main firmware via `MinimalBoot/Common/` (guarded by `#ifdef MinimumBuild`), including `ISRs.c`, IO handler headers, and `Fab04FeatureCtl.h` feature gating — a comment in `Common_Defs.h:2` warns "re-compile both minimal and full if anything changes here!"

**Purpose:** this is a dual-firmware system specifically for large-CRT support. Full firmware handles CRT files up to ~650KB; MinimalBoot strips out USB host support to free RAM, extending the ceiling to ~850KB (files in that range must load from SD, not USB) — see [docs/ControlComms.md](/docs/ControlComms.md) and [docs/CRT_Implementation.md](/docs/CRT_Implementation.md) for the user-facing framing.

**The switch trigger is dynamic exhaustion, not a size check.** `ParseChipHeader()` (`FileParsers.ino:97-` ) tries RAM1 first, then falls back to `malloc()` in RAM2 for each CRT chip/bank (`FileParsers.ino:120-128`). Only when that `malloc()` fails does it look for a MinimalBoot image already present in flash at a higher address (magic-number/vector-table sanity checks, `FileParsers.ino:135-157`) and reboot into it by writing `EEPROM.write(eepAdMinBootInd, MinBootInd_ExecuteMin)` followed by `REBOOT` (`FileParsers.ino:165-168`). On the next boot, `Teensy.ino`'s `switch (EEPROM.read(eepAdMinBootInd))` (`Teensy.ino:189`) sees `MinBootInd_ExecuteMin` and boots straight into `MinimalBoot.ino`, which auto-launches the pending CRT (`eepAdCrtBootName`) rather than showing the menu (`MinimalBoot.ino:135-146`). If the user later launches something else from within minimal mode, `Min_SerUSBIO.ino:274` writes `MinBootInd_LaunchFull` so the *next* reboot returns to full firmware and launches it there instead (`Teensy.ino:206-209`). The ~650KB figure quoted elsewhere is the empirical result of this exhaustion point, not a hardcoded threshold in source.

A host app can query which mode is currently active via `FWCheckToken`. Only a restricted command set (`ResetC64Token`, `LaunchFileToken`, `VersionInfoToken`, `FWCheckToken`) is available while in minimal mode. Dual-boot hex combining (so both images ship in one `.hex`) is handled by `tools/Build-DualBoot.ps1` — see [Build-System.md](Build-System.md).

**Why MinimalBoot includes Ethernet** despite the RAM cost of an otherwise memory-scarce build: it's specifically so a host app can remote-launch a command over the network while a large CRT is running in minimal mode, interrupting it and taking back system control — not because minimal-mode logic itself needs network access.

## File associations: full vs. MinimalBoot vs. shared

Confirmed directly from each sketch's `#include` list, not inferred:

- **Full-firmware-only** (siblings of `Teensy.ino`, no minimal counterpart): `BusSnoop.ino`, `D64.ino`, `DMAControl.ino`, `DriveDirLoad.ino`, `FileParsers.ino`, `FileTransfer.ino`, `FlashUpdate.ino`, `IOHandlers.ino`, `MeatloafComm.ino`, `RemoteControl.ino`, `SerUSBIO.ino`, `ServiceTCP.ino`, `midiDevName.c`, `nfcScan.ino`, plus `TeensyROM.h` and `MainMenuItems.h` (the embedded interactive-menu ROM table — `MinimalBoot.ino` does not include this; it has no interactive menu). `Flash/` and `src/PN532/` are likewise full-only.
- **MinimalBoot-only** (siblings of `MinimalBoot.ino`): `Min_DriveDirLoad.ino`, `Min_RunTeensyApp.ino`, `Min_SerUSBIO.ino`, `Min_ServiceTCP.ino`, `Min_TeensyROM.h`, `Min_core_cm7.h`. **The `Min_` prefix means "parallel, independently-written counterpart," not shared code** — despite the name similarity to a same-named full-FW file, each is its own implementation. `MinimalBoot.ino` doesn't include `USBHost_t36.h`, `Bounce.h`, or `MainMenuItems.h` — those subsystems are genuinely absent from this build, not just unused.
- **Truly shared, single source, compiled into both** (confirmed by both sketches' `#include` lists pointing at the same files, and a marker file literally named `___These files used by both main and minimal builds___` sitting in the directory): everything under `MinimalBoot/Common/` — `Common_Defs.h`, `DriveDirLoad.h`, `Fab04FeatureCtl.h`, `IOHandlers.h`, `ISRs.c`, `Menu_Regs.h`, and all `IO_Handlers/*.c` files. There's no `Min_IOHandlers.ino` because none is needed — `MinimalBoot.ino` calls straight into the shared `Common/IOHandlers.h` dispatch table itself.

## Memory budgets (see [Constraints.md](Constraints.md) for the full list)

- `MaxRAM_ImageSize = 128` KB in full build (`TeensyROM.h:26`)
- MinimalBoot: `(392 - 8*Num8kSwapBuffers - EthernetDeduction)` KB (`Min_TeensyROM.h:58`)

<br>

[Back to Architecture Overview](Overview.md)
