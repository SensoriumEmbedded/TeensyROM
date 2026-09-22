# TR+ NTSC C128 DMA Findings

2026-09-20

## Summary

The TeensyROM+ can use bus-mastering DMA over the C64/C128 expansion port to read and write host memory directly. This is used in features such as REU and the C64Cast project.

On NTSC flat C128s, 2 of the 4 units tested showed intermittent DMA write failures under stress testing, with memory corruption appearing outside the targeted address range. The true rate across NTSC C128s generally isn't established by a sample this small. This investigation characterized the failure mode across all four units and found a single hardware fix that eliminates the fault, since confirmed on both previously failing boards.

Note that NTSC/PAL C64s are not affected, and a low sample size of PAL C128s shows passing as well. Let me know if you see a PAL C128 failing these tests as well.
The easiest way to see if your C128 has this issue is to loop the built-in Expansion Port Diags included in FW 0.8 and higher.

## Experiments Performed

| Experiment | Result |
| --- | --- |
| Address-targeted write/readback stress testing (single-address and full-page patterns) | Reliably reproduced the fault; established the baseline failure signature. Both boards also corrupted memory outside the target range, indicating an addressing issue. |
| Oscilloscope captures (DMA, R/W, address signals at U55, system clock, VCC rail) | Found a real supply-noise burst at U55 coincident with failures; the system clock itself looked clean. |
| Decoupling capacitors added at TeensyROM+'s bus transceivers | No measurable improvement. |
| Decoupling capacitors added at U55 | Helped significantly — roughly 80–500x reduction in failure rate at worst case temp (cold). |
| DMA timing constant sweep (5 parameters) | Confirmed that current settings hit the "sweet spot" for all parameters, no improvement path found via timing. |
| Read/write isolation via firmware debug commands | Confirmed failures are write-side only — reads were perfectly consistent. |
| Chip swap at C128 U55 (74F245 → 74LS245) | **Fixed the write fault entirely on both previously failing C128s**. A small remaining fault on one board was found to be caused by an unrelated/intermittent expansion port connector issue. |
| Multi-board comparison (4 units) | Separated board-specific defects from systemic issues; two of the four units stayed clean throughout, serving as negative controls, while the other two reproduced the faults this report is based on, until repaired. |

## Key Finding

**Chip swap at the C128's U55 bus transceiver (74F245 → 74LS245) is the single most effective fix found.** One board tested with its stock 74F245 failed a stress write test ~85% of the time. Swapping U55 for a 74LS245 eliminated the write fault *nearly* entirely. The <1% remaining fault rate was found to be caused by an expansion port connector issue.

The fix was then reproduced on a second, independently failing board: the same swap took it from consistently faulty to a fully clean 6+ megabyte test pass, with no further tuning. That board also passed 512 consecutive loops of the standard on-device Expansion Port Diagnostics test with zero failures, where it used to fail immediately.

The best current theory for *why* this fix works is not propagation delay (the LS is *slower*) and not Schottky clamping (both 74LS and 74F are Schottky-clamped families, so that doesn't differentiate them) — more likely, 74F245 has substantially higher output drive current and a faster output edge than 74LS245, so when the chip switches its own outputs it pulls a bigger, faster current spike from its own supply than the LS part does for the same transition, consistent with a real supply-noise burst measured on scope coincident with failures.

Of course, not everyone seeing this issue wants to (or is *able* to) reliably desolder a chip from their original C128 motherboard. If you do, be sure to replace it with a *socketed* 74LS245 part, which is the best practice.
A full test suite has been completed with the 74LS245 in place at U55 and no negative impacts have been observed.

## Potential Next Step

Evaluate a dual-supply level-translating bus transceiver for the TeensyROM+ (ie 74LVC8T245) — would require board revision, added BOM cost, and pushing PCB space constraints.
Low confidence this would help on its own assuming the C128-side U55 chip stayed 74F245. This is based on the leading theory which points to that chip's own output stage as the noise source, not the driver feeding it. Deferred pending stronger evidence it's a viable solution.

## References

Related public reports of similar DMA-timing difficulty on NTSC C128s, found during this investigation. This solution may help users of [frntc's RAD cartridge](https://github.com/frntc/RAD) as well.

- [RAD C128 timing discussion](https://github.com/frntc/RAD/discussions/2) — reporting the same symptom class (garbage characters, menu corruption) on NTSC flat C128 units specifically
- [RAD C128 issues discussion](https://github.com/frntc/RAD/discussions/50) — a first-hand account describing NTSC C128 as unusually difficult for reliable DMA timing compared to C64 or C128DCR

## Acknowledgments

- **[Kelly](https://github.com/kfox)** — for the initial findings and debugging scripts
- **Bill** — for the initial U55 LS lead and PAL C128 testing
- **Marco** — for lending me three(!) C128 units to run these experiments
