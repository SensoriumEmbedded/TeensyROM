# Hard Constraints

Rules that are easy to violate without realizing it because nothing enforces them at compile time. Check this file before proposing firmware-level design changes.

## The per-C64-cycle ISR is off-limits for new logic

`isrPHI2()` in `Source/Teensy/MinimalBoot/Common/ISRs.c` (`FASTRUN void isrPHI2()`) fires on every C64 bus cycle — a budget of roughly **985ns (PAL) / 1023ns (NTSC)** (`PALBusFreq`/`NTSCBusFreq`, `Common_Defs.h:205-206`). It's `#include`d into both `Teensy.ino` and `MinimalBoot.ino` and attached to the PHI2 pin rising edge in each `setup()`.

Inside the budget: decode the address/R-W bus off GPIO, dispatch into the active IO handler's `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` (see [Teensy-Firmware.md](Teensy-Firmware.md#the-io_handlers-pattern)), optionally emulate VIC "bad line" behavior, manage DMA state transitions. Hardcoded nanosecond-precision timing constants (`Def_nS_MaxAdj=1030`, `Def_nS_RWnReady=135`, `Def_nS_PLAprop=150`, `Def_nS_DMASetupPAL/NTSC=440/430` — `Common_Defs.h:319-342`) are tuned against real hardware and specific board revisions (comments cite e.g. "Reloaded MKII"); nearby comments document what breaks if a value shifts.

**Do not propose adding new logic to this function — not even a single cheap branch or null-check — for signaling, handshakes, or status flags.** This has come up concretely: a design discussion about signaling PRG-load completion from Teensy to C64 considered mirroring the existing `fBusSnoop`/`fKernRepl` pre-dispatch hook pattern with a new dedicated flag check, and was rejected specifically because it touched the ISR. Existing hooks like `fBusSnoop` are also not safe to repurpose for unrelated one-off signaling — they're contested global slots already owned by specific handlers (`IOH_REU.c`'s `InitHndlr_REU()` unconditionally nulls it and REU dynamically re-installs it at runtime for its FF00-write trigger; `IOH_KernalReplace.c` also uses it).

If a design genuinely seems to require touching the ISR, look for a solution entirely outside the hot path first (main-loop-only mechanism, existing register semantics, timing-based approach). If none exists, surface that explicitly as a tradeoff rather than defaulting to an ISR change.

New cartridge/IO types are added by writing a new `IO_Handlers/*.c` file and registering it in the `IOHandler[]` dispatch array — not by editing the ISR.

## Memory budgets are hard caps, not soft targets

- Full firmware: `MaxRAM_ImageSize = 128` KB (`Source/Teensy/TeensyROM.h:26`) — the RAM1 image buffer. Beyond that, CRT banks spill into RAM2 via `malloc()` (`FileParsers.ino:120-128`); when that allocation fails, firmware reboots into MinimalBoot (`FileParsers.ino:165-168`, see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware) for the full trigger mechanism) — this exhaustion point empirically lands around **~650KB** total, it is not a hardcoded threshold.
- MinimalBoot: `(392 - 8*Num8kSwapBuffers - EthernetDeduction)` KB (`Source/Teensy/MinimalBoot/Common/Min_TeensyROM.h:58`) — extends CRT support to **~850KB**, at the cost of dropping USB host support (SD-only loading in this mode).
- Files beyond ~850KB use bank-swapping from SD (only), asserting the DMA line for ~3ms per uncached swap — not true bus-mastering DMA, just the old-school REU-style pause assertion. This mechanism is unreliable on most C128s and a low percentage of NTSC systems; a DMA Pause check utility (`TODCheck`/Test+Diags BASIC tool) exists to test a given system before relying on this.

When editing anything that changes per-file/per-struct RAM footprint in either firmware image, check these budgets — MinimalBoot in particular has very little headroom (its whole reason for existing is memory scarcity).

## Toolchain pin: avoid Teensyduino 1.62.0

Confirmed root cause is the GCC 15.2.1 toolchain bump (from 11.3.1) in Teensyduino 1.62.0, **not** TeensyROM source code — causes intermittent SD-read stalls with 2 PSRAM chips installed. Build against **1.61.0**. Do not attempt to work around this by modifying source; it's an upstream toolchain regression.

## `Common_Defs.h` is shared between full and MinimalBoot builds

Explicit warning in the file itself (`Common_Defs.h:2`): "re-compile both minimal and full if anything changes here" — a change here silently desyncs the two firmware images if only one is rebuilt and flashed.

## Cartridge register layout must be kept in sync by hand

`Source/C64/MainMenuCRT/source/Menu_Regs.i` and the Teensy-side `Menu_Regs.h` define the same register map independently — there is no shared source of truth or build-time check. See [Comms-Protocol.md](Comms-Protocol.md).

<br>

[Back to Architecture Overview](Overview.md)
