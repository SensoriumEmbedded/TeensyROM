# Hard Constraints

Rules that are easy to violate without realizing it because nothing enforces them at compile time. Check this file before proposing firmware-level design changes.

## The per-C64-cycle ISR is off-limits for new logic

`isrPHI2()` in `Source/Teensy/MinimalBoot/Common/ISRs.c` (`FASTRUN void isrPHI2()`) fires on every C64 bus cycle — a budget of roughly **985ns (PAL) / 1023ns (NTSC)** (`PALBusFreq`/`NTSCBusFreq`, `Common_Defs.h:205-206`). It's `#include`d into both `Teensy.ino` and `MinimalBoot.ino` and attached to the PHI2 pin rising edge in each `setup()`.

Inside the budget: decode the address/R-W bus off GPIO, dispatch into the active IO handler's `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` (see [Teensy-Firmware.md](Teensy-Firmware.md#the-io_handlers-pattern)), optionally emulate VIC "bad line" behavior, manage DMA state transitions. Hardcoded nanosecond-precision timing constants (`Def_nS_MaxAdj=1030`, `Def_nS_RWnReady=135`, `Def_nS_PLAprop=150`, `Def_nS_DMASetupPAL/NTSC=440/430` — `Common_Defs.h:319-342`) are tuned against real hardware and specific board revisions (comments cite e.g. "Reloaded MKII"); nearby comments document what breaks if a value shifts.

**Do not propose adding new logic to this function — not even a single cheap branch or null-check — for signaling, handshakes, or status flags.** This has come up concretely: a design discussion about signaling PRG-load completion from Teensy to C64 considered mirroring the existing `fBusSnoop`/`fKernRepl` pre-dispatch hook pattern with a new dedicated flag check, and was rejected specifically because it touched the ISR. Existing hooks like `fBusSnoop` are also not safe to repurpose for unrelated one-off signaling — they're contested global slots already owned by specific handlers (`IOH_REU.c`'s `InitHndlr_REU()` unconditionally nulls it and REU dynamically re-installs it at runtime for its FF00-write trigger; `IOH_KernalReplace.c` also uses it).

If a design genuinely seems to require touching the ISR, look for a solution entirely outside the hot path first (main-loop-only mechanism, existing register semantics, timing-based approach). If none exists, surface that explicitly as a tradeoff rather than defaulting to an ISR change.

New cartridge/IO types are added by writing a new `IO_Handlers/*.c` file and registering it in the `IOHandler[]` dispatch array — not by editing the ISR.

### The real dividing line: flash-backed vs. RAM-backed, not just "the ISR function itself"

The no-new-logic rule extends to everything reachable from the ISR's call path, not just `isrPHI2()` literally: `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` of the *active* IO handler are all called every cycle and must stay just as disciplined. Within that call path, only cheap, deterministic operations are safe — register reads/writes, small variable updates, anything backed by RAM1/RAM2. Nothing backed by flash is safe, and that's a wider category than it first looks:

- **`FLASHMEM`-attributed code** — flash execution goes through the FlexSPI cache; a cache miss blows the cycle budget. `FLASHMEM` is fine for `InitHndlr`/`PollingHndlr`/main-loop code (never called from the ISR path), never for `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr`.
- **EEPROM access — including reads.** The Teensy's "EEPROM" isn't a separate low-latency peripheral; it's emulated on top of the same flash `FLASHMEM` code lives in, so it carries identical unbounded latency. Not just writes — reads too.
- **SD card and USB access** — same unbounded-latency problem, different peripheral.

So new cartridge/IO type authors: none of the per-cycle-called handler functions may touch flash in any form (code or EEPROM), SD, or USB. If a handler needs any of that, it belongs in `InitHndlr` (setup-time) or `PollingHndlr` (main-loop, see the wait mechanism below), never in the cycle-called functions.

**Separate exception, different reason: `Source/Teensy/FlashUpdate.ino`'s `DoFlashUpdate()` (the vendored FlasherX firmware-update code) and `Flash/FlashTxx.c` are deliberately *not* `FLASHMEM` candidates, despite being main-context-only.** This isn't about ISR reachability — it's a self-modifying-flash hazard: this code erases and rewrites the Teensy's own flash while running, so if it lived in flash itself, the CPU could be fetching its own next instruction from the exact region currently being erased/reprogrammed. Don't flag this file's core update path in a FLASHMEM audit for the reason above; it has its own, unrelated reason to stay RAM-resident.

