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

For a distilled, public-facing summary of the NTSC C128 write-fault investigation
(items #6/#15/#16 below), see
[TR+ NTSC C128 DMA Findings](../TR+NTSC_C128_DMA_Findings.md).

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

**Next step:** scope it. Only one call site (`DMAControl.ino`, inside
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

- `DMAControl.ino` — `nS_DMASetup` (address/R-W setup before Phi2 rising)
- `ISRs.c:53,91,107,195,207` — `nS_DMAAssert` (×2), `nS_RWnReady`, `nS_PLAprop`,
  `nS_VICStart` — all inside the main `isrPHI2()` cycle handler and its
  DMA-assert path
- `Common_Defs.h:482,504` — `nS_VICDHold`, `nS_DataSetup` (the non-DMA
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

## Validation

### 2. PAL branch of the timing fix is unverified on real PAL hardware `[Validation]`
The sweep that produced `Def_nS_DMADataHoldNTSC=410` was run on a C128; PAL's
430 default is inherited from the old shared constant, not independently
measured. An FPGA C64 can regression-test that the PAL/NTSC switch logic fires
correctly, but not the analog margin (its buffers/bus loading are its own).

## Closed

### 6. Original C128 PHI2-generation-delay theory still untested against hardware `[Closed]`
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

**Update (2026-09-19), first possible supporting evidence for the MMU-arbitration
side specifically:** confirmed via schematic-level research (not just the earlier
qualitative PRG cross-check) that **U55 (the chip whose F245/LS245 variant this
session has been tracking — see master list item #35) is specifically an address
bus transceiver**, working alongside U17/U18/U19 to perform the MMU's TA
(translated address) / SA (shared address) bus-direction reversal this item already
describes — not a data bus buffer, as first assumed. On Marco2, cooling U55 directly
worsened its DMA write fault rate (50.7% → 84.5%), heating it cleared the fault
almost entirely (0.05%), reversibly (see item #16's update and the experiment log's
"Marco2 — U55 thermal sensitivity" section for the full data). **Originally
misread as evidence for a data-bus simultaneous-switching-noise mechanism — it
isn't, since U55 doesn't carry data.** The better-supported reading: if U55's own
address-bus-reversal timing is temperature-sensitive, a slow/marginal reversal
during the MMU's GAEC-gated hand-off could let a write briefly target the wrong
address rather than (or in addition to) corrupting the intended byte's data — which
would also explain the screen/memory corruption seen beyond the targeted byte range
on Marco1 (see item #16). Still not a scope-confirmed mechanism, but this is the
first hardware evidence pointing at the MMU-arbitration-path side of this item's
two competing theories specifically, rather than treating them as equally
unsupported.

**Update (2026-09-19), first direct scope capture at U55 itself:** using the user's
own schematic, probed U55's `DIR`(pin1)=`/DMA`, `/OE`(pin19)=`/AEC`, A0(pin3), and
SA0(pin17) directly. Confirmed on-scope that `/OE` toggles repeatedly *within* a
single `/DMA`-asserted session (VIC stealing cycles even mid-DMA, per the official
PRG rule already cited above) — so A0/SA0 disagreement during `/OE`-high isn't
diagnostic (nothing forces agreement while tri-stated, and VIC's own unrelated fetch
address may be on the bus then); the window that matters is `/OE`-low, when U55
should actively force the two sides to agree.

Clean (warm) capture: A0/SA0 converge to a matching value within the `/OE`-low
window, after brief ringing. Cold (faulty) captures: **not one consistent
signature** — the first `/OE`-low cycle in a session can still converge cleanly,
while later cycles in the *same* session show a burst of high-frequency ringing on
*both* A0 and SA0 together, with correlated noise appearing on `/DMA`/`DIR` at the
same moments. That correlation across otherwise-unrelated signals points toward a
shared disturbance (ground bounce / supply noise) rather than an isolated
address-line problem — possibly the original data-bus SSN idea in a new form: U55
doesn't carry data, but a real data-bus-driven disturbance elsewhere on the board
could couple into U55's own local reference and destabilize its address behavior,
especially when cold. Probe loading ruled out (similar low fault rate with/without
probes attached).

**Update (2026-09-19), coupling hypothesis confirmed directly, and a practical fix
found.** Traded a probe for U55's own VCC (AC-coupled, 50mV/div — DC coupling at 1V/div
would hide a real sag this small). Result: a real, large noise burst on VCC, landing
exactly coincident with the A0/SA0 ringing, both in the same `/OE`-enabled window. Not
just correlated timing — an actual voltage disturbance at U55's own supply pin at the
moment its address output can't settle.

Direct test: added a supplemental decoupling capacitor across U55's VCC/GND (parallel
with the existing 0.1uF), re-ran the same cold/`te445`/`$C0FF` condition. **Clean
dose-response**: no extra cap ~85% fail → `+0.1uF` 1.05% (~80x better, and the
signature cleaned up to almost entirely one bit, `$7F`) → `+0.1uF +1.0uF electrolytic`
0.15% (further improvement). More capacitance at U55 = progressively less failure —
strong, practical confirmation that insufficient local decoupling at U55 is a major,
real, directly-fixable contributor, not just a scope-observed correlation. Residual
~0.15-1% may be the genuine underlying timing-margin issue this whole item chain has
been chasing, previously masked by much larger power-integrity chaos.

**Best current theory for *why* LS245 fixes it (2026-09-21), from working through the
electrical difference with the user.** Not propagation delay, and not "Schottky vs.
non-Schottky" (both 74LS and 74F are Schottky-clamped families — LS = Low-power
Schottky, F = Fast/Advanced Schottky, so that dimension doesn't differentiate them).
The more likely lever is **U55's own output drive current and slew rate**: F245 has
substantially higher output drive current and a faster output edge than LS245. When
U55 switches its own outputs, F245 pulls a bigger, faster current spike from its own
VCC/GND than LS245 does for the identical transition — the textbook mechanism for
ground bounce / simultaneous-switching noise, and it's intrinsic to U55's own output
stage, independent of what's driving its input side. This fits the hardware evidence
better than a pure timing-margin story: the VCC probe above found a real noise burst
on U55's own supply coincident with the failures, and a bigger di/dt from a
higher-drive output stage is a more direct explanation for that than delay alone
(delay shifts *when* a transition happens, not how violently it happens).

One open tension this doesn't fully resolve: U55 is address-only (A0-A7), but the
fault signature has always been data-value-dependent (`$FF` fails, `$00` doesn't). If
the mechanism is U55's own switching noise, it has to be coupling into the data path
via a shared rail/ground-return impedance, not a direct connection — consistent with
the "unified mechanism" framing below, but the exact coupling path is still not
proven, just theorized.

**Update (2026-09-19), TR+'s own schematic checked — confirmed facts, after two
wrong guesses along the way.** `PCB/archive/v0.4 TRPlus/TeensyROM_v0.4_Schem.pdf`:
the Teensy 4.1's GPIO pads don't drive the C64/C128 bus directly — four 74LVC245
transceivers sit in between (`U2`-`U5`), plus one 74LVC07 hex buffer (`U6`) for
single-bit control signals. The schematic PDF's OCR text doesn't reliably preserve
which labels belong to which chip in a dense drawing — guessed `U5` then `U2` as
the data bus, both wrong, before confirming directly from the PCB layout:
**`U2` = address bus A0-A7 (drives U55 directly), `U3` = the actual data bus D0-D7.**
Each chip has its own single 0.1uF cap (`U2`/`C3`, `U3`/`C4`, `U4`/`C5`, `U6`/`C6`,
`U5`/`C7`) — the same starting configuration already shown insufficient at U55.

Mechanism also corrected: `DMAByte()`'s `SetAddrBufsOut`/`SetAddrBufsIn` don't
enable/disable U2's outputs, they flip its `DIR` pin — `/OE` is tied permanently to
`GND` on these chips, so outputs are never tri-stated, and a `DIR` flip is an
instant full reversal on all 8 lines at once. This happens once per byte
transferred (once per `DMAByte()` call, timed to roughly a half-`Phi2`-cycle
window), so a 256-byte page means 256 reversal events in one session — fits the
recurring ringing seen across multiple `/OE`-low windows better than a
one-transient-per-session story. Since U2 drives U55 directly, its switching lands
on U55 with no indirect coupling needed — though U2's `DIR` flips happen the same
regardless of `$00` vs `$FF`, so U3 (data, confirmed) is the more likely source of
the `$FF`-specific asymmetry seen all day; both are candidates, not competing
explanations. Plan: add supplemental capacitance to all five cap locations
(`C3`-`C7`) at once, since they're physically adjacent — thorough test of the whole
bus-driver bank rather than isolating one chip first. Staggering the 8-bit write
was considered as an alternative firmware mitigation but deprioritized — timing
budget is already tight everywhere in this investigation, and capacitance is
lower-risk and already validated. Full detail: experiment log's "Marco2 — U55 scope
session", "Supplemental decoupling at U55", and "TR+ as a candidate noise source"
sections.

**Update (2026-09-20), Marco1 — a second data point, and U55 confirmed F245.** Marco1's
screen filled with garbage again during z999 testing, the same symptom that first prompted
this theory on 2026-09-19 — corruption appearing outside the `$c000-$c0ff` target range,
which a pure data-bit-value fault (wrong value, right address) can't explain but a write
occasionally landing at the *wrong address* during the MMU's DMA hand-off can. Opening
Marco1's RF shield (never done before) confirmed **U55 is F245** — the same chip family
this whole theory, and Marco2's independent thermal-sensitivity findings, are built around.
Cold (freeze spray) on Marco1's U55 didn't reproduce Marco2's clean, consistent
cold-worsening pattern, though — it added chaos/variability instead (one pattern dropping
below baseline while two normally-clean patterns spiked), more consistent with Marco1's
already-documented high run-to-run variability than a clean thermal signal on its own. Still
unconfirmed, but now with two independent data points and a shared candidate chip. The
LS245 swap that fixed Marco2 is a natural next test here too, since it would validate or
rule out the theory by fixing (or not) both the write-reliability numbers and the
escaping-target garbage at once — not yet done. Full detail: experiment log's "Marco1
revisit — F245 confirmed, address-targeting theory strengthened" section.

**Update (2026-09-21) — the swap was done, and it's decisive.** A rough day first: a
fresh-power-on baseline regressed sharply on completely unmodified hardware (11.35% vs.
the prior day's 2.15%, now failing in both bit directions), an inconclusive thermal test
on U62 (the C128's upper-address-bus buffer, a chip not previously investigated), a
mid-session Teensy hardware failure (replaced), and the discovery of two undocumented
bodge wires on the board's underside from an apparent prior repair. Then: U55 swapped
F245 → LS245, same as Marco2. **5/5 immediately clean runs, ~6.4MB, 0 bad** — matching
Marco2's exact clean standard, bodge wires notwithstanding. A fully clean full-page test
means every intended address read back correctly every time, which argues against the
address-mis-targeting theory being active in this state, not just against the raw
failure rate. **The same single fix has now taken two independent boards from clearly
faulty to fully clean.** Full detail: experiment log's "Marco1 — new-day regression, U62
thermal test (inconclusive), bodge wires found, LS245 swap decisive" section.

**Closed (2026-09-21):** Resolved by the U55 chip-swap investigation above — the best-supported mechanism is now U55's own output drive current/slew rate (ground-bounce-style supply noise on switching), not propagation delay or Schottky clamping, and a validated practical fix exists (swap U55 74F245 -> 74LS245). See [TR+ NTSC C128 DMA Findings](../TR+NTSC_C128_DMA_Findings.md).

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

### 14. NTSC C128 needs a later DMA assert point (`te`/`nS_DMASetup`) than the shared NTSC set — detected and applied automatically `[Closed]`
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

**Third independent confirmation (2026-09-19):** a borrowed NTSC C128
(`Marco1`) reproduces this fault cleanly at `$C0EF` — whole-byte `unchanged`
failures at `te430` (15/3000, ~5,000 bad bytes/MB), completely fixed by
`te445` (0/6000). Same direction as kfox's own rig and Travis's own brief,
unconfirmed-on-retest reproduction (see
[C128-DMA-Timing-Experiment-Log.md](C128-DMA-Timing-Experiment-Log.md)), on a
third, independent machine — real validation value for the fix itself, though
at a notably higher magnitude than kfox's own 19–36 bad bytes/MB (~140-260x).
This same board also shows a separate, `te`-independent fault at `$C0FF` — see
#15's update, not part of this item's finding.

**Complication (2026-09-19), Marco2:** on this board (confirmed F245 at U55), `$C0EF`
still shows this item's whole-byte `unchanged` signature at `te430` — but **`te445`
makes it worse, not better** (17.5% vs. 8.5% at `te430`), the opposite of this fix's
intent and the opposite of every other machine tested (kfox's rig, Marco1, Travis's).
Given the same board's `z` sweep (see #15's update) points to a severe, non-timing
drive-strength issue on this specific F245 chip, the likeliest read is that Marco2's
underlying hardware fault is severe enough to swamp whatever benefit `te445` provides
here — not that `te445` itself is wrong. Doesn't change this item's validation on
Marco1/kfox's hardware, but means the fix can't be assumed to help on every board
regardless of its own hardware condition.

**Closed (2026-09-21):** Now confirmed across five independent configurations — kfox's own rig, Marco1, Marco2 (both F245 and post-swap LS245), and Marco3's clean baseline — all agree `te445` is correct or neutral. The one outlier (Marco2 getting worse at `te445` while still F245) is explained: that board's F245-specific drive-strength fault was severe enough to swamp the benefit; once U55 was swapped to LS245, a direct re-sweep confirmed `te445` remains the right setting there too (see item #15's 2026-09-20 `te`-sweep update).

### 15. Residual C128 DMA write fault persists even at `te445` `[Closed]`
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
one to `$C0FF` (different bits fail). `tools/dma_scope_write.py`
(imported alongside this) drives single-byte DMA writes from a PC and stops
on the first bad readback, for triggering the scope.

**Update (2026-09-19):** a borrowed NTSC C128 (`Marco1`, TR+, FW `0.8.0.9`/`0.8.0.9td`)
shows a real fault at `$C0FF`, but a full `te430`/`te445` matrix (see the experiment
log) shows it's **not a match for this item's residual signature — it looks like a
separate mechanism.** Rate is 2.2-2.6% either way (`te430` 65/3000, `te445` 153/6000,
~21,700-25,500 bad bytes/MB) — `te445` doesn't meaningfully reduce it here, unlike
kfox's own 13–25x improvement at this item's residual fault. Signature is also unusually
specific: bits 1 and 3 (`$FD`/`$F7`) account for nearly every bad byte, both `te`
settings — a tighter, more consistent pattern than this item's own "different, unfixed
bit position each time" note above. Whatever's happening at `$C0FF` on this board,
`te445` isn't touching it, so it probably isn't this item's fault at a worse rate — see
the experiment log's "Marco1" section for the full matrix and open candidate
explanations (board-specific hardware issue, unconfirmed).

**`z` sweep at `$C000` (2026-09-19), triggered directly over serial:** a third distinct
signature on the same board. All 5 of `TestDMAPattern()`'s sub-patterns run cleanly
through (`z` has no short-circuit, unlike `ExpPortDMA()`), 255,744 bytes each. Real but
low-rate faults (~4-293 bad bytes/MB, far below `$C0FF`'s ~2.2-2.6%), with a very
consistent theme across every pattern: **writing `$FF` (driving bits high) fails, `$00`
doesn't**, concentrated in the low-order bits (0-3) — broader than `$C0FF`'s bit-1/3-only
signature. Even the control pattern (nothing should change) shows a trace amount at this
sample size. Full table in the experiment log's "z sweep" section.

**Update (2026-09-19), Marco2 — confirmed F245, mechanism identified:** a second
borrowed C128 (`Marco2`, confirmed F245 at U55) shows the same "`$FF` fails, `$00`
doesn't" theme as Marco1's `$C000` sweep, but roughly 600x worse and spread across all
8 bits instead of just 0-3: writing a page to `$ff` fails ~15-20% of the time,
regardless of whether it's the actual test pattern or just a prefill step, overwhelmingly
as complete whole-byte drops. Every single-byte corner also fails, at rates from 4% to
82%. This looks like a textbook simultaneous-switching-noise/drive-strength failure — 8
outputs slamming high at once — not this item's marginal-timing mechanism at all. First
quantified, TeensyROM-specific support for master list item #35's F245 concern. Full
detail: experiment log's "Marco2" section.

**Update (2026-09-19), Marco3 — clean negative control:** a third borrowed C128
(`Marco3`) shows none of this — full corner matrix (6 combos, 27,000 writes) and `z999`
sweeps at both `te430`/`te445` (~2.56MB, including the exact `$ff`-write patterns
catastrophic on Marco2), all clean. Confirms neither this item's fault, #14's, nor
Marco2's mechanism is universal to all C128s — genuinely board-specific. Full detail:
experiment log's "Marco3" section.

**Update (2026-09-20), Marco2 — U55 swapped F245 → LS245, the most decisive result of
the whole investigation.** Directly tests master list item #35's F245-vs-LS245 question
(Bill's own hands-on RAD/SIDKick fix) against TeensyROM specifically. No supplemental
U55 caps needed with the LS245 (see #16's dose-response fix above — that was an F245-only
workaround). Single-byte alternating test (`te445`/`$C0FF`): 5/5 runs clean, 10,000
writes, 0 bad, room temp — cleaner than F245 ever got even with its best decoupling
(0.15%) — and *also* 0/2000 cold (freeze spray), a stark contrast to F245's catastrophic
cold failure (85% baseline). The harsher `z999` sweep — the test that originally caught
F245's 15-20% catastrophic failure above — is not perfectly clean: the same "`$FF` fails,
`$00` doesn't" signature persists (bits only fall 1→0, spread across all 8 bits), but at
roughly 60-150x lower rate (room temp 0.11-0.25%, cold 0.12-0.19%, thermally flat unlike
F245). **Conclusion:** LS245 eliminates F245's signature thermal sensitivity entirely and
fully fixes the single-byte-write fault; what remains is a small, temperature-independent
residual under heavy simultaneous-switching stress — likely a genuine, separate, low-level
timing-margin issue distinct from the F245-specific mechanism, not something a chip swap
would be expected to touch. Full detail: experiment log's "U55 chip swap, F245 → LS245"
section.

**Update (2026-09-20), Marco2/LS245 — `te` swept directly, confirms the residual really
is timing-related, and finds why it can't easily be dodged.** Both z999 runs above left
visible screen/menu garbage afterward, same as Marco1's 2026-09-19 finding — not confined
to the tested page. A coarse `te` sweep (400-500, room temp) found a real, non-monotonic
structure: relatively flat 0.2-0.7% from 400-460, a sharp bad spike at 480 (13.9%,
confirmed on repeat), and a sharp improvement at 500 (0.02%, confirmed 3/3 runs) — not
flat noise. But mapping past 500 in finer steps found zero margin: te505 already jumps to
4.0% bad, climbing to 6.0% (510), 4.5%→17% (515→520), and 74% (540) — a knife-edge, not a
usable improvement. The failure direction flips past te500 too (writing `$00` becomes the
dominant failure, vs. writing `$FF` for every other result all session) — a qualitatively
different symptom. This matches `nS_DMASetup`'s own firmware definition
(`Common_Defs.h`, "delay from Phi2 falling to RW/Addr setup, just before rising edge")
and a similar collapse already documented in that file's comments from an entirely
different board/session (380 collapses, 440-450 clean, 465+ collapses) — strong evidence
this is a genuine Phi2-low-phase-boundary effect, present identically on LS245 (which
showed none of F245's other problems), not a leftover trace of the F245-specific
mechanism. **te445 remains the right setting** — te500 is real but has no safety margin
and isn't a safe recommendation for any board. Full detail: experiment log's "`te` sweep
on the residual — Marco2/LS245" section.

**Update (2026-09-20), Marco2/LS245 — the remaining four DMA timing knobs (`ta`/`tb`/
`tw`/`ty`) swept too, none offer an actionable improvement.** A real verification gap
was found and fixed along the way: neither sweep tool logged the firmware's full
confirmation listing (the only place `ta`/`tb` ever appear — the `z999` status line only
echoes `te`/`tw`/`ty`), so the first `tb` pass had no direct proof its override, or the
other four knobs' state, actually took effect. Both `tools/dma_scope_write.py` and
`tools/run_z_sweep.py` now capture and log that listing on every run. `tw`
(`nS_DMADataHold`, write data-hold): flat/gentle 0.11-0.25% from 300-430, then a total
catastrophic collapse at 450 (37-56% across every pattern, including the
previously-always-clean `$ff over $ff` control) — matches `Common_Defs.h`'s existing
"455+ overruns Phi2 falling" comment for the sibling constant almost exactly; default
(395) has ~55ns margin. `ty` (`nS_DMADataSetup`, read data-latch): hard walls on both
sides — catastrophic below ~300 (matches the existing "too soon = bad reads" comment)
and, newly found, catastrophic above ~425-450 (not previously documented); flat plateau
0.42-0.46% in between; default (375) centered with good margin. `tb` (`nS_DMABAWait`,
steal-cycle sample point): noisy 0.53-1.25% across 60-400, no catastrophic wall, no clear
trend — possibly a modest real noise contributor, but no actionable better setting. `ta`
(`nS_DMAAssert`, once-per-session assert): completely flat 1.01-1.08% across 0-400, no
measurable effect at all — expected, since it fires once per DMA session rather than once
per byte like the other four. **All five DMA timing knobs are now characterized on this
board/config; only `te` has a real (if unusably fragile) better point, and none of the
others move the small residual at all.** Full detail: experiment log's "Verification gap
found and fixed, then `tb`/`ty` swept" section.

**Major reframe (2026-09-20, later the same day): the residual itself turned out to be
connector-state-dependent, not a fixed timing-margin floor.** After the TR+ was
reinserted following an extended power-off, 5/5 consecutive `z999` runs came back
**perfectly clean** (0 bad, all 5 patterns, ~6.4MB) at default timing — the first
perfectly-clean `z999` result all session, after an entire afternoon of runs at every one
of the five knobs' settings showing the 0.1-1.2% residual. A quick reseat (no extended
power-off) stayed clean too (6/6 total), weakening "long power-off" specifically as the
cause — more likely just the same general connector-state sensitivity already established
for this board (see item #16). Removing the weight brought back a tiny residual
(6/255744, 0.0023%, plus single-digit prefill counts) — real, but two orders of magnitude
smaller than the afternoon's 0.1-1.2%, which was measured *with* weight on — so pressure
alone doesn't explain the size of the afternoon's swing; the connector likely also
genuinely improved through the day's many reseats (ordinary contact wiping), with pressure
acting as a smaller modulator on top. Reapplying weight returned it to clean.
**Conclusion: the whole afternoon's `te`/`tw`/`ty`/`tb`/`ta` sweep characterized a residual
that was itself a connector-state artifact at the time, not a hard physical timing limit.**
The individual knob findings (the `tw`/`ty` hard walls, `te`'s knife-edge at 500, `tb`'s
noise, `ta`'s total insensitivity) are still real — but the baseline they were measured
against wasn't as fixed as it looked. In a good connector state with weight applied, this
board now demonstrates a full clean 6.4MB `z999` pass, matching Marco3's own clean
standard. Full detail: experiment log's "The residual reframed — it was connector-state,
not a timing floor" section.

**Update (2026-09-20), Marco2/LS245 — write vs. read isolated directly, decisive.** Using
the firmware's `u`/`v` debug commands (`SerUSBIO.ino:98-146`, a bulk DMA write/read-compare
over `$0c00-$a000`, separate from the `z999`/`dma_scope_write.py` tooling used everywhere
else in this investigation): repeated `v` (read+compare) calls against *unchanged* RAM
contents returned the exact same miscompare count and the same specific values every
time — no read-to-read variation at all. Repeating `u` (write) reduced the miscompare
count — some previously-wrong bytes got corrected by re-writing the same data. This
cleanly isolates the fault: DMA reads are perfectly deterministic (faithfully reporting
whatever's actually in RAM), and the *stored RAM content itself* is sometimes wrong after
a write — a fresh write attempt has an independent chance of succeeding where the last one
didn't. **Confirms the fault lives entirely in the write path, not in DMA reads or `ty`'s
read-latch timing** — closes off read-side error as a possible confound for every
read-verified write test run this session. Separately, a PHI2 scope session (4 channels:
`/DMA` trigger at U55, PHI2/"1MHz" at both the expansion port and CIA2, R/W at CIA2) caught
3 live bad writes via a new page-sweep capture tool (`dma_scope_page_sweep.py` —
`dma_scope_write.py` alone couldn't reproduce the fault with 30,000 single-address writes;
it needs `z999`'s full-page prefill-then-sweep pattern) — PHI2 looked clean on every
capture, no visible ringing on either tap. Full detail: experiment log's "Further
reseat-state characterization, and a PHI2 scope session" section.

**Closed (2026-09-21):** Fixed. Swapping U55 (74F245 -> 74LS245) eliminated this residual entirely on both previously-faulty boards: Marco2 and Marco1 each went from a real, measurable residual fault to 5/5 clean runs (~6.4MB, 0 bad). Marco1 additionally passed 512/512 loops of the on-device diagnostic with zero failures. See [TR+ NTSC C128 DMA Findings](../TR+NTSC_C128_DMA_Findings.md) for the summary.

### 16. Mechanical connector pressure and power-cycle state are major, confirmed variables in DMA write reliability `[Closed]`
Discovered while re-baselining before further Marco1/Marco2 work (2026-09-19). Two
separate, real effects, both confirmed reproducible on real hardware:

**Connector pressure (Marco2):** pressing down vs. lifting up on the TR+/connector
interface, at the identical `te445`/`$C0FF` condition, swings the failure rate roughly
3x (33% vs. 94%), confirmed identically across two separate trials before and after
cleaning the connector. Cleaning itself made no measurable difference — not simple
oxidation. Even the best pressure condition still shows a real ~33% fault rate, so this
doesn't explain the whole fault, just a large fraction of its variability. Some of the
worst-case runs show readback values stuck at a constant, independent of what was
written — a symptom more consistent with a genuinely bad connection (marginal solder
joint, partially open pin) than with electrical margin alone.

**Power-cycle state (Marco1):** the same `te445`/`$C0FF` condition went from
consistently faulty (1.75-3.42% across 3 runs, with the affected bits changing every
time even with nothing touched) to completely clean (0/6000) immediately after a power
cycle — twice in a row. Screen-blanking (see below) also produced a large improvement,
but was run in the same window as a power cycle, so the two are confounded and neither
has been independently isolated yet.

**Badline/VIC-timing state (Marco1):** confirmed a `--blank-screen` flag on
`tools/dma_scope_write.py` (DMA-writes `$D011=$00`/`$D015=$00` before the loop, no
firmware change) reduces `$C0FF`'s rate ~10-17x (3.42% → 0.2%) — bigger than item #4's
original "roughly halved" finding for the `$C0EF` fault, and points the same direction.

**This reframes a meaningful part of this session's data.** The connector-independent
findings (kfox's item #14 fix, confirmed on 3 machines) still stand — but a lot of the
noisier, harder-to-pin-down signatures this session found (Marco1's shifting bits,
Marco2's chaos, the general "highly variable run-to-run" note under item #15) may be
substantially driven by connector contact state and badline timing, not purely
board-specific silicon variance. Full detail, all run data: experiment log's "Marco1
revisit" and "Marco2 revisit" sections.

**Update (2026-09-19), Marco1 chosen as primary focus — pressure-insensitive, and
visible memory corruption confirmed.** Unlike Marco2, connector pressure has no effect
on Marco1 (1/6000 both with no pressure and while lifting up) — its fault looks like
genuine timing/runtime-state marginality, not a bad connection. A full suite re-run
came back clean everywhere except `z999`/`te430`, which reproduced the same "writing
`$FF` fails, `$00` doesn't" signature the original pass found at `te445` — the
timing-sensitivity flipped which setting shows it, but the mechanism itself has been
consistent across every `z999` run on this board all session, the one stable thread in
otherwise-drifting data.

**More seriously: after that run, the C64/128 menu screen showed visibly garbled text
and graphics**, despite `z999` only ever targeting `$c000-$c0ff` — well outside screen
RAM. Screenshot: `docs/Architecture/images/Marco1-menu-corruption-2026-09-19.png`. This
directly confirms the "rolling garbage"/stray-characters symptom reported earlier the
same day was real, not incidental — the marginal-write condition is not cleanly
confined to the single targeted byte. Consistent with the looped `ExpPortDMA()`
diagnostic on this board resetting after a few loops rather than just reporting bad
bytes. Not yet investigated further, but any future scope capture should account for
this — a capture scoped only to the tested write may not show the whole mechanism.

**Update (2026-09-19), Marco2 — U55 thermal sensitivity, a third independent
variable.** With connector pressure held constant (weight, not hand pressure), cooling
U55 directly (freeze spray) worsened the rate 50.7% → 84.5%, reversibly (recovered to
6.5% after warming back up); heating U55 directly cleared it to 0.05%. **Initially
misread as the same mechanism as the connector-pressure finding above — it isn't.**
Pressure was applied at the TR+/expansion-port connector; heat/cold was applied
directly to U55, a separate physical location on the motherboard with no indication of
its own connection problem.

**Correction (2026-09-19, same day):** also initially read this as evidence for a
data-bus simultaneous-switching-noise mechanism (per the `z999` sweep data) — that
doesn't hold up either. **U55 is confirmed to be an address bus transceiver** (part of
the MMU's TA/SA bus-direction-reversal, alongside U17/U18/U19 — see item #6's update),
not a data bus buffer, so it has no direct path to cause the data-bit corruption the
`z999` sweep found. The better-supported reading now points at item #6's
MMU-arbitration-path theory instead: temperature-sensitive address-bus-reversal timing
at U55 could cause a write to briefly target the wrong address during the DMA hand-off,
which would also explain the screen/memory corruption seen beyond the targeted byte
range on Marco1, above. Still gives a reliable, on-demand way to induce a high failure
rate (cold-spray U55 → ~85%) for future scope work, rather than waiting on natural
drift — just pointed at a different underlying mechanism than first thought. Full
detail: experiment log's "Marco2 — U55 thermal sensitivity" section, item #6's update.

**Update (2026-09-20), Marco2 — connector pressure/mechanical settling reconfirmed
as the dominant same-day variable, thermal ruled out.** A morning re-baseline (same
condition as yesterday's clean 0-0.15% end state — room temp, U55 supplemental caps
still connected, nothing deliberately changed overnight) unexpectedly regressed to
93.5% bad, and restoring the weight (previously this board's strongest positive
lever) made it worse, not better (93.5% → 98.45%). Adding more weight plus mild U55
self-heat (board simply powered on, no external heat source) produced two clean
runs — but then flipped back, with nothing touched, into a stuck-bad regime that
held steady across 4 consecutive runs (~93-99.7%, one partial 30.9% excursion),
each with its own distinct sticky readback value ($54-56, then $70/71/78, then
$79). Ruled out "too hot" — the user confirmed today's self-heat was well below
yesterday's beneficial heat-gun level, which only ever helped. A fresh connector
reseat (no thermal change) immediately recovered clean operation (1.05%, single
consistent `$00`→`$80` bit-stuck signature) and stayed stable across 3 more
consecutive runs (0.1-0.5%). Read together with 2026-09-19's finding above:
connector pressure isn't just sensitive to hand pressure at the moment of testing —
a fixed weight can apparently creep/settle to a worse-contact position over several
minutes with nothing touched, meaning "pressure applied" is not a stable state by
itself and needs periodic reseating, not just initial placement. U55's decoupling
fix remains intact and unrelated — it fixes local supply noise at U55, not this
separate mechanical variable. Full detail: experiment log's "Marco2 — overnight
regression and connector-pressure/mechanical-settling confirmation" section.

**Closed (2026-09-21):** Fully characterized, not just discovered. Connector pressure, power-cycle state, and mechanical settling are all confirmed, reproducible variables (see the updates above), and are now factored into how this investigation's own results are read and reproduced. This isn't something with more data left to gather — it's a real, permanent characteristic of the connector interface, not an open question. Closing as characterization-complete rather than fixed.

<br>

[Back to Architecture Overview](Overview.md)
