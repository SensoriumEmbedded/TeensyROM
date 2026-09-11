# DMA Timing — Outstanding Issues

Tracking list for the `DMA_Timing` branch, started after merging PR #21 (NTSC DMA
write-hold fix + `TestDMAPattern()`). Covers what's still open in the DMA path,
not just what PR #21 fixed.

This is about TR+'s true bus-mastering DMA (`DMAControl.ino`'s `PerformDMA()`/
`DMAByte()` — real REU, Kernal Replace, freezer carts, remote
`WriteC64Mem`/`ReadC64Mem`), **not** the DMA-line-assert-only pause used by the
>850KB CRT bank-swap mechanism (see [Known-Issues.md](Known-Issues.md)'s
"Large-CRT bank-swap DMA reliability" entry) — that one never does an actual
bus-mastered data transfer, so nothing here bears on it.

## Open bugs

### 1. Autolaunch never applies PAL/NTSC-specific DMA constants `[Development]`
`wRegVid_TOD_Clks` (video standard detection) is only written by the C64-side
main menu ROM (`MainMenu.asm` → `Start:`). CRT/REU images launched via SD or TR
autolaunch (`Teensy.ino` → `RemoteLaunch` → `DoCartDirect`) skip the menu
entirely, so an NTSC machine runs the whole transfer on PAL's `nS_DMADataHold`
default (430) — which sits inside NTSC's own measured 430–450ns partial-byte
error band. Reachable via the REU handler and `WriteC64Mem`/`ReadC64Mem` before
the menu has ever loaded once. Does **not** affect the menu-driven DMA Pause
Check, which necessarily runs after `Start:` has already set the constants.
— *Common_Defs.h `nS_MaxAdj` comment, PR #21*

### 2. PAL branch of the timing fix is unverified on real PAL hardware `[Validation]`
The sweep that produced `Def_nS_DMADataHoldNTSC=410` was run on a C128; PAL's
430 default is inherited from the old shared constant, not independently
measured. An FPGA C64 can regression-test that the PAL/NTSC switch logic fires
correctly, but not the analog margin (its buffers/bus loading are its own).

### 3. NTSC-vs-PAL and C64-vs-C128 are confounded in the PR #21 data `[Investigation]`
The only "NTSC" characterization rig was a flat C128; the only "PAL" rig was a
C64 (+Kawari, cross-checked against Ultimate). No NTSC-C64 or PAL-C128 data
point exists. Can't yet tell whether the 410/430 cliff difference is a
video-standard cycle-length effect (the stated theory) or a C64/C128 hardware
effect (the original question this branch started from) — or both.

### 4. Marginal partial-byte failure mode is intermittent and uncharacterized `[Investigation]`
The 430–450ns partial-byte error band went quiescent partway through the PR #21
test session and could not be re-confirmed in a follow-up interleaved A/B
(10 rounds, 410 vs 430, zero errors either arm — while 460 still reproduced as
a positive control). Unknown what gates it: thermal, uptime, VIC/screen state.
No margin number here is fully trustworthy until this is understood, and no A/B
against it is repeatable yet.

### 5. Cycle-overrun mode has no graceful degradation `[Development]`
Past ~455ns hold, the DMA write wait doesn't return before Phi2 falls, and the
transfer collapses completely (deterministic, whole-byte corruption) rather
than degrading. The non-DMA `DataPortWriteWait()` already caps its wait at the
Phi2 falling edge; the DMA path doesn't. Needs a scope trace to confirm the
cycle budget before capping it — a robustness guard, not a fix for the marginal
mode above.

### 6. Original C128 PHI2-generation-delay theory still untested against hardware `[Validation]`
Earlier research (RAD project postmortem) suggested the C64 has more delay
between its internal CPU clock and the port-visible PHI2 than the C128 does —
a distinct, unmeasured hypothesis from the DMA-hold marginality PR #21 found.
No scope capture has confirmed or ruled this out independently.