### The sanctioned way to run slow work: cooperative polling, not a CPU halt

When a handler genuinely needs to do something slow — flash-backed work, or anything else that can't fit the cycle budget — the pattern used throughout the C64 menu code is a cooperative polling handoff, **not** a bus/CPU halt:

1. C64 side writes a `rCtl*WAIT`-style command to `wRegControl`, then calls `WaitForTRDots` / `WaitForTRWaitMsg` (`WaitForTRMain` loop, `Source/C64/MainMenuCRT/source/MainMenu.asm:942-982`). This is ordinary running 6502 code — it prints one dot per second off the CIA TOD clock as a progress indicator and polls `rwRegStatus`, waiting for `rsReady` (or displays an interim `rsC64Message`).
2. On the Teensy side, the ISR-path handler (e.g. `IO1Hndlr_TeensyROM`) only does the fast, RAM-safe part: record that a status/command was written. The actual work happens in `PollingHndlr_TeensyROM()` (`Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c:1878`), called from the **main `loop()`**, not the ISR — `Teensy.ino`'s `loop()` dispatches to `IOHandler[CurrentIOHandler]->PollingHndlr()`. It sees `rwRegStatus != rsReady`, dispatches to the real handler via `StatusFunction[...]` (free to use FLASHMEM/EEPROM/SD/USB here), then writes `rsReady` back when done.

The C64 keeps running its own bus cycles the entire time (feeding the ISR normally) — it's just spinning harmlessly on a status register while the Teensy works outside the ISR. This is a **different** mechanism from the DMA-line-assert pause used for large-CRT bank swapping (see below) — that one actually halts the C64 CPU; this one doesn't.

### The PHI2 ISR runs at elevated interrupt priority, even over Ethernet

`Teensy.ino:130` — `NVIC_SET_PRIORITY(IRQ_GPIO6789, 16); //set HW ints as high priority, otherwise ethernet int timer causes misses`. (Note: this "NVIC" is the ARM core's Nested Vectored Interrupt Controller — unrelated to the C64's VIC video chip; the acronym collision is coincidental.)

Beyond priority, Ethernet/PIT interrupts are fully **disabled** (not just deprioritized) during specific windows where `isrPHI2` can't tolerate any preemption at all:

- **VIC-cycle emulation** — `DriveDirLoad.ino:242-243` disables `IRQ_ENET`/`IRQ_PIT` when `EmulateVicCycles` is set, re-enabled at `Teensy.ino:352-353`. Servicing the VIC half of the cycle extends `isrPHI2`'s per-cycle work enough that a stray Ethernet IRQ could blow the timing budget for the *next* PHI2 cycle. See [Known-Issues.md](Known-Issues.md) for a related gap in MinimalBoot's version of this.
- **Firmware self-flash** — `Flash/FXUtil.cpp:175-176` disables both, and goes further by fully `detachInterrupt()`-ing the button and PHI2 pins entirely just above (lines 173-174) — there's no valid handler to service PHI2 while live firmware is being overwritten.
- **ASID (MIDI SID) playback** — `IOH_ASID.c:579-580` disables both during ASID streaming, which has its own tight audio-rate timing requirement.

## Memory budgets are hard caps, not soft targets

- Full firmware: `MaxRAM_ImageSize = 128` KB (`Source/Teensy/TeensyROM.h:26`) — the RAM1 image buffer. Beyond that, CRT banks spill into RAM2 via `malloc()` (`FileParsers.ino:120-128`); when that allocation fails, firmware reboots into MinimalBoot (`FileParsers.ino:165-168`, see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware) for the full trigger mechanism) — this exhaustion point empirically lands around **~650KB** total, it is not a hardcoded threshold.
- MinimalBoot: `(392 - 8*Num8kSwapBuffers - EthernetDeduction)` KB (`Source/Teensy/MinimalBoot/Common/Min_TeensyROM.h:58`) — extends CRT support to **~850KB**, at the cost of dropping USB host support (SD-only loading in this mode).
- Files beyond ~850KB use bank-swapping from SD (only), asserting the DMA line for ~3ms per uncached swap — not true bus-mastering DMA, just the old-school REU-style pause assertion. TR+'s true bus-mastering DMA doesn't help here: the pause just needs to be perceptually instant to the 6510 for an SD→RAM bank transfer, which bus-mastering doesn't improve. This mechanism was documented as unreliable on most C128s and a low percentage of NTSC systems; **that reliability claim may now be stale and is due for a re-test** (flagged during architecture review, 2026-08-11 — see [Known-Issues.md](Known-Issues.md)). A DMA Pause check utility (`TODCheck`/Test+Diags BASIC tool) exists to test a given system before relying on this.

