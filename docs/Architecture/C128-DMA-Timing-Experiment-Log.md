# C128 DMA Timing — Experiment Log (this rig)

Raw run-by-run log for the local reproduction/validation pass against items #14/#15
in [DMA-Timing-Known-Issues.md](DMA-Timing-Known-Issues.md). Not a polished writeup —
kept for building a later summary. Scratch/working doc, not yet reviewed for the main
tracking doc.

## Setup

- **Firmware:** `Beta_0.8.0.7td` (tag, `Beta_Releases` branch) — `Dbg_SerDMA` +
  `Dbg_SerTimChg` enabled, otherwise identical to the `Beta_0.8.0.7` release (verified
  via `git diff Beta_0.8.0.7 Beta_0.8.0.7td`, 2 files, 3 lines). No C128-detection code
  (that's `C128-DMA-Timing`-only) — timing forced by hand via `--te` on each run instead
  of relying on auto-detection.
- **Hardware:** TR+ (Fab04/full-DMA-capable), real C128 (NTSC — confirmed via the `td`
  default-restore message each run).
- **Tool:** `Source/Teensy/tools/dma_scope_write.py`, single-byte DMA write/readback
  loop, alternating `$00`/`$FF`.
- **`--te` mapping used throughout:** `te430` = stock/unfixed shared NTSC
  `nS_DMASetup` (`tw`=410, `ty`=390 by the script's formula — NOT the dedicated
  NTSC128 set). `te445` = item #14's proposed NTSC128 fix (`tw`=395, `ty`=375 —
  matches `Def_nS_DMADataHoldNTSC128`/`Def_nS_DMADataSetupNTSC128` exactly).
- **Interposer:** a probe-access extender/interposer board was inserted between TR+
  and the C128 partway through this session, specifically to allow scope probing.
  Flagged per-run below — it is a real variable, not a neutral pass-through.

## Runs

| # | te | addr | count | interposer | bad | notes |
|---|----|------|-------|------------|-----|-------|
| 1 | 445 | C0EF | 3000 | out | 0 | |
| 2 | 445 | C0EF | 3000 | out | 0 | |
| 3 | 445 | C0EF | 3000 | out | 0 | |
| — | — | — | — | — | — | port access error (COM91) between runs, TeensyROM UI likely reclaimed it; retried after user closed it |
| 4 | 445 | C0EF | 3000 | out | 0 | |
| 5 | 445 | C0EF | 3000 | out | 0 | **5/5 clean, 15,000 writes total** at te445/C0EF, no interposer |
| 6 | 430 | C0EF | 3000 | out | 0 | unfixed baseline, sanity check |
| 7 | 430 | C0EF | 3000 | out | 0 | |
| 8 | 430 | C0EF | 3000 | out | 0 | **3/3 clean** — even the unfixed setting doesn't reproduce kfox's original #14 fault at this address on this board |
| 9 | 430 | C0FF | 3000 | out | 0 | **27,000 total writes, 0 bad**, both addresses, both settings, no interposer |
| — | — | — | — | — | — | **interposer installed** for scope probe access |
| 10 | 430 | C0EF | 3000 | in | 0 | same address that was clean before — still clean |
| 11 | 445 | C0EF | 3000 | in | 0 | |
| 12 | 430 | C0FF | 3000 | in | **92** | 61 "unchanged" (full byte failed to update), 31 "partial" — first reproduction on this rig. Interposer appears to be the enabling factor: same address/setting was clean without it (run 9). |
| 13 | 445 | C0FF | 3000 | in | 0 | fix holds clean immediately adjacent to a 3%-rate failure at te430, same address, same interposer |
| 14 | 445 | C0FF | 15000 | in | 0 | bigger batch, still 0 — 5x the volume of run 12's 3%-rate failure at te430, same address/interposer, and still clean |
| 15 | 430 | C0FF | 15000 | in | 652 | **4.35% rate** (652/15000). 518 unchanged (79%) / 134 partial (21%) — same lopsided split as run 12. Rate looked stable across the run, roughly 3-5.5% per 1000-write segment, no strong drift (thermal or otherwise). Combined with run 12: 744/18,000 ≈ **4.13%** overall at te430/C0FF/interposer-in. |

## Control test: same interposer on a C64

