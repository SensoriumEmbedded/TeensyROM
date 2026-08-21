# Hard Constraints

Rules that are easy to violate without realizing it because nothing enforces them at compile time. Check this file before proposing firmware-level design changes.

## The per-C64-cycle ISR is off-limits for new logic

`isrPHI2()` in `Source/Teensy/MinimalBoot/Common/ISRs.c` (`FASTRUN void isrPHI2()`) fires on every C64 bus cycle — a budget of roughly **985ns (PAL) / 1023ns (NTSC)** (`PALBusFreq`/`NTSCBusFreq`, `Common_Defs.h:205-206`). It's `#include`d into both `Teensy.ino` and `MinimalBoot.ino` and attached to the PHI2 pin rising edge in each `setup()`.

Inside the budget: decode the address/R-W bus off GPIO, dispatch into the active IO handler's `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` (see [Teensy-Firmware.md](Teensy-Firmware.md#the-io_handlers-pattern)), optionally emulate VIC "bad line" behavior, manage DMA state transitions. Hardcoded nanosecond-precision timing constants (`Def_nS_MaxAdj=1030`, `Def_nS_RWnReady=135`, `Def_nS_PLAprop=150`, `Def_nS_DMASetupPAL/NTSC=440/430` — `Common_Defs.h:319-342`) are tuned against real hardware and specific board revisions (comments cite e.g. "Reloaded MKII"); nearby comments document what breaks if a value shifts.

**Do not propose adding new logic to this function — not even a single cheap branch or null-check — for signaling, handshakes, or status flags.** This has come up concretely: a design discussion about signaling PRG-load completion from Teensy to C64 considered mirroring the existing `fBusSnoop`/`fKernRepl` pre-dispatch hook pattern with a new dedicated flag check, and was rejected specifically because it touched the ISR. The mechanism that shipped instead reuses the existing `fBusSnoop` hook rather than adding a new one — see below.

If a design genuinely seems to require touching the ISR, look for a solution entirely outside the hot path first (main-loop-only mechanism, existing register semantics, timing-based approach). If none exists, surface that explicitly as a tradeoff rather than defaulting to an ISR change.

New cartridge/IO types are added by writing a new `IO_Handlers/*.c` file and registering it in the `IOHandler[]` dispatch array — not by editing the ISR.

## `fBusSnoop` is a single contested global slot — never assign it directly if anything else might own it

`fBusSnoop` (`Common_Defs.h:164`) is checked unconditionally, ahead of everything else, at the top of `isrPHI2()` — whatever it points to gets first refusal on every single bus cycle, regardless of `CurrentIOHandler`. That priority is exactly why it's useful for anything that needs to intercept the bus independent of the active handler, and exactly why blindly assigning it (`fBusSnoop = X;`) is dangerous the moment more than one thing might want it at once: REU (`IOH_REU.c`) and Kernal Replace (`IOH_KernalReplace.c`) both use it, `BusSnoop.ino`'s `BusAnalysis()` diagnostic borrows it temporarily, and the PRG-load handshake below claims it for the duration of a handler swap. Two failure shapes to watch for:

- **Stomp on install**: something assigns `fBusSnoop = X` while another owner still needs it live, silently disabling that owner with no error.
- **Stomp on cleanup**: something unconditionally resets `fBusSnoop = NULL` (or reinstalls its own value) when it's done, without checking whether a *different* owner has since taken over — wiping out state that has nothing to do with the code doing the resetting. `BusAnalysis()` hit exactly this: its original `fBusSnoop = NULL;` cleanup, if it ran while a PRG-load handshake had taken over the slot mid-sample, would silently hang the C64's poll loop forever. Fixed by checking ownership both at entry (`if (fBusSnoop!=NULL || PendingfBusSnoop!=NULL) { ...abort... }`) and at cleanup (`if (fBusSnoop == &BusCount) fBusSnoop = NULL;`) — never assume you're still the owner just because you were a moment ago.

**The convention now used for anything that wants a bus-snoop hook installed from `InitHndlr` (i.e. from inside `IOHandlerInit()`, `Source/Teensy/IOHandlers.ino:37`): stage into `PendingfBusSnoop` (`Common_Defs.h:167`) instead of writing `fBusSnoop` directly**, and let `IOHandlerInit()` decide when it's safe to promote — either immediately (no handshake in progress) or deferred until the handshake's own completion-read (see below). `InitHndlr_REU()` and the handler-swap-time `InitHndlr_KernalReplace()` (not to be confused with `InitHndlr_KERNALReplace_PreStart()`, see next section) both follow this. Code that runs *outside* `IOHandlerInit()`'s call path and provably can't race the handshake (see the PreStart exception below) may still assign `fBusSnoop` directly — but that safety has to be argued for explicitly, not assumed.

### Worked example: the PRG-load IO-handler-swap handshake

