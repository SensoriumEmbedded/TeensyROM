# C64/C128 Expansion Port DMA — Official Documentation Reference

Factual background from Commodore's own documentation and schematics, gathered
(2026-09-11) to check whether the C64 and C128 expansion ports impose
different `/DMA` assert/de-assert timing requirements — directly relevant to
[DMA-Timing-Known-Issues.md](DMA-Timing-Known-Issues.md) item #6 (the
"C128 PHI2-generation-delay" theory) and to `nS_DMAAssert`'s existing
C128-specific tuning bumps (item #9 in the same file). This file is reference
material, not a bug tracker — nothing here is a TeensyROM firmware finding,
it's what Commodore itself published.

## Bottom line

**No documented difference in the assert/de-assert timing rule itself.** The
C64 and C128 Programmer's Reference Guides state the identical rule, in
near-identical wording: assert `/DMA` only while Φ2 is low; asserting during
Φ2-high risks a "potentially fatal DMA" (bus tri-states but the CPU doesn't
actually halt for up to 3 cycles if it's mid-write-cycle, corrupting memory on
release). Neither manual publishes a separate numeric setup/hold spec for one
machine's port vs. the other's.

What *is* documented is architectural, not a timing-window change: the C128
routes `/DMA` through the MMU rather than straight to the CPU, adding a
verified-real (but unquantified) extra logic stage between the port pin and a
settled bus. That's a plausible source of the C128-specific margin `nS_DMAAssert`
and friends already carry (per item #9), but it is **not** the same claim as
item #6's "C128 has less PHI2-generation delay than C64" theory — that one
remains untested by any source found here; it would have to be a real,
undocumented difference in the two VIC chips' PHI2 output stage.

## What the official docs actually say

Primary source: *Commodore 128 Programmer's Reference Guide* (1986, Bantam
Books), Chapter 16 hardware appendix, "DMA Capability" section (p.636-637) —
[full OCR text](https://archive.org/stream/C128_Programmers_Reference_Guide_1986_Bamtam_Books/C128_Programmers_Reference_Guide_1986_Bamtam_Books_djvu.txt),
cross-checked against the [C64 Programmer's Reference Guide, Ch. 6](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_6/page_368.html).

**C64 (both PRGs agree this part is baseline behavior):** `/DMA` low drives
the 6510's RDY and AEC pins directly. Only assert while Φ2 is low — RDY is
ignored during a write cycle until the next read cycle, so asserting on
Φ2-high can tri-state the bus up to 3 cycles before the CPU actually stops.
Any DMA source must still yield to VIC-II every cycle it wants (VIC always
has top priority, even mid-DMA).

**C128 ("a similar DMA scheme," same source):** `/DMA` low simultaneously
drives 8502 RDY, 8502 AEC, **and** Z80 `/BUSRQST`, and additionally drops a
gated-AEC signal (`GAEC`) that forces the MMU into VIC-cycle mode — reversing
the TA (translated address) and SA (shared address) bus directions so the
external source can drive the full processor address bus, and tri-stating the
Z80's data-out buffer. Two consequences with no C64 equivalent:
- The MMU itself and the 8563 VDC become **unreachable** during DMA — C128
  memory-map/banking state must be set up *before* asserting, not after.
- The MMU's 2MHz clock output is gated off automatically whenever "VIC or
  external DMA is taking place" — DMA forces 1MHz regardless of the C128's
  FAST/SLOW software setting. C64 has no dual-speed mode, so this class of
  interaction doesn't exist there at all.

Numeric electrical specs exist only for the C128 side (Table 16-5, 8502
processor timing chart, characterized at the 2MHz-mode 489ns cycle time):
Taec (AEC setup) 25-60ns, Taads (address setup from AEC) 60ns, Taedt
(data→tri-state from AEC) 120ns max, Taeat (address→tri-state from AEC) 120ns
max, Trdy (RDY setup) 80ns. The C64 PRG publishes no equivalent table — only
the qualitative Φ2-phase rule above. Since the 8502 is 6510-pin/timing-
compatible in 1MHz mode, these numbers are the closest thing to a C64 spec
that exists in print, but they were never asserted to be identical.

## REU R4 resistor — correction, not a DMA signal

Old field reports (Commodore engineer Fred Bowen, via a 1990s upgrade
article) describe a 390Ω resistor (`R4`) present on C128-targeted REU boards
(1700/1750) and absent on the C64-targeted 1764, said only to "tweak the
signal supplied to the C-128." This is easy to misread as a `/DMA`-line
difference given the REU is a DMA device. **It isn't.** Traced directly off
Commodore's own factory schematic (`#311752 Rev. 4`, from the official
1750/1764 service manual, [HQ scan](https://rr.pokefinder.org/rrwiki/images/7/7f/1750_1764_Service_Schematics_HQ.pdf)):
R4 runs from VCC to pin 3 of the 8726 RAM Expansion Controller — the
`8.18MHZ` **DOTCLK** (video dot clock) input, not `/DMA`, `/GAME`, `/EXROM`,
or anything in the bus-arbitration path above. It's a pull-up on a clock line
TeensyROM doesn't use either way. Confirmed by wire-tracing the scanned
schematic (bend lands exactly on the `8.18MHZ` row, not the `/DMA`/`/ROMH`/
`/ROML`/`/ROMSEL` group above and below it) — cross-validated against the
build note on the later quad-pack revision (`#312534 Rev. 3`): *"Populate R4
only for 1700 or 1750. Do not populate R4 for 1764."* Same part, same
instruction, different board layout.

## Sources

- [Commodore 128 Programmer's Reference Guide, 1986 Bantam Books (full OCR text)](https://archive.org/stream/C128_Programmers_Reference_Guide_1986_Bamtam_Books/C128_Programmers_Reference_Guide_1986_Bamtam_Books_djvu.txt) — "DMA Capability" (p.636-637), MMU/VIC signal descriptions, Table 16-5 (8502 timing chart), Figure 16-4 (2MHz clock stretching)
- [C64 Programmer's Reference Guide, Ch. 6 — The Expansion Port](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_6/page_368.html)
- [Commodore REU — C64-Wiki](https://www.c64-wiki.com/wiki/Commodore_REU) (Fred Bowen / R4 anecdote, pre-correction)
- [1750/1764 Service Schematics, HQ scan — ReplayResources](https://rr.pokefinder.org/rrwiki/images/7/7f/1750_1764_Service_Schematics_HQ.pdf) — official Commodore schematics `#311752 Rev.4` and `#312534 Rev.3`
- [C128 Expansion Bus pinout — pinouts.ru](https://old.pinouts.ru/Motherboard/ExpansionC128_pinout.shtml)

<br>

[Back to Architecture Overview](Overview.md)
