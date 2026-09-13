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

Grouped by what's needed next: **Development** (a code change), **Investigation**
(no clear fix direction yet, needs more data), **Validation** (fix direction
known, needs hardware to confirm), then **Closed**.

## Development

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

Confirmed to hit C128 too, not just NTSC C64 — see #14: an autolaunched C128
never reaches its own `te445` set either, same mechanism.

### 5. Cycle-overrun mode has no graceful degradation `[Development]`
Past ~455ns hold, the DMA write wait doesn't return before Phi2 falls, and the
transfer collapses completely (deterministic, whole-byte corruption) rather
than degrading. The non-DMA `DataPortWriteWait()` already caps its wait at the
Phi2 falling edge; the DMA path doesn't. Needs a scope trace to confirm the
cycle budget before capping it — a robustness guard, not a fix for the marginal
mode above.

Mirroring `DataPortWriteWait()`'s cap verbatim isn't safe here — the removed
Phi2 check in `DataPortWriteWaitDMA()` was taken out on purpose (comment:
"not checking Phi2 state due to tight timing and early in cycle call can
cause early exit"); a per-iteration `GP6_Phi2` read this early/tight could
cut the hold *short* on a false read, trading one corruption mode for another.

**Next step:** scope it. Only one call site (`DMAControl.ino:93`, inside
`DMAByte()`'s write branch), so no per-site ambiguity. Data bus alone isn't
enough to read on a scope — persistence-mode capture just shows a noisy band
(every transaction overlaid, DMA and non-DMA indistinguishable). Bracket the
wait itself with the existing debug signal instead:
```c
SetDebugAssert;
WaitUntil_nS_fine(nS_DMADataHold);
SetDebugDeassert;
```
in `DataPortWriteWaitDMA()` ([DMAControl.ino:28](../../Source/Teensy/DMAControl.ino)) — pin 52 on this TR+ build
(`Fab04_DebugSignals`). Phi2 on one channel, that pulse on another: the gap
between the pulse's falling edge and Phi2's next falling edge is the real
margin, and tells you how early in the wait it'd be safe to start polling
Phi2 without risking the early-exit failure the original check was pulled
for. Deferred — needs the scope, not done yet.

### 9. Convert remaining `WaitUntil_nS()` call sites to `WaitUntil_nS_fine()`, methodically `[Development]`
`WaitUntil_nS_fine()` (`Common_Defs.h`) fixes the same per-pass-reconversion
overshoot as the DMA data-hold fix above, but only `DataPortWaitReadDMA()` and
`DataPortWriteWaitDMA()` have been switched over so far. Eight live call sites
still use the coarser `WaitUntil_nS()`:

- `DMAControl.ino:77` — `nS_DMASetup` (address/R-W setup before Phi2 rising)
- `ISRs.c:53,91,107,195,207` — `nS_DMAAssert` (×2), `nS_RWnReady`, `nS_PLAprop`,
  `nS_VICStart` — all inside the main `isrPHI2()` cycle handler and its
  DMA-assert path
- `Common_Defs.h:435,457` — `nS_VICDHold`, `nS_DataSetup` (the non-DMA
  read/write helpers)
- `IOH_REU.c:178` — `nS_DMAAssert`

(`DMAControl.ino:70`'s BA-transition wait is no longer on this list — the
`C128-DMA-Timing` import hoisted it to the same manual pattern as the
already-fixed data-setup/hold waits, and gave it a name, `nS_DMABAWait`. Not
using the `WaitUntil_nS_fine()` macro itself, so still worth a small follow-up
for consistency, but the actual overshoot bug is already gone.)

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

## Investigation

### 3. NTSC-vs-PAL and C64-vs-C128 are confounded in the PR #21 data `[Investigation]`
The only "NTSC" characterization rig was a flat C128; the only "PAL" rig was a
C64 (+Kawari, cross-checked against Ultimate). No NTSC-C64 or PAL-C128 data
point exists. Can't yet tell whether the 410/430 cliff difference is a
video-standard cycle-length effect (the stated theory) or a C64/C128 hardware
effect (the original question this branch started from) — or both.

**Update (2026-09-12):** the missing NTSC-C64 point now exists, via the `TR
Validation Tracker` hardware log (fw `0.8.0.6t`, Expansion Port Diagnostics,
looped). Two separate NTSC C64 units passed clean — 757 loops (#14) and 461
loops (#002b "Rat64") — while the one C128 in the collection, also NTSC, same
firmware constants, failed repeatedly on the DMA test. Same video standard,
same constants, one hardware type passes and the other doesn't — that's
direct evidence for the C64-vs-C128 hardware-effect side of this confound,
not the video-standard theory (which would predict the NTSC-C64 units
struggling too). Still not fully closed: no PAL-C128 unit exists in the
collection, so the video-standard effect isn't ruled out entirely, just
outweighed by the one remaining cell still being empty rather than
contradicting.

**Second, independent line of evidence (2026-09-13, see #14):** on a
different constant entirely (`te`/`nS_DMASetup`, not `nS_DMADataHold`), a
real NTSC C128 measurably needs a later assert point (445) than the shared
NTSC default (430) — 13–25× fewer errors at the C128-specific value. Same
direction as the tracker data, different mechanism/constant, independently
pointing at C64-vs-C128 as a real, active variable rather than noise.

### 4. Marginal partial-byte failure mode is intermittent and uncharacterized `[Investigation]`
The 430–450ns partial-byte error band went quiescent partway through the PR #21
test session and could not be re-confirmed in a follow-up interleaved A/B
(10 rounds, 410 vs 430, zero errors either arm — while 460 still reproduced as
a positive control). Unknown what gates it: thermal, uptime, VIC/screen state.
No margin number here is fully trustworthy until this is understood, and no A/B
against it is repeatable yet.

### 6. Original C128 PHI2-generation-delay theory still untested against hardware `[Investigation]`
Earlier research (RAD project postmortem) suggested the C64 has more delay
between its internal CPU clock and the port-visible PHI2 than the C128 does —
a distinct, unmeasured hypothesis from the DMA-hold marginality PR #21 found.
No scope capture has confirmed or ruled this out independently.

Cross-checked (2026-09-11) against Commodore's own manuals/schematics — see
[C64-C128-DMA-Port-Reference.md](C64-C128-DMA-Port-Reference.md). The C64 and
C128 PRGs publish the *identical* `/DMA` assert/de-assert rule (only while Φ2
is low) with no separate timing spec for either machine, so this specific
PHI2-generation-delay theory is neither corroborated nor ruled out by any
official spec — it'd have to be a real, undocumented difference in the two
VIC chips' PHI2 output stage. What the official docs do confirm is a related
but distinct fact: C128 routes `/DMA` through the MMU (GAEC gating, Z80
`/BUSRQST`, TA/SA bus-direction reversal) rather than straight to the CPU like
C64 does — one extra, verified-real logic stage between the port pin and a
settled bus that could plausibly cost margin without appearing in any
published number. Still no scope capture on either theory.

Reclassified from Validation to Investigation (2026-09-12): unlike #2, there's
no existing fix or established direction here to confirm — just competing,
untested hypotheses (PHI2 phase/skew at the VIC output stage vs. the MMU
arbitration path) and candidate test methods, same shape as #3/#4.

**Related but not resolving (2026-09-13):** #14's finding (C128 needs a later
`te` than shipped) is consistent with this theory — but equally consistent
with the MMU-arbitration-path theory above, since both predict "C128 needs
more time before the bus settles." Doesn't separate the two. #15's residual
fault (address-bit-to-data-bit coupling, DRAM-mux signature) looks like a
*different* mechanism from either PHI2-timing theory here — worth not
conflating the two open questions just because they're both C128-specific.

### 15. Residual C128 DMA write fault persists even at `te445` `[Investigation]`
From the same write-up as #14. Even with the fixed C128 assert timing, a
small but real fault remains — ~1.4 bad bytes/MB, all partial-byte (some bits
took the new value; none left the byte fully unchanged).

Two preconditions, neither fully explained yet:
- **Needs the C64 program to be actively writing RAM.** A quiet loop (CPU
  parked, no writes) shows zero errors — `DMA_S_StartAsynch`'s normal start
  path needs a write→read boundary to trigger; a quiet program only ever
  starts DMA through its 5000-cycle timeout instead. Whether the *start path*
  or the *ongoing CPU activity* is what actually matters isn't separated yet.
- **Highly variable run-to-run** — identical 3000-write runs at the same
  settings produced anywhere from 0 to 70 bad writes. No condition found yet
  (same shape as #4's intermittency, though a different mechanism/constant).

**A specific, address-dependent signature, not fully consistent:** in
256-byte write sessions, errors clustered on addresses whose low byte has
exactly one 0 bit, with the *specific data bit that drops matching that
address bit's position* (`$C0EF`, A4=0, drops data bit 4). Lone (single-byte)
writes don't hold this up as cleanly — `$C0FF` (no 0 bit) fails about as
often as `$C0EF` when tested alone, just with a different, unfixed bit
position each time.

**Extensively eliminated already** (firmware-only, no scope needed):
- BA sample point — exposed as a new tunable (`Def_nS_DMABAWait`, was a
  hardcoded `WaitUntil_nS(200)` in `DMAByte()`), swept 60–400: worse at both
  ends, no better value than the existing 200. Directly relevant to #9's
  `nS_DMAAssert`/bank-swap cross-reference, though this rules out the BA-wait
  specifically, not `nS_DMAAssert` itself.
- Badlines/VIC-IIe cycle stealing — screen blanked, sprites off: roughly
  halved the error count, so a real contributor, not the sole cause.
- Read path — write once, read back 30×, 0 of 230k reads disagreed: fault is
  write-side only.
- Stray writes into video RAM, write-release (`tw`) timing, address bus
  tri-state switching — all ruled out (holding the address bus driven for a
  whole session instead of releasing per-byte made it *worse*, doubling
  errors, at the same addresses).

**Next step:** firmware-side elimination has run out of road — needs a
scope/logic analyser capture of A0–A7 plus DRAM `/RAS`, `/CAS`, and the
address-mux select on a real C128, comparing a lone write to `$C0EF` against
one to `$C0FF` (different bits fail). `Source/Teensy/tools/dma_scope_write.py`
(imported alongside this) drives single-byte DMA writes from a PC and stops
on the first bad readback, for triggering the scope.

## Validation

### 2. PAL branch of the timing fix is unverified on real PAL hardware `[Validation]`
The sweep that produced `Def_nS_DMADataHoldNTSC=410` was run on a C128; PAL's
430 default is inherited from the old shared constant, not independently
measured. An FPGA C64 can regression-test that the PAL/NTSC switch logic fires
correctly, but not the analog margin (its buffers/bus loading are its own).

### 14. NTSC C128 needs a later DMA assert point (`te`/`nS_DMASetup`) than the shared NTSC set — detected and applied automatically `[Validation]`
Imported from kfox's fork (`C128-DMA-Timing` branch, off `DMA_Timing`) — real
hardware data, not a hypothesis. On a flat NTSC C128, the shipped shared NTSC
`nS_DMASetup` (430) drops bits intermittently: 19–36 bad bytes/MB. Moving the
R/W+address assert point to 445 — later, not earlier — cuts that 13–25×, to
~1.4 bad bytes/MB. Not zero (see #15), but a real, large, measured fix.

Detection: the C128's VIC-IIe implements `$D030` bits 0–1 differently — it
reads `$FC` at 1MHz where a C64's VIC-II reads `$FF`. `MainMenu.asm` now
checks this and sets a new `rvtcC128` bit in `wRegVid_TOD_Clks` alongside the
existing NTSC/PAL bit. `SetVideoStdTiming()` (`IOH_TeensyROM.c`, refactored
out of the inline `wRegVid_TOD_Clks` write handler so the `td` serial command
can call the same logic) then selects `Def_nS_DMASetupNTSC128=445` /
`Def_nS_DMADataSetupNTSC128=375` / `Def_nS_DMADataHoldNTSC128=395` instead of
the plain NTSC set, when both bits are set.

Same exposure as #1: this only ever fires once the menu has reported the
machine type, so an autolaunched C128 never reaches its own set either —
confirmed directly in the write-up ("an autolaunched NTSC C64 gets the write
errors PR #21 fixed, and a C128 gets the same, never reaching its own set
either").

**Not yet independently verified** — tested extensively by kfox on their own
rig, not yet run against this branch's own hardware or reconciled with
`DMA_Timing`'s existing NTSC/PAL constants. PAL C128 has no measured set of
its own yet either; falls through to the plain PAL constants (itself
untested on C128, per #2).

## Closed

### 7. `TestDMAPage()` (uniform-fill diagnostic) has a detectability blind spot `[Closed]`
A write that never lands is invisible to a uniform-fill verify if the page
already held that value. This means historical "passed DMA check" results —
including whatever data underlies the "most C128s fail" line in
`General_Usage.md` — were only ever screened with an instrument that can't see
this failure class on the passing side either. `TestDMAPattern()` (added in
PR #21) is sensitive to it, proven at `0xc000`.

`TestDMAPattern()` only runs at `0xc000` — the other six addresses
`TestDMAPage()` covers (`0x3f00`, `0x4000`, `0x5000`, `0x6000`, `0x7000`,
`0x8000`) are still only screened by the blind method. Not extending it there
is an accepted scope decision, not an open gap: nothing about those addresses
is special, and `TestDMAPage()` running alongside costs nothing to leave in
place. Closing this as "the tool that fixes the blind spot exists and works,"
not "every address has been swept with it."

### 8. `TestDMAPattern()` reports only to serial, not to the C64/C128 screen `[Closed]`
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

Verified on hardware after the fix above — screen output confirmed correct.

### 10. `ExpPortDMA()` doesn't force off active IO Handlers before testing `[Closed — not a bug]`
Initial read: entry to `ExpPortDMA()` ([StatusFunctions.c:845-851](../../Source/Teensy/MinimalBoot/Common/IO_Handlers/StatusFunctions.c)) never touches `CurrentIOHandler`, so whatever handler was active before the diagnostic started would stay live and could cross-talk with the test's own bus activity.

Doesn't hold up: both menu entries that can reach `ExpPortDMA()` ("TeensyROM
External Ports Test", "TR+ C64 Expansion Port Test" — [MainMenuItems.h:294,296](../../Source/Teensy/MainMenuItems.h))
are registered with `IOHndlrAssoc = IOH_TeensyROM`, so launching either from
the menu already sets `CurrentIOHandler = IOH_TeensyROM` via the normal
`IOHandlerSelectInit()` path before the diagnostic program ever runs. And it
has to stay that way: `rCtlExpPortDMAWAIT` (the control-register write that
actually triggers `ExpPortDMA()`) is handled inside `IOH_TeensyROM.c`'s own
IO1 handler — `isrPHI2()` routes every IO1 access through
`IOHandler[CurrentIOHandler]->IO1Hndlr`, so that write literally cannot reach
TR's code unless `CurrentIOHandler` is already `IOH_TeensyROM`. No stale-handler
scenario is reachable by construction, and forcing `IOH_None` would have
broken the diagnostic's own ability to report status/results back to the
still-running C64 program afterward. False alarm - closed without a code
change (2026-09-11).

### 11. `ExpPortDMA()` leaves IRQs disabled on early-return failure paths `[Closed — not a bug]`
Disables `IRQ_ENET`/`IRQ_PIT` for the duration of the self-test. Turns out this
isn't asymmetric between pass/fail as originally framed — all 19 `return;`
points in the function leave them off, and so does the "success" path (which
hands off into a follow-on ROM/GAME/EXROM test phase instead of restoring
them). That's deliberate: `SetUpMainMenuROM()` (`Teensy.ino:341-356`, the
normal way back to the menu from any diagnostic) unconditionally re-enables
both alongside its other resets. No reboot needed, no degraded state survives
returning to the menu — confirmed by the user, and a short comment added at
the disable site in `ExpPortDMA()` pointing at `SetUpMainMenuROM()` so this
doesn't get re-flagged later.

### 12. `tools/BootLinkerFiles/bootdata.c.orig` is stale `[Closed — script being deprecated]`
Differs from the Teensyduino version (`1.61.0`) pinned in `BuildInfo.md` in the
FlexSPI configuration block; `Build-DualBoot.ps1`'s `Copy-LinkerFiles` writes it
straight over `$TeensyCorePath\bootdata.c` — the developer's real, shared
Teensyduino core, not a private copy — so the stale content silently sticks
around for unrelated future builds too.

Not fixing it in place: `Build-DualBoot.ps1` is being deprecated in favor of
`mpe/tools/build.mjs` (`mpe-vm-review`), which doesn't have this problem by
design — every build works in a fresh `mkdtempSync` temp copy of the installed
core and never writes back into the real shared install. This bug is one more
reason for that deprecation, not something worth patching in the outgoing
script. Flagged to the `MeanHamster VM Incorporation` session (2026-09-11).

### 13. `nSToCyc(N)` doesn't parenthesize its argument `[Closed — fixed by calculation, unverified]`
```
#define nSToCyc(N)  (N*(F_CPU_ACTUAL>>16)/(1000000000UL>>16))
```
so `nSToCyc(nS_DMASetup-90)` in `IOH_REU.c:217` expands to `nS_DMASetup -
54` = 386 cycles (643 nS), not the intended 210 cycles (350 nS). The PSRAM
slow-read guard arms far later than the comment implies. The Teensy core's own
version of this expression does parenthesize.

**Two call sites, not one:** `IOH_REU.c:236` has the identical pattern,
`nSToCyc(nS_DMASetup-85)`, guarding the same PSRAM slow-read detection for a
different REU command type (`TypeSwp` vs. `TypeR2C` at line 217) — with its
own separate empirical tuning history in the comment ("fixes block missing
pixels during Bit Fill in CMD 1750 Test"). Every other `nSToCyc(` call site
in the codebase passes a bare variable with no operator inside (`nS_DataHold`,
`nS_MaxAdj`), so those are unaffected — this only bites when the argument is
itself an expression.

Worth noting why both survived: the slope is identical in both expansions
(−0.6 cycles/nS), so the 75 / 80 / 85 sweep recorded at line 217 (and
presumably whatever sweep produced line 236's `85`) behaved sensibly and
converged — locally correct, just offset by a constant amount from what the
comment claims. Which also means fixing the macro invalidates both
calibrations and needs a re-tune on hardware, so it was left alone.

**Fixed at the root** — `nSToCyc(N)`/`CycTonS(N)` in `Common_Defs.h` now
parenthesize `N`. Safe as a global change: every other call site in the
codebase already passes a bare variable (`nS_DataHold`, `nS_MaxAdj`,
`BigBuf[Cnt]`), so parenthesizing `N` is a no-op for all of them — only these
two `IOH_REU.c` sites were ever affected.

Fixing the macro alone would have shifted both guards ~98-100nS *earlier*
than their existing empirically-converged behavior (the old `-90`/`-85`
literals were themselves calibrated against the buggy expansion, not a clean
one) — so the literals were adjusted too, to reproduce the *same* arm point
under the now-correct formula rather than silently moving it:

- `:217`: `-90` → **`+9`** (`nSToCyc(nS_DMASetup+9)`) — matches the old ~367
  cycles/PAL, ~357/NTSC within ~1-2 cycles either standard
- `:236`: `-85` → **`+14`** (`nSToCyc(nS_DMASetup+14)`) — matches the old
  ~371/~361 the same way

Both new literals came out negative-of-the-original (subtracting a negative,
i.e. now adding) because the buggy formula was systematically arming *later*
than its own literal suggested — matching that behavior with a clean formula
needs the sign to flip, not just the magnitude to shrink. The ~99nS shift is
consistent across both sites, which checks out: the gap between the buggy and
correct expansions works out to a near-constant offset largely independent of
which literal you start from, for values this close to each other.

Still **not verified on real hardware** — `USE_PSRAM` isn't in active use on
this project, so there's no live way to sweep-confirm either the original
values or this correction. Closed on that basis: the precedence bug itself is
fixed and protected against recurrence elsewhere, and the two affected sites
are calculated to behave the same as before, not differently — the honest
residual risk is only in the *original* empirical numbers, which was already
true before this fix and is unchanged by it.

<br>

[Back to Architecture Overview](Overview.md)