Run 15's 4.1% rate at `te430`/`$C0FF`/interposer-in raised an open question: is the
interposer exposing a real C128-specific mechanism, or is it just electrically marginal
in a way that would break *any* machine (making the C128 result an interposer artifact,
not real #14/#15 data)? Disambiguating test: same `te430`/`$C0FF`, same interposer,
swapped to a C64.

- **Machine:** NTSC C64 ("trav's system #14" — user's own hardware label, unrelated to
  this doc's item numbering).

| # | te | addr | count | interposer | machine | bad | notes |
|---|----|------|-------|------------|---------|-----|-------|
| 16 | 430 | C0FF | 3000 | in | C64 | 0 | |
| 17 | 430 | C0FF | 3000 | in | C64 | 0 | **0/6000 total on C64**, vs. 744/18,000 (≈4.1%) on the C128 at identical settings |
| 18 | 445 | C0FF | 3000 | in | C64 | 0 | completes the 2x2 (te430/te445 x C0FF) on C64 — clean at both settings, matching the C128's te445 result but not its te430 one |
| 19 | 430 | C0EF | 3000 | in | C64 | 0 | C64 clean at every combination tested so far except te445/C0EF (untested) |
| 20 | 445 | C0EF | 3000 | in | C64 | 34 | **Surprise — first C64 failure in the whole matrix.** 1.13% rate, 22 unchanged/12 partial. Reversal of the pattern so far: C64 clean at te430, fails at te445 (opposite of the C128's te430-bad/te445-clean story). |
| 21 | 445 | C0EF | 3000 | in | C64 | 2 | Repeat of run 20 to confirm. 0.067% rate (17x lower than run 20), both bad writes partial (0 unchanged). Real and repeatable, but highly variable rate — combined 36/6000 across runs 20+21. |

**C64 te445/C0EF is a real, novel finding — not yet in #14/#15, needs its own write-up.**
Plausible mechanism, not yet confirmed: the script's `--te` formula computes
`tw = 840-te`, `ty = 820-te`, so `te445` isn't just "assert later" than `te430` — it also
*shortens* the write-hold/data-setup windows by 15ns each (tw 410→395, ty 390→375). If
C64's own margin was tuned around te430's wider windows, te445 could be eroding
C64-specific margin for a reason unrelated to the C128 mechanism entirely. This would
actually support item #14's design choice to gate te445 behind C128 detection (a new,
separate constant set) rather than changing the shared NTSC default outright — doing the
latter blind, e.g. via the `--te` override on a C64, appears to introduce a real fault
that doesn't exist at the stock te430 setting.

**Conclusion (revised after runs 20/21):** not a generic interposer artifact — but not
simply "C128-specific" either. `te430` is today's *shared* NTSC default (what both
machines already use); `te445` is kfox's *C128-only* proposed addition, never meant to
apply to a C64. With the interposer in: C128 fails at the shared default it's stuck with
today (`te430`, ~4.1% at `$C0FF`) and is clean on the proposed C128-only fix (`te445`).
C64 is clean at the shared default it already uses (`te430`) but breaks
(1.13%/0.067% across two runs) if `te445` — a setting it would never receive under
kfox's gated detection — is forced onto it anyway. So the interposer is exposing real,
machine-specific margin limits on *both* machines, each at whichever setting doesn't
belong to it. That's a direct, accidental confirmation that kfox's design choice — a
separate, detection-gated C128 constant set, rather than raising the shared NTSC default
for everyone — is the right call: raising the shared default would fix C128 but break
C64. Still not a reproduction of #15 specifically (see below) — both findings here are
new, not the partial-only residual fault `te445` was supposed to still be leaving behind
on C128.

## Scope session — te430/C0FF stopped reproducing

Returned to the C128 to take scope shots at the same `te430`/`$C0FF`/interposer-in
condition that gave 744/18,000 (≈4.1%) in runs 12/15. Ran without `--keep-going`
(should stop itself at the first bad write) to leave a capture on the scope — instead
it ran clean to **675,000+ writes** before being manually stopped, no trigger.

That's ~150x the volume that previously caught the fault within a couple hundred
writes. Two candidate explanations raised:

1. **Interposer disturbed** — user confirmed moving/reseating the interposer while
   swapping to the C64 for the control tests (runs 16-21), before returning to the C128.
2. **Probe loading on R/W/D0** — scope probes were added to R/W and D0 for this
   session (`/DMA` and PHI2 were connected throughout, including during the original
   fault-reproducing runs 12/15). Added capacitance on R/W specifically could plausibly
   mask a marginal-timing fault, especially given the dominant "unchanged" (full write
   failure) signature — R/W's transition is what determines whether the write lands at
   all.

**Probe loading ruled out:** re-ran `te430`/`$C0FF` with R/W and D0 disconnected —
**0 bad / 18,000** (3000 + 15000). Still clean without those probes attached, so they
weren't masking anything. Combined with 675,000+ clean writes *with* them attached
earlier, this cleanly rules out probe loading as the explanation.

**Interposer reseated, retested twice more — still clean (0/3000, 0/3000).** Neither
"undo" attempt (unprobing R/W/D0, nor reseating the interposer) brought the fault back.
Running total since the fault last reproduced: **~699,000 writes, 0 bad**
(675,000+ before stopping, +3000 and +15000 with probes off, +3000 and +3000 post-reseat).

**Conclusion: the `te430`/`$C0FF` finding from runs 12/15 is not currently
reproducible, and neither candidate explanation (probe loading, interposer
disturbance) accounts for why.** The most likely reading is that the original 4.1%
rate depended on a marginal/flaky interposer connection state that has since changed —
possibly improved by all the handling during the C64 swap and reseat attempts — rather
than a stable, repeatable electrical-margin effect. This significantly weakens
confidence in that result as real #14-class timing data. **Needs a caveat (or removal)
in the message/summary already drafted for Kelly** — it was presented as validated
findings and should not be, pending either a repeat reproduction or a clearer
understanding of what changed.

**Full matrix re-run (all four cells, interposer still in): all clean.**

| te | addr | count | bad |
|----|------|-------|-----|
| 430 | C0EF | 3000 | 0 |
| 445 | C0EF | 3000 | 0 |
| 445 | C0FF | 3000 | 0 |
| 430 | C0FF | 3000 | 0 |

Every combination that originally distinguished `te430` from `te445` on this board
(runs 12/13/15) is now clean at both settings. Running total since the fault last
reproduced is now **~711,000 writes, 0 bad**. At this point the original 4.1% finding
looks less like "reproducible under condition X" and more like a one-time (or
seating-window-dependent) event — worth treating as unconfirmed until/unless it
reappears.

Two more spot-checks at `te430`/`$C0FF` (same interposer), 3000 writes each: both clean.
Running total now **~717,000 writes, 0 bad** since the fault last reproduced.

## Interposer removed — 10x pass on all 4 corners

With the interposer taken back out, ran all 4 `te`/address combinations 10x each
(3000 writes/run) to build a much stronger direct-connection baseline than the
original 3-run spot checks (runs 1-9).

| te | addr | runs | writes/run | bad |
|----|------|------|------------|-----|
| 430 | C0EF | 10 (+1 verification run) | 3000 | 0 |
| 430 | C0FF | 10 | 3000 | 0 |
| 445 | C0EF | 10 | 3000 | 0 |
| 445 | C0FF | 10 | 3000 | 0 |

**All 41 runs clean — 0 bad / 123,000 writes this pass.** Combined with the original
27,000-write direct-connection baseline (runs 1-9), that's **150,000 total clean
writes with no interposer**, across every `te`/address combination tested, no
exceptions. Direct connection remains completely unable to reproduce anything —
kfox's original fault, the residual #15 fault, or the te430/C0FF finding that briefly
appeared with the interposer in. Whatever's going on only ever showed up with the
interposer in circuit, and even then, only for one session.

(Note: an early attempt at this batch used a broken `grep "writes to \$"` pattern that
matched nothing due to shell escaping — `\$` inside double quotes strips to a bare `$`,
which grep then reads as an end-of-line anchor, not a literal dollar sign. Two full
40-run batches were wasted this way with no usable output before catching it via a
single verification run. Fixed by dropping the `$` from the pattern entirely.)

## Open threads

- Run 12's failure mix (61 unchanged / 31 partial) does **not** match #15's residual
  signature ("all partial-byte... none left the byte fully unchanged") — but run 12 is
  at `te430` (item #14's scenario), not `te445` (item #15's). Still no bad writes at
  `te445` after 18,000 combined writes (runs 2/5/11/13/14 at te445) to actually compare
  against #15's signature — this rig hasn't shown item #15's residual fault at all yet,
  only item #14's original (interposer-dependent) fault at te430.
- `$C0EF` stayed clean on the **C128** across every condition tested (both te settings,
  with and without the interposer) — only `$C0FF` showed anything on that machine,
  a reversal of kfox's own notes (which called out `$C0EF` as the more reliable
  reproducer). But `$C0EF` is exactly where the **C64** failed (runs 20/21, at
  `te445`) — so each machine has its own vulnerable address, not just its own
  vulnerable timing setting. Address-dependence looks real on both machines, just not
  shared between them.
- Interposer's exact electrical contribution (added length? connector transitions?
  loading?) not yet characterized — it went from enabling zero failures to a 3% rate
  at one address/setting, so it is doing real work, not incidental.
