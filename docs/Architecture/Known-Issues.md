# Known Issues / Deferred Work

Concrete, scoped findings surfaced during architecture walkthroughs — real issues with a known cause and (usually) a designed fix, deliberately queued rather than acted on immediately. Distinct from [Constraints.md](Constraints.md), which documents permanent rules; this file is a to-do list and should shrink as items get resolved (move resolved items out rather than leaving them marked done).

## MinimalBoot's Ethernet-disable protection has a gap during VIC-cycle emulation

**Where:** `Source/Teensy/MinimalBoot/Min_DriveDirLoad.ino:53-55`

The full-firmware path disables `IRQ_ENET`/`IRQ_PIT` while `EmulateVicCycles` is active, because servicing the VIC half of the cycle extends `isrPHI2` enough that a stray Ethernet interrupt can cause a missed cycle (see [Constraints.md](Constraints.md)). MinimalBoot has the identical `EmulateVicCycles = true;` line (`Min_DriveDirLoad.ino:55`) but its matching `NVIC_DISABLE_IRQ(IRQ_ENET/IRQ_PIT)` lines just above are commented out. This protection gap is now live, not just theoretical, since MinimalBoot runs Ethernet (added deliberately for remote-launch/interrupt support, see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware)).

**Why it hasn't caused a visible problem:** carts needing VIC-half emulation are old/small titles (e.g. Jupiter Lander, Radar Rat Race, Clowns) that always fit within full-firmware's RAM budget — the combination "needs VIC emulation" + "too large for full FW, must run from MinimalBoot" doesn't occur among real carts. This is an accidental non-collision, not a designed-safe state.

**Proposed fix:** comment out `EmulateVicCycles = true;` at `Min_DriveDirLoad.ino:55` too, so MinimalBoot never attempts VIC-cycle emulation at all — trading "a hypothetical oversized cart needing VIC emulation wouldn't run correctly from MinimalBoot" for closing the unprotected-Ethernet window.

**Status:** deferred — not ready to test yet (2026-08-11).

## Cartridge register map (`Menu_Regs.i` / `Menu_Regs.h`) is hand-duplicated across two languages

**Where:** `Source/C64/MainMenuCRT/source/Menu_Regs.i` (ACME) and the Teensy-side `Menu_Regs.h` (C) — see [Comms-Protocol.md](Comms-Protocol.md).

Two independently hand-maintained files define the same register offset/enum map, with only a comment ("These need to match Teensy Code: Menu_Regs.h") enforcing consistency — no shared source, no build-time check.

**Designed fix (not yet implemented):**
1. Keep `Menu_Regs.h` as the single hand-edited source of truth, written in ordinary C (`#define NAME VALUE`) — no invented dual-purpose macro syntax.
2. Regenerate `Menu_Regs.i` from it via a standard C-preprocessor pass (`gcc -E -x assembler-with-cpp` or plain `cpp`) — a well-established pattern for sharing C headers with assembly (used by e.g. GNU `as`, many NES/SNES homebrew toolchains), not a bespoke generator script.
3. `Menu_Regs.i` keeps its current location and format, so none of its **6 consumers** need to change: `MainMenuCRT` (`TeensyROMC64.asm`, `MainMenu.asm`), `SettingsMenu`, `TRHelpScreens`, `TRExtPortCheck`, `MIDI2SID`, `ExpansionPortTest` (see [C64-Software.md](C64-Software.md)) all keep their existing `!src "../MainMenuCRT/source/Menu_Regs.i"` line unchanged.
4. Regeneration step goes in **`Source/C64/SetToolPaths.bat`**, not `BuildAllC64.bat` — confirmed via grep that all 12 sub-project build scripts source `SetToolPaths.bat` first, unconditionally, so this is the only chokepoint that's guaranteed to run whether a developer invokes `BuildAllC64.bat` or a single sub-project's `build*.bat` directly. Putting it in `BuildAllC64.bat` alone would leave `Menu_Regs.i` stale for anyone building a single sub-project standalone.

**Status:** deferred — plan agreed, not yet implemented (2026-08-11).

## Large-CRT bank-swap DMA reliability claim may be stale

**Where:** [CRT_Implementation.md](/docs/CRT_Implementation.md), [Constraints.md](Constraints.md#memory-budgets-are-hard-caps-not-soft-targets)

The >850KB bank-swap mechanism (REU-style DMA-line-assert pause, not true bus-mastering DMA — TR+'s bus-mastering doesn't help here since the pause just needs to be perceptually instant, which bus-mastering doesn't improve) is documented as unreliable on most C128s and a low percentage of NTSC systems. That claim may no longer hold and is worth re-testing.

**Status:** flagged for re-test, not yet re-verified (2026-08-11).

## `ASIDPlayer.asm` has its `Menu_Regs.i` include commented out

**Where:** `Source/C64/ASIDPlayer/source/ASIDPlayer.asm:7`

Every other `Menu_Regs.i` consumer has an active `!src` line; ASIDPlayer's is present but commented out. Not yet confirmed whether this is intentional (ASID genuinely doesn't need those registers) or a leftover from an earlier split.

**Status:** unverified, low priority.

<br>

[Back to Architecture Overview](Overview.md)
