# Text-interface MPE 1.2.6 review

[Download the full firmware](../mpe/review/TeensyROM+_0.8.0.5_MPE-1.2.6_full.hex)
for **TeensyROM+ PCB v0.4**. This is a candidate for Travis's consideration,
not an accepted upstream release. His text menu is retained.

The branch includes TeensyROM 0.8.0.5 with the latest upstream changes,
MHS F1 colour fitting, NUFLIX double buffering and SID servicing during
uploads. Startup and button handling match upstream exactly. The two-button
firmware-recovery option is gone; normal SD/USB firmware updating remains.
NUFLIX-capable clients can switch to F1/F3/F7 without restarting; timing tests
cover all 256 possible handshake values and reject unsupported ones before DMA.

## Checks

- MPE, stock-plus and original-TR comparison builds pass with Teensy core
  1.61.0 and GCC 11.3.1.
- All 219 protected upstream menu, asset and configuration files are unchanged.
- Startup, 25 flash-parser safety cases, module/file services, packet replay,
  colour conversion, RAM2 bounds, auxiliary RAM and launch routing pass.
- PAL/NTSC tests check inactive-bank uploads, SID scheduling, DMA failure
  handling and preservation of the visible picture.
- The VM host uses 91,324 of its 98,304-byte code allowance, with no RAM2 or
  PSRAM globals. Its 16 KiB heap and 48 KiB execution stack remain reserved.
- The normal application retains seven code banks and a 25,920-byte linked
  static-to-stack gap. The full image fits the updater's staging space.

[Build and installation](MPE-VM.md), [test output](../mpe/review/host-1.2.6-tests.log)
and [source/memory record](../mpe/review/host-1.2.6-verification.json).

No hardware was flashed. Physical acceptance is still needed for VM graphics,
input/audio, reset/menu return, normal and large cartridges, Final Cartridge
III/freezer operation, USB/Ethernet/MIDI, REU/KERNAL, settings and firmware
updating. Host tests simulate the C64 bus and do not establish those results.
