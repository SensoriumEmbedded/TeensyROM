# Reboot-Time Screen Blanking

`REBOOT` (`Common_Defs.h:492`) is a bare Cortex-M `SCB_AIRCR` software reset — a full MCU reset, GPIO/IOMUX included. The C64 keeps running on its own clock through the ~ms the Teensy takes to come back up, so a still-running C64 mid-fetch from cartridge space at that instant can see floating/indeterminate bus data. That's the source of "screen glitches on reboot."

## `RebootTR()`

`RebootTR()` (`Common_Defs.h:497`, `do { SetResetAssert; REBOOT; } while(0)`) asserts the C64's own `/RESET` line before rebooting the Teensy, and is used at every real reboot site in the firmware — never call bare `REBOOT` directly. `do/while(0)`-wrapped because several call sites are unbraced (e.g. `if (x) REBOOT;`).

Caveat: for the one case that's actually been hardware-tested against a reported glitch (below), this half turned out not to be load-bearing. It's kept anyway — harmless, and consistent across all call sites — but don't assume it's doing real work everywhere it's used just because it's present.

## The large-CRT-to-MinimalBoot case

`FileParsers.ino:168` (RAM2 exhausted mid-load of a large `.crt`, switching to `MinimalBoot` to reload and auto-launch it) had a reported, reproducible glitch that `RebootTR()` alone did not fix.

**Root cause, confirmed on hardware:** the glitch appears *after* the reboot completes, while `MinimalBoot` is reloading the same CRT — not during the reboot transition itself. Symptom was random/varying garbage (not a consistent frozen frame), ruling out a simple deterministic stale-state explanation.

**What was tried and didn't help** (don't re-try these here without new evidence):
- A 250ms delay between `SetResetAssert` and `REBOOT` — if timing/settling were the issue, this margin (250,000+ PHI2 cycles) would have been far more than enough. No change.
- Detaching the PHI2 bus-service interrupt before rebooting, on the theory it might be caught mid-transaction — no change. (Also: `PerformDMA()`'s state machine is itself advanced by this same interrupt, so if you do use a DMA write here, it must happen *before* any PHI2 detach, or the write hangs forever.)
- Detaching `isrExtResetDetect` on `Fab04_BiDirReset` boards — `BiDir_Reset_PIN == CORE_PIN6`, the same pin `SetResetAssert` drives low, so `RebootTR()` self-triggers this interrupt on that hardware variant. `MinimalBoot.ino:78` has a pre-existing comment noting this same interrupt "leaves it in an odd state" in another context — a real, independently-documented quirk — but detaching it here didn't fix this glitch either.

**Actual fix** (`FileParsers.ino:168-177`, TR+ only): DMA-write `$D011 = 0x00` (VIC-II `DEN=0`, blank) via `PerformDMA()` immediately before `RebootTR()`, guarded by `#ifdef Fab04_FullDMACapable`. This works because `$D011` is C64-side VIC-II hardware state — untouched by the Teensy's own MCU reset, it stays blanked straight through the reboot and the entire MinimalBoot reload, until something on the C64 side writes it again. A fixed `0x00` is used rather than a read-modify-write: `DEN=0` stops all VIC-II byte fetches regardless of whatever mode the interrupted CRT/game had set, and nothing needs that mode preserved across this transition.

**How it turns back on:** nothing TeensyROM-side re-enables it — confirmed by repo search, no Teensy code writes `$D011` anywhere else. For a normal (non-Ultimax) cartridge, the reset vector still resolves through KERNAL ROM, and KERNAL's own reset init sets `$D011` back to default *before* it even checks for the cartridge autostart signature — the display comes back regardless of what the cartridge's own code does. For an Ultimax-mode cartridge (`GAME=0`/`EXROM=1`), `ROMH` covers the vectors too, so the cartridge's own code supplies the reset vector directly and KERNAL never runs — no safety net. **Known, untested, non-blocking edge case:** an Ultimax-mode cartridge whose own startup code never sets `$D011` would stay blanked. If this is ever hit in practice, the fix would need to detect Ultimax mode from the CRT header and skip the blank for that case.

<br>

[Back to Architecture Overview](Overview.md)