Historical context: this used to be a fixed C64-side delay loop (`ldy #$06 / ldx #$00 / dex / bne ...`) empirically tuned to outlast the Teensy's handler swap after a PRG finishes loading. It was both wasteful (always paid the worst-case delay) and unsound (an REU pre-load with no SD/USB media present could exceed even that tuned worst case, since a media timeout has no fixed upper bound a constant delay can cover).

The mechanism that replaced it, entirely without touching the ISR:

- **Register**: `rRegIOHSwapPoll` (`Menu_Regs.h`, high/otherwise-unused IO1 offset `0xFE`) with values `rihsBusy`/`rihsReady`.
- **Arm** (`IOH_TeensyROM.c`, the `rCtlRunningPRG` case that already runs when the C64 signals "PRG loaded, pick a handler"): resets `HandshakeReady = false`, `PendingfBusSnoop = NULL`, and installs `fBusSnoop = &HandshakeSnoop` — a one-time assignment triggered by this specific write, not new per-cycle ISR logic.
- **Watch** (`HandshakeSnoop()`, `IOH_TeensyROM.c`): answers reads of `rRegIOHSwapPoll` with `rihsBusy`/`rihsReady` depending on `HandshakeReady`, ahead of whatever `CurrentIOHandler` actually is — this is what lets the C64 poll a stable answer across a swap that changes which handler is even active.
- **Complete**: `IOHandlerInit()` runs the target handler's `InitHndlr()` (which may stage `PendingfBusSnoop`), reassigns `CurrentIOHandler`, then either sets `HandshakeReady = true` (if `fBusSnoop` is still `&HandshakeSnoop` — handshake in progress) or promotes `PendingfBusSnoop` immediately (if not — e.g. a cart loaded directly from the menu, no handshake involved at all).
- **Hand off**: `HandshakeSnoop`, on the read where it reports `rihsReady`, also does `fBusSnoop = PendingfBusSnoop` in that same call — atomic with the read the C64 is about to act on, so the real handler's snoop (or none) is already live before the PRG starts running.
- **C64 side**: `Source/C64/MainMenuCRT/source/PRGLoadStartReloc.s` polls `rRegIOHSwapPoll` immediately after sending `rCtlRunningPRG` — no priming delay needed, since arming happens synchronously in the same ISR call as that write.

**Deliberate exception — `InitHndlr_KERNALReplace_PreStart()`** (`IOH_KernalReplace.c`): Kernal Replace needs its hook live *before* BASIC even initializes, which is earlier than any PRG-load handshake could possibly be armed — so this one function is correctly allowed to assign `fBusSnoop = &KernalCheck` directly, unlike every other `InitHndlr`. It's a separate function from the struct's actual `InitHndlr` (`InitHndlr_KernalReplace()`, which *does* go through `PendingfBusSnoop`, since it runs later via the normal swap path and needs to re-stage the hook after the handshake temporarily takes the slot). Conflating the two — same underlying "load the kernal binary and hook the check" logic, called from two different timing contexts — was a real mistake made once already during this design; the split and the `_PreStart` naming exist specifically to keep that from happening again.

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

**RAM1's 512KB is not two independently-fixed 512KB regions.** Verified against the actual `imxrt1062_t41.ld` linker script (Teensyduino 1.61.0): it's a single 16-bank × 32KB FlexRAM pool, dynamically split between ITCM (code) and DTCM (variables + stack) at link time, based on compiled code size rounded *up* to the next 32KB bank boundary (`_itcm_block_count`). `padding` is that rounding waste — not usable as stack headroom directly, but it *is* usable runway for new RAM1-resident code (new IO handlers, features) to grow into before the next bank boundary gets crossed. `free for local variables` is the real, usable stack headroom, and it's the number that determines crash risk — but `padding` is the number that determines how much longer new code can be added before `free for local variables` takes its next hit. This matters for planning: because future ISR-reachable code (which can never move to `FLASHMEM`) will eventually cross another 32KB boundary and remove a full 32K from `free for local variables` in one discontinuous jump, the working target shouldn't be "stay above the reliability floor" — it should be "stay above the floor **plus** enough margin to absorb that next jump," i.e. **~56KB+**, not the ~24KB floor itself.

**The ~24KB reliability floor itself is independently confirmed twice in your own crash-testing history, in both firmware images:**
- `Source/Teensy/TeensyROM.h:26-30` (full firmware): `// Test case: Random(?) NFC tag with large directory, crash when tapped / 20000 free got further, but still crashes. Less always crashes / *Need >24000 RAM1 free for local` — this is also what motivated shrinking `MaxRAM_ImageSize` from 144KB to 128KB (20,000 → 36,476 bytes free). A local build on 2026-08-12 measured 40,444 bytes free for the current Fab04 config — roughly 4KB more headroom than that 9/25/2025 note, gained from unrelated changes since.
- `Source/Teensy/MinimalBoot/Min_TeensyROM.h:48-52` (MinimalBoot, with Ethernet): same `>24000` threshold, independently arrived at.

