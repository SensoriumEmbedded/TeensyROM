# MHS Power Engine for TeensyROM+

This review adds the MHS Power Engine host to TeensyROM **0.8.0.5**, using the
same shared services as MPE firmware **1.2.6**. Travis's text menu, settings,
bundled applications and ordinary cartridge configuration are retained. The
Custom GUI desktop and VM engines are not compiled into this firmware.

MHS created the Power Engine system and MPE Cartridge VM format so downloadable
engines can run on the Teensy's ARM processor, using shared video, input, sound
and file services. Travis Smith / Sensorium Embedded created the TeensyROM
hardware and original firmware. [More from Mean Hamster Software](https://MeanHamster.com).

**MPE requires TeensyROM+ PCB v0.4 and its full bus-mastering DMA hardware.**
Original TeensyROM PCB v0.2/v0.3 remains supported for ordinary firmware, not
for these VMs. No PSRAM is required.

## Install and try

1. Download the full HEX linked in the [current test notes](MPE-RETEST-1.2.6.md).
2. Install it through the ordinary TeensyROM+ firmware updater.
3. Extract a complete [VM download](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/vms)
   onto the SD root, keeping its launcher and `VMS/` directory together.
4. Select the launcher's CRT through the normal text browser.

For DoomVM, open `DOOMVM.crt`. Its full ZIP includes the current shareware game
data; follow the package's instructions for controls and optional music.
Reset/menu-button exit returns to the text interface and skips autolaunch once.
The packages contain no firmware: use this separate candidate HEX when testing
the text-interface integration.

Startup and ordinary button handling match upstream. There is **no two-button
SD firmware-recovery shortcut**. Normal SD/USB firmware updating and the PJRC
USB hardware-loader route remain available. Do not interrupt power during an
update. The normal updater retains its target checks and stricter Intel HEX
validation.

## Shared services

- MHS palette-cached F1 colour fitting, with optional solid-colour status rows.
  Other negotiated modes, including F7 Sharp, retain their existing conversion.
- Full-width 320x200 NUFLIX double buffering, dirty updates and bounded PAL/NTSC
  transfer grants. SID packets can be serviced while a picture uploads.
- NUFLIX-capable clients can also use ordinary F1/F3/F7 with their existing
  timing handshake. Invalid timing values still fail before DMA.
- Immutable source pixels, palettes, raster contexts and dirty maps may reside
  in the lower 416 KiB RAM2 arena. Configuration and video workspace stay in RAM1.
- ABI-2 module loading, files, input, sound, packet replay and opt-in auxiliary
  RAM ownership. Ordinary cartridge bank swapping remains separate.

VM engines and C64 clients are separate packages. A VM owns its execution memory
exclusively; normal USB, Ethernet and cartridge services resume on returning
to ordinary firmware. This is not a sandbox for untrusted native modules.

## Build on Windows

Use Node.js, Arduino CLI, Teensy core **1.61.0** and the libraries listed in
[BuildInfo](../Source/BuildInfo.md), including CRC32 2.0.0. From the repository root:

```powershell
.\mpe\Build.ps1 -Output C:\MPE-build
```

For a nonstandard installation, supply `-ArduinoCli`, `-ArduinoData` and
`-ArduinoUser`. Keep the output path short for the Windows ARM toolchain.
The builder copies source and the core into an isolated directory. It does
not modify the installed SDK, regenerate the C64 menu, or flash hardware.

The result is `TeensyROM+_0.8.0.5_MPE-1.2.6_full.hex`. The visible stock version
is 0.8.0.5; the filename identifies the MPE candidate. `latest.json` records
its source inputs, image layout and hash.

For ordinary-firmware comparison builds, with MPE disabled:

```powershell
.\mpe\Build.ps1 -Mode stock-plus -Output C:\MPE-stock-plus
.\mpe\Build.ps1 -Mode stock -Output C:\MPE-stock
```

The original-TR comparison checks normal firmware only; it does not establish
VM support on original hardware.

## Integration layout

Ordinary MinimalBoot keeps its upstream cartridge buffers, swap cache and
network/USB configuration. VM launch uses a one-shot request to a separately
linked host image. Users still install one full HEX and use the same menu.

| Image | Flash interval |
| --- | --- |
| Ordinary MinimalBoot | `0x60000000..0x6005ffff` |
| Normal application | `0x60060000..0x6027ffff` |
| MPE host | `0x60280000..0x602dffff` |

The linker and HEX combiner reject overlaps, preserve the top 256 KiB reserved
for the loader/EEPROM, and check staging space for a full firmware update.
VM modules load into RAM, not firmware flash.

The branch includes upstream `dc1174c` and retains Travis's FLASHMEM and
root-file hot-key launch fixes. Shared host source is imported from committed
MPE revision `59b86dff016ded4cdab27e5da5a989a4cb2eddca`; the complete imported
files are present here. [source-lock.json](../mpe/source-lock.json) records the
source pin and the small dedicated-boot adaptations. See the
[ABI guide](../vm/abi/README.md) and [component notices](MPE-FIRMWARE-NOTICES.md).

## Verify

Extract the full DoomVM ZIP to a test SD-root directory, then run:

```powershell
node mpe/tools/verify.mjs --build C:/MPE-build/latest.json --packages C:/DoomVM-SD --cxx C:/msys64/mingw64/bin/g++.exe
```

Use a C++17 host compiler. Indexed-DMA tests use Windows memory mapping.
The suite checks startup, flash parsing, module/file services, replay, video,
RAM ownership, launch routing, firmware layout and protected upstream files.
`--all-packages` optionally checks locally available development packages;
those payloads are not included in this review.

Build and host checks are not physical acceptance. Before merging or releasing,
test PAL/NTSC VM graphics/audio/input, reset/menu return, normal and large CRTs,
USB/Ethernet/MIDI, REU/freezer/KERNAL, settings retention and firmware updating
against the ordinary upstream build on the same hardware.