### 7. `TestDMAPage()` (uniform-fill diagnostic) has a detectability blind spot `[Development]`
A write that never lands is invisible to a uniform-fill verify if the page
already held that value. This means historical "passed DMA check" results —
including whatever data underlies the "most C128s fail" line in
`General_Usage.md` — were only ever screened with an instrument that can't see
this failure class on the passing side either. `TestDMAPattern()` (added in
PR #21) is the first diagnostic actually sensitive to it.

### 8. `TestDMAPattern()` reports only to serial, not to the C64/C128 screen `[Development — implemented, unverified]`
All of its pass/fail detail (bad-byte counts, XOR mask, unchanged count,
per-bit direction) goes through `Serial.printf()` only, same as the rest of
`ExpPortDMA()`'s DMA bit transition tests. Without a PC serial terminal
attached, the only thing visible on the C64/C128 itself is the overall "OK" /
"Failed, details on serial" line. Items #2, #3, #4, and #6 above all depend on
running these sweeps on various hardware in the field, which is a lot more
accessible if the actual numbers show up on screen instead of requiring a
serial capture.

Added a `ToScreen` parameter: the detailed bit-direction table stays
serial-only (too wide for 40 columns), but a one-line summary per pattern
(`$ValA/$ValB: bad/total OK|Failed`) now goes through `SendMsgPrintfln()` when
set. `ExpPortDMA()`'s menu-driven self-test passes `true`; the `'z'` serial
sweep command passes `false`, unchanged.

First on-hardware run (9/11/26) caught a real bug in the first pass at this:
a pre-fill-only failure (`BadBytes=0`, `PrefillBad=1`) showed "Failed" on
screen followed by `xor $00 unch 0` — misleading, since that line was always
the *pattern* write's `XorMask`/`Unchanged`, both legitimately zero when the
failure is entirely in the pre-fill phase. Fixed to mirror serial's structure:
`pre-fill $XX bad N` prints when `PrefillBad` is nonzero, `xor $XX unch N`
prints when `BadBytes` is nonzero — independently, not both gated on the same
`!Clean`. `Unchanged` stays included because it's the actual discriminator
between the two failure modes above — 0 means marginal/partial-byte (#4), high
means cycle-overrun/whole-byte (#5). `WorstPass` still stays serial-only.

### 9. Convert remaining `WaitUntil_nS()` call sites to `WaitUntil_nS_fine()`, methodically `[Development]`
`WaitUntil_nS_fine()` (`Common_Defs.h`) fixes the same per-pass-reconversion
overshoot as the DMA data-hold fix above, but only `DataPortWaitReadDMA()` and
`DataPortWriteWaitDMA()` have been switched over so far. Nine live call sites
still use the coarser `WaitUntil_nS()`:

- `DMAControl.ino:70` — BA-transition wait in `DMAByte()`
- `DMAControl.ino:77` — `nS_DMASetup` (address/R-W setup before Phi2 rising)
- `ISRs.c:53,91,107,195,207` — `nS_DMAAssert` (×2), `nS_RWnReady`, `nS_PLAprop`,
  `nS_VICStart` — all inside the main `isrPHI2()` cycle handler and its
  DMA-assert path
- `Common_Defs.h:435,457` — `nS_VICDHold`, `nS_DataSetup` (the non-DMA
  read/write helpers)
- `IOH_REU.c:178` — `nS_DMAAssert`

"Methodically" because each conversion is a real timing change, not a pure
refactor — same lesson as this branch's own hoist: shrinking the overshoot
moves every one of these waits earlier by some amount, and several
(`nS_RWnReady`, `nS_PLAprop`, `nS_DMAAssert`) already carry board-specific
tuning history (C128/C64C-specific bumps, Reloaded Mk2 special builds) in
their surrounding comments. Converting all nine in one pass risks re-opening
several already-settled margins at once with no way to tell which one
regressed. Convert and re-verify one call site — or one tightly related group,
like the three VIC-cycle waits inside `isrPHI2()` — at a time.

`nS_DMAAssert` specifically is worth prioritizing over the others in this
list: it's the *only* one of the nine that's shared with the CRT bank-swap
pause mechanism (confirmed via `IOH_MagicDesk2.c`, which drives the same
`DMA_State` machine and `SetDMAAssert`/`nS_DMAAssert` sequence used to enter
`DMA_S_ActiveReady`, then never touches `nS_DMADataSetup`/`nS_DMADataHold` at
all — those only run inside `DMAByte()`'s per-cycle transfer loop). If
`nS_DMAAssert` turns out marginal on C128 the same way `nS_DMADataHold` was
on NTSC, that would explain the bank-swap mechanism's separately-documented
C128 unreliability (see [Known-Issues.md](Known-Issues.md)'s "Large-CRT
bank-swap DMA reliability" entry) through one shared root cause — unconfirmed,
but the most direct link between the two mechanisms found so far.

## Housekeeping (low risk, found along the way)

### 10. `ExpPortDMA()` leaves IRQs disabled on early-return failure paths `[Development]`
Disables `IRQ_ENET`/`IRQ_PIT` for the duration of the self-test but doesn't
re-enable them if it bails out early on a failure — leaves the system degraded
until reboot.

### 11. `tools/BootLinkerFiles/bootdata.c.orig` is stale `[Development]`
Differs from the Teensyduino version (`1.61.0`) pinned in `BuildInfo.md` in the
FlexSPI configuration block; the build script leaves the older file behind in
the developer's core.

### 12. `nSToCyc(N)` doesn't parenthesize its argument `[Development]`
```
#define nSToCyc(N)  (N*(F_CPU_ACTUAL>>16)/(1000000000UL>>16))
```
so `nSToCyc(nS_DMASetup-90)` in `IOH_REU.c` expands to `nS_DMASetup -
54` = 386 cycles (643 nS), not the intended 210 cycles (350 nS). The PSRAM
slow-read guard arms far later than the comment implies. The Teensy core's own
version of this expression does parenthesize.

Worth noting why it survived: the slope is identical in both expansions
(−0.6 cycles/nS), so the 75 / 80 / 85 sweep recorded in that comment behaved
sensibly and converged — it was locally correct, just offset by a constant
293 nS. Which also means fixing the macro invalidates that calibration and
needs a re-tune on hardware, so it was left alone.

<br>

[Back to Architecture Overview](Overview.md)