**Local build capability exists to verify this going forward** (see [[teensyrom_project_facts]] memory / ask before assuming it's still set up if picking this up much later): `Source/Teensy/tools/Build-DualBoot.ps1 -Fab04_Features -SkipMinimalBuild -SkipCombine`, run from `Source/Teensy/tools/`, builds the Fab04 main firmware in about a minute and prints this exact report — use it to check real before/after impact of any `FLASHMEM` migration rather than estimating from line counts.

### Full-audit-list FLASHMEM experiment (2026-08-12) — applied, measured, then reverted

Every candidate in the [Known-Issues.md FLASHMEM audit](Known-Issues.md#flashmem-audit-running-list) was applied at once (~115 functions across 27 files, both firmwares) to see where it landed. Both firmwares built clean.

```
Baseline: RAM1: variables:254468, code:220104, padding:9272   free for local variables:40444
After:    RAM1: variables:254468, code:202744, padding:26632  free for local variables:40444
```

**The goal here wasn't stack headroom (`free for local variables`) — it was clearing `padding` runway for future RAM1-resident code growth**, e.g. new IO handlers. `code` dropped 17,360 bytes, all of which landed in `padding` (9,272 → 26,632) since the drop didn't cross a 32KB ITCM bank boundary. By that measure this was a clear success: 17KB is a lot of runway — for scale, the RetroReplay IOH added around the same time took about 800 bytes out of this same pool, so this batch is worth roughly 20+ IOH-sized additions before `padding` runs out again and a new feature starts eating into the crash-risk floor instead.

It was reverted anyway, not because it failed, but because keeping ~115 functions in `FLASHMEM` continuously pays a small per-call flash-access-time cost on every one of them, including some not-especially-cold paths (`Swift_Browser.c`'s HTML/URL parsing, `IOH_MIDI.c`'s per-note callbacks) — not worth paying yet when the ~9KB baseline still has a while to run at ~800 bytes/feature. The per-file data below is kept so a future pass can move just enough, deliberately, rather than repeating the full 27-file batch.

**Separately, also worth keeping in mind:** at 26,632/32,768 of padding, only **~6.1KB more** would have been needed to cross the boundary and jump `free for local variables` by a full 32KB in one step — a bigger, different win than the padding-runway one above, available whenever stack headroom itself (not just code-growth runway) is actually needed. Per-file impact, measured from the built ELF (`arm-none-eabi-nm --print-size`, not estimated from line counts) for files that compile into the Fab04 full firmware (Teensy.ino) — the build with the tight constraint:

| File | Bytes moved | Functions |
|---|---|---|
| `DriveDirLoad.ino` | ~3970 | 10 |
| `Swift_Browser.c` | ~3040 | 16 |
| `IOH_ASID.c` | ~2420 | 9 |
| `Teensy.ino` | ~1640 | 6 |
| `IOH_Swiftlink.c` | ~1480 | 3 |
| `IOH_MIDI.c` | ~1450 | 14 |
| `nfcScan.ino` | ~1070 | 4 |
| `RemoteControl.ino` | ~870 | 4 |
| `Swift_RxQueue.c` | ~850 | 11 |
| remaining 15 files (single `InitHndlr_*`/small helpers) | ~950 combined | ~20 |

`DriveDirLoad.ino` + `IOH_ASID.c` alone (~6.4KB) would clear the ~6.1KB gap — no need to repeat the full 27-file batch to get the next boundary jump; those two are both large, already call-graph-verified, and genuinely cold (menu navigation, MIDI/SID setup) rather than latency-sensitive.

(MinimalBoot-only files, not relevant to the Fab04 constraint above since that build already has 57KB+ headroom: `Min_DriveDirLoad.ino` ~5150 bytes/11 functions — `LoadFile()` alone is ~3890 bytes, the single largest function found in either firmware — plus `MinimalBoot.ino` ~1040 bytes/6 functions and `Min_ServiceTCP.ino` ~46 bytes.)

## Toolchain pin: avoid Teensyduino 1.62.0

Confirmed root cause is the GCC 15.2.1 toolchain bump (from 11.3.1) in Teensyduino 1.62.0, **not** TeensyROM source code — causes intermittent SD-read stalls with 2 PSRAM chips installed. Build against **1.61.0**. Do not attempt to work around this by modifying source; it's an upstream toolchain regression.

## `Common_Defs.h` is shared between full and MinimalBoot builds

Explicit warning in the file itself (`Common_Defs.h:2`): "re-compile both minimal and full if anything changes here" — a change here silently desyncs the two firmware images if only one is rebuilt and flashed.

## Cartridge register layout must be kept in sync by hand

`Source/C64/MainMenuCRT/source/Menu_Regs.i` and the Teensy-side `Menu_Regs.h` define the same register map independently — there is no shared source of truth or build-time check. See [Comms-Protocol.md](Comms-Protocol.md) and [Known-Issues.md](Known-Issues.md) for a designed-but-not-yet-implemented fix.

<br>

[Back to Architecture Overview](Overview.md)