When editing anything that changes per-file/per-struct RAM footprint in either firmware image, check these budgets — MinimalBoot's RAM scarcity is well-known and obvious, but **RAM1 pressure in the full Fab04 (TR+) build is actually the tighter, less-visible constraint**: as of 2026-08-12, the Fab04 main build (the tightest full-firmware config, since it compiles in REU/KernalReplace/Freezers on top of everything else) has only **~9-10KB of RAM1 "padding"** left before a hard link failure:
```
FLASH: code:398356, data:1518744, headers:8496   free for files:6200868
 RAM1: variables:254468, code:220104, padding:9272   free for local variables:40444
 RAM2: variables:17600  free for malloc/new:506688
```
`RAM_Image` (the fixed 128KB CRT buffer) is RAM1's biggest single consumer and won't shrink, so moving eligible code to `FLASHMEM` (see the running audit in [Known-Issues.md](Known-Issues.md)) is the main lever available to keep this build compiling as more features get added — not just an optimization.

**RAM1's 512KB is not two independently-fixed 512KB regions.** Verified against the actual `imxrt1062_t41.ld` linker script (Teensyduino 1.61.0): it's a single 16-bank × 32KB FlexRAM pool, dynamically split between ITCM (code) and DTCM (variables + stack) at link time, based on compiled code size rounded *up* to the next 32KB bank boundary (`_itcm_block_count`). `padding` is exactly that rounding waste — not usable, already consumed. `free for local variables` is the real, usable stack headroom, and it's the number that determines crash risk. This matters for planning: because future ISR-reachable code (which can never move to `FLASHMEM`) will eventually cross another 32KB boundary and remove a full 32K from `free for local variables` in one discontinuous jump, the working target shouldn't be "stay above the reliability floor" — it should be "stay above the floor **plus** enough margin to absorb that next jump," i.e. **~56KB+**, not the ~24KB floor itself.

**The ~24KB reliability floor itself is independently confirmed twice in your own crash-testing history, in both firmware images:**
- `Source/Teensy/TeensyROM.h:26-30` (full firmware): `// Test case: Random(?) NFC tag with large directory, crash when tapped / 20000 free got further, but still crashes. Less always crashes / *Need >24000 RAM1 free for local` — this is also what motivated shrinking `MaxRAM_ImageSize` from 144KB to 128KB (20,000 → 36,476 bytes free). A local build on 2026-08-12 measured 40,444 bytes free for the current Fab04 config — roughly 4KB more headroom than that 9/25/2025 note, gained from unrelated changes since.
- `Source/Teensy/MinimalBoot/Min_TeensyROM.h:48-52` (MinimalBoot, with Ethernet): same `>24000` threshold, independently arrived at.

**Local build capability exists to verify this going forward** (see [[teensyrom_project_facts]] memory / ask before assuming it's still set up if picking this up much later): `Source/Teensy/tools/Build-DualBoot.ps1 -Fab04_Features -SkipMinimalBuild -SkipCombine`, run from `Source/Teensy/tools/`, builds the Fab04 main firmware in about a minute and prints this exact report — use it to check real before/after impact of any `FLASHMEM` migration rather than estimating from line counts.

## Toolchain pin: avoid Teensyduino 1.62.0

Confirmed root cause is the GCC 15.2.1 toolchain bump (from 11.3.1) in Teensyduino 1.62.0, **not** TeensyROM source code — causes intermittent SD-read stalls with 2 PSRAM chips installed. Build against **1.61.0**. Do not attempt to work around this by modifying source; it's an upstream toolchain regression.

## `Common_Defs.h` is shared between full and MinimalBoot builds

Explicit warning in the file itself (`Common_Defs.h:2`): "re-compile both minimal and full if anything changes here" — a change here silently desyncs the two firmware images if only one is rebuilt and flashed.

## Cartridge register layout must be kept in sync by hand

`Source/C64/MainMenuCRT/source/Menu_Regs.i` and the Teensy-side `Menu_Regs.h` define the same register map independently — there is no shared source of truth or build-time check. See [Comms-Protocol.md](Comms-Protocol.md) and [Known-Issues.md](Known-Issues.md) for a designed-but-not-yet-implemented fix.

<br>

[Back to Architecture Overview](Overview.md)
