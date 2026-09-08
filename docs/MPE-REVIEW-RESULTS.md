# MPE upstream integration: initial review1 results

Historical baseline record. The current build uses the 1.1.12 shared host and
the newer four-level Doom package; see [review2 retest results](MPE-RETEST-2026-09-06.md).
The firmware and package hashes below identify the earlier review1 pairing.

Date: 2026-09-05. Status: **build and host checks pass; physical acceptance pending**.
DoomVM is the only VM selected for public shipment. This branch contains generic
host source, build tools and tests; no VM engines or game media are included.

Upstream base: `442aaaa266f3306ba30dd925235939ee3878db77`.
Imported host base: `1dde1563ba2cea31761d04300b6d962ae2ec5ca2`.
Compiled firmware source: `0bbd9a0be3ebb953922b04895133280d4268265a`.
Machine-readable measurements and DoomVM file hashes are in
[evidence.json](../mpe/review/evidence.json).

## Builds

Three configurations compiled with Arduino CLI 1.4.1, core 1.61.0, GCC 11.3.1,
600 MHz build setting / upstream's 816 MHz runtime clock:

| Build | Hardware | MPE VM support |
| --- | --- | --- |
| `stock-plus` baseline | TR+ PCB v0.4 | Disabled; ordinary firmware comparison. |
| `stock` baseline | Original TR PCB v0.2/v0.3 | Disabled; ordinary firmware regression check only. |
| `mpe` candidate | TR+ PCB v0.4 | Enabled; requires full bus-mastering DMA. |

**The successful original-TR build does not establish VM compatibility. Current
MPE VMs, including DoomVM, require TR+.** All builds retain stock C64 assets.
No hardware was flashed.

Candidate: `TeensyROM+_0.8.0.4_MPE-review1_full.hex`

SHA-256: `b45e8e477170406d02b2bd21ce039ce687d7fd28a4587ec81a5d81a80b914745`

The combined image spans 2,745,344 bytes of flash. The stock updater has
5,378,048 bytes available above the candidate for staging another full image.
All three images fit their declared regions and preserve the top 256 KiB
loader/EEPROM reservation. These are address/layout checks, not physical updater
or interrupted-write tests.

## Memory comparison

| Measurement | Stock TR+ | MPE candidate |
| --- | ---: | ---: |
| Ordinary MinimalBoot ITCM code | 136,808 B | 136,888 B |
| Ordinary MinimalBoot ITCM allocation | 5 banks | 5 banks |
| Ordinary MinimalBoot DTCM end-to-stack gap | 14,560 B | 14,560 B |
| Ordinary MinimalBoot RAM2 heap span | 511,840 B | 511,840 B |
| Ordinary MinimalBoot swap buffers | 16 | 16 |
| Normal application ITCM code | 220,440 B | 223,608 B |
| Normal application ITCM allocation | 7 banks | 7 banks |
| Normal application DTCM end-to-stack gap | 30,656 B | 26,560 B |
| Normal application RAM2 heap span | 506,688 B | 506,688 B |

The DTCM gaps above come from ELF symbols (`_estack - _ebss`), including the
actual section placement. They are static bounds, not measured runtime stack
high-water values. The core's summary omits an orphan non-cacheable section,
so its displayed free-variable figures should not replace these measurements.

The dedicated VM host uses 90,792 B of its 98,304 B ITCM ceiling, a 16 KiB heap
below module data, 192 KiB reserved for module data/support and a separate
48 KiB stack. RAM2 has no host static allocations. DoomVM's constant profile
supplies 416 KiB guest memory plus the upper 96 KiB read-only constant region.
The raw gap between VM host BSS and its stack includes module reservations and
must not be counted as free memory.

## Checks completed

- All 219 checked upstream C64 source/assets, menu files, the original
  MinimalBoot configuration, CRT loader and Magic Desk 2 source match the pinned
  upstream Git blobs. The ordinary images contain no VM runtime symbols.
- Actual DoomVM manifest, module and client pass host preflight and matching
  descriptor/CRC checks. Launcher and `.gbd` selection produce the expected
  one-shot launch records. These checks do not execute Doom gameplay.
- Malformed headers, unsupported services, truncation, overflow, corrupt
  payloads, missing packages, duplicate associations and invalid paths reject.
- Protected stock types and non-SD launches retain stock routing. Synthetic
  1 MiB and 2 MiB ordinary CRT files fall through to the stock cartridge loader.
  This checks routing, not real cartridge execution or bank-swap performance.
- Generic read/write/flush/truncate/rename/list/space operations, 24 file handles,
  invalid spans and injected write/flush failures pass.
- Packet replay retains immutable output, preserves ACK ordering, handles busy
  DMA and releases the bus on injected DMA failures.
- Indexed/raster conversion, stable F5 plans, PAL/NTSC receiver kernels, range
  checks, generation handling, sliced uploads and picker reinitialization pass
  host tests. Host conversion timings are not Teensy timings.
- Relocated boot image headers, Thumb entry bounds, VM linker reservations,
  no-PSRAM layout and HEX checksum/overlap/updater-space checks pass.

## Physical review still required

Test cold/warm boot into the unchanged stock interface; enter DoomVM; exercise
E1M1 gameplay, graphics, input and sound; reset/menu-button back to the stock
menu with autolaunch enabled. Cover PAL/NTSC, missing SD, corrupt packages and
reset during launch. Test firmware installation, subsequent update and return
to stock firmware with settings retained.

Compare ordinary and large CRT execution/bank swapping, USB and remote
networking, MIDI/ASID, NFC/hotkeys, REU, KERNAL/freezer/alternate-button behavior,
RTC and C128 stock operation with the baseline on the same hardware. Record
stack high-water and bus timing where appropriate. The user's successful
large-CRT tests in the Custom GUI are useful prior evidence; they are not
physical acceptance of this new stock-interface candidate.

The DoomVM download home is
[MHS-Teensy-Rom-Power-Engine](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine).
The repository became public during preparation. Its complete `vms/DOOMVM.zip`
was downloaded and its manifest, engine and C64 client hashes match the tested
files recorded in `evidence.json`. The current Doom release supports E1M1;
saving and later levels are not supported. Other VMs remain withheld.
