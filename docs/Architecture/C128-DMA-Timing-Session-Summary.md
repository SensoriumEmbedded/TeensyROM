# C128 DMA Timing — Local Validation Summary

Summary of a local hardware pass validating kfox's C128 DMA timing fix (from the
`c128-dma-timing` fork, imported here as `C128-DMA-Timing` off `DMA_Timing`) against
Travis's own C64 and C128. Full run-by-run data in
[C128-DMA-Timing-Experiment-Log.md](C128-DMA-Timing-Experiment-Log.md); this is the
narrative version.

## Setup

- Firmware: `Beta_0.8.0.7td` (tagged beta build with `Dbg_SerTimChg`/`Dbg_SerDMA`
  enabled, otherwise identical to the `Beta_0.8.0.7` release) — does **not** include
  kfox's C128-auto-detection code, so timing was forced by hand via serial (`te###`)
  rather than relying on machine detection.
- Tool: `Source/Teensy/tools/dma_scope_write.py` — repeated single-byte DMA writes to
  one address, alternating `$00`/`$FF`, with readback verification.
- Two `te` (`nS_DMASetup`) values compared: `430` (today's shared NTSC default, used by
  both machines currently) and `445` (kfox's proposed C128-only fix, meant to be
  applied only when C128 is auto-detected).
- Two addresses: `$C0EF` and `$C0FF`, the two kfox's own writeup called out.
- Midway through, a probe-access interposer was added between the TR+ and the target
  machine (needed for a planned scope capture). It turned out to matter a great deal —
  see below.

## Findings

**1. Direct connection (no interposer): everything clean.**
27,000 writes on the C128 (both `te` values, both addresses) and clean runs on the C64
at the matched conditions — no reproduction of kfox's original fault, and no sign of the
residual fault either, on a straight TR+-to-machine connection.

**2. With the interposer in, kfox's core finding initially appeared to reproduce on
this C128 — but did not hold up on further testing.** At `$C0FF`: `te430` (today's
shared default) first showed **~4.1%** (744 bad / 18,000 writes across two batches);
`te445` (kfox's fix) was clean, 0/18,000, same address, same interposer — direction
matched kfox's writeup exactly. However, returning to take scope shots at the same
`te430`/`$C0FF` condition, the fault could no longer be triggered at all. Extensive
follow-up (removing the scope probes added in the meantime, reseating the interposer,
repeating the full four-way `te`/address matrix) never brought it back — **~711,000
further writes, 0 bad**, across every combination that had originally distinguished
`te430` from `te445`. **This finding should be treated as unconfirmed, not validated.**
The most likely explanation is that the original 4.1% rate depended on a
marginal/flaky interposer connection state rather than a stable electrical-margin
effect — but that's not confirmed either, since deliberately trying to restore or
disturb the connection didn't reproduce it or explain its disappearance.

**3. The C64 control run's conclusion is now in question too, since it depended on
finding 2.** Same interposer, same `te430`/`$C0FF`, swapped onto Travis's NTSC C64:
clean, 0/6000, at the time. The original interpretation — "not a generic interposer
artifact, since it should have hit the C64 too" — assumed the C128 side was a stable,
reproducible effect. Given finding 2 no longer holds up, this comparison doesn't prove
much on its own anymore. Not retested since.

**4. Unexpected: forcing kfox's C128 fix onto the C64 appeared to introduce a new
fault — status uncertain given finding 2.** Not something either of us set out to
find. With the interposer in, forcing `te445` (the C128-only setting) onto the C64 at
`$C0EF` produced what looked like a real, repeatable fault — 1.13% then 0.067% across
two runs (36 bad / 6000 combined), nonzero both times. `$C0EF` is otherwise clean on
the C64 at `te430`. This was never retested after the interposer/probe instability
turned up in finding 2, so given that the C128 finding from the same session and same
interposer setup didn't hold up under repeat testing, this one should be treated with
the same caution until it's independently reconfirmed — not assumed solid just because
it happened twice in a row at the time.

**5. The original target of this session — item #15's residual fault at `te445` — never
reproduced here at all.** 18,000 writes at `te445` on the C128, both addresses, with and
without the interposer, all clean. Whatever kfox found on their rig at a low rate after
the fix, this specific C128 isn't showing it (at least not at these two addresses, this
sample size).

## Bottom line

- **This pass is inconclusive, not a validation.** A C128 fault matching kfox's
  direction (bad at `te430`, clean at `te445`) appeared to reproduce reliably at
  `$C0FF` with the interposer in, but stopped reproducing on return and never came back
  despite ~711,000 further writes and multiple attempts to restore the original
  conditions. Neither of the two obvious explanations (added scope probes, interposer
  seating) accounts for it cleanly. The downstream C64 finding (a real-looking fault at
  `te445`/`$C0EF`) rests on the same interposer setup and hasn't been reconfirmed since.
- What's still solid: **direct-connection testing (no interposer) is clean everywhere**
  — 27,000+ writes, both machines, both `te` values, both addresses. No evidence either
  way on kfox's fix from that condition; it simply doesn't reproduce anything on this
  hardware without the interposer in play.
- Item #15 (the residual post-fix fault) remains open and unreproduced on this
  hardware — no data for or against it from this pass.
- The interposer needed for scope work is clearly not a neutral, stable probe point —
  whatever it did to produce the original 4.1% finding, it isn't currently doing it,
  and we don't yet understand why either state occurred. Any future scope-based work
  through this interposer needs to account for that instability before trusting a
  single run's result.
- Not yet done: the auto-detection logic itself (the `$D030` C128-vs-C64 check) hasn't
  been tested — everything above used manual `--te` overrides on a build without that
  code. PAL is entirely untested. A scope capture was planned at the `te430`/`$C0FF`
  fault specifically because it was (at the time) the one reliably reproducible C128
  condition — that's no longer true, so the scope plan needs to be revisited once (if)
  the fault reappears.
