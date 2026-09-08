# MHS Power Engine VM support for TeensyROM

Maintainer review candidate for TeensyROM+ PCB v0.4 / Teensy 4.1. The firmware
keeps Travis's C64 menu, settings, bundled applications and ordinary cartridge
configuration, and adds the generic MHS Power Engine module loader. No Custom
GUI or emulator engine is compiled into this firmware.

**TR+ hardware is required for the current MPE VM implementation, including
DoomVM. It is not just a performance upgrade.** The host uses TR+'s full
bus-mastering DMA to write display data directly into C64 memory. Original TR
PCB v0.2/v0.3 can pause the CPU through `/DMA` but lacks the bus-control hardware
used by these transfers. There is no supported original-TR VM mode or non-DMA
display fallback in this release. Retaining Travis's original interface means
using that interface on TR+ hardware.

MHS created the MHS Power Engine system and MPE Cartridge VM format for the
TeensyROM+ cartridge. Its host, module loader, shared services and C64 transport
let downloadable VM engines execute directly on the Teensy's ARM processor,
with display, SID sound and input handled by the C64. This integration brings
that MHS platform to Travis's original interface. Travis Smith / Sensorium
Embedded retains credit for TeensyROM hardware and the original firmware;
individual VM engines retain their authors' credits and licences.

**DoomVM is the only VM selected for public shipment.** Other VMs remain under
development and are withheld pending testing. The host remains generic so
future compatible modules can use the same platform without engine-specific
firmware additions.

## DoomVM download and installation

The public home for DoomVM and the shared platform is
[MHS-Teensy-Rom-Power-Engine](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine).
The complete [DoomVM download](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/raw/refs/heads/main/vms/DOOMVM.zip)
is public. Its manifest, module and client match the files verified for this
candidate. The separate [Custom GUI firmware](https://github.com/ziggystar12/Teensy-Rom-Custom-GUI)
supports the same VM contract for users who prefer that interface.

Install this candidate's full HEX through the existing TeensyROM updater for
PCB v0.4, then place DoomVM's launcher and `VMS/DOOMVM/` directory on SD. Select
`DOOMVM.crt` through Travis's normal browser. The package supplies its own C64
client, engine and presentation; follow its instructions for game data. The
current Doom release follows E1M1, E1M4, E1M5 and E1M8; saving is not supported.
Physical testing of that package with this firmware remains pending.
A registered `.gbd` file on SD can also launch the installed DoomVM directly.
Reset/menu-button exit returns to Travis's interface and skips autolaunch once.

The firmware requires no PSRAM for VM execution. VM launches use SD; USB,
Ethernet and ordinary cartridge features remain available in normal firmware
modes. A VM takes exclusive ownership of its execution memory and does not run
those background services concurrently. VM graphics, input and sound use the
package's C64 client and shared host transport.

## Build on Windows

Prerequisites: a Git checkout, Node.js, Arduino CLI, Teensy core **1.61.0**, and
the libraries in
[upstream BuildInfo](../Source/BuildInfo.md), including CRC32 2.0.0. The code was
built with Arduino CLI 1.4.1 / GCC 11.3.1. Keep core 1.61.0 as upstream currently
recommends. No C64 assembly build is necessary; upstream's checked-in menu,
help and settings assets are retained.

From the repository root:

```powershell
.\mpe\Build.ps1 -Output C:\MPE-build
```

For a nonstandard Arduino installation, add `-ArduinoCli` for the executable,
`-ArduinoData` for the Arduino15 package directory, and `-ArduinoUser` for the
sketchbook directory containing additional libraries. The wrapper can locate
Arduino CLI in common Arduino IDE installations. Node.js must be on PATH.

The builder copies source and the core into a private build directory, selects
the v0.4 features, builds the firmware images and combines them into one HEX.
It does not modify the installed core, regenerate menu assets, flash hardware
or publish anything. Use a short output directory because the Windows ARM
toolchain still encounters path-length limits. Each run has its own directory;
`latest.json` identifies its HEX, hashes, source inputs and memory layout.

The output is named `TeensyROM+_0.8.0.4_MPE-review2_full.hex` for this upstream
revision. The visible stock firmware version remains unchanged; identify this
review build by its filename and SHA-256. The original stock build script is
unchanged. For comparison builds with the isolated builder:

Both comparison modes below disable MPE. `stock-plus` builds ordinary TR+
firmware; `stock` builds ordinary original-TR firmware. A passing `stock` build
checks preservation of the original firmware, not VM support on original TR.

```powershell
.\mpe\Build.ps1 -Mode stock-plus -Output C:\MPE-stock-plus
.\mpe\Build.ps1 -Mode stock -Output C:\MPE-stock
```

## Why the review has a separate VM image

Large cartridges and VMs already coexist in the Custom GUI firmware. The user
reports successful 1 MB and 2 MB CRT boots. The integration preserves stock
bank swapping; it does not assume a cartridge must fit entirely in RAM.

The unmodified upstream v0.4 build measured 136,808 bytes of MinimalBoot code
and a five-bank ITCM layout. The VM ABI reserves only 98,304 bytes for host
code and uses a six-bank layout. Upstream also keeps 16 swap buffers and
network/USB support, while the GUI's VM-oriented MinimalBoot uses a smaller
configuration. Copying that configuration would change upstream behavior.

This review therefore retains the ordinary MinimalBoot and normal application,
with a narrow one-shot routing hook into a separate VM image. The same existing
core-startup handoff initializes the VM's memory profile. Users still receive
one full HEX and use the same menu. Image addresses are:

| Image | Flash interval | Memory behavior |
| --- | --- | --- |
| Ordinary MinimalBoot | `0x60000000..0x6005ffff` | Upstream CRT buffers, swap cache, USB and networking configuration. |
| Normal application | `0x60060000..0x6027ffff` | Upstream menu and normal services, plus VM launch validation. |
| MPE VM host | `0x60280000..0x602dffff` | ABI 2: fixed RAM1 host/module windows and exclusive RAM2 guest memory. |

The linker and HEX combiner reject overlaps and verify there is enough unused
flash for the stock updater to stage another full image. The final 256 KiB
loader/EEPROM reservation is preserved. VM packages load into RAM; launching
or updating a module never programs firmware flash.

Actual reset/handoff and firmware update behavior still require physical
testing. Build success and memory bounds do not establish that acceptance.

## Review scope and source provenance

Base: upstream `442aaaa266f3306ba30dd925235939ee3878db77` (2026-08-31).
Shared MPE code: Custom GUI revision
`69a711a42fd8e07d7857872073c9d2c7dee5bcc9`, ABI 2, shared host version 1.1.12.
This matches the host source used by the public GUI firmware 1.1.12. The Doom
module and launcher are unchanged; uncommitted development experiments are excluded.
See [source-lock.json](../mpe/source-lock.json) for the imported file hashes.
The imported VM loader, ABI, file services, packet replay and video code retain
their source provenance. The dedicated boot sketch adapts request consumption
and return behavior to the stock image router. The VM module binaries and
emulator source are distributed separately through the Power Engine repository.

Six existing upstream files receive guarded hooks:

- `DriveDirLoad.ino`: recognize a VM descriptor or registered SD extension before
  ordinary file loading. Protected stock file types bypass the registry.
- `MinimalBoot.ino` and `Min_RunTeensyApp.ino`: consume the one-shot VM request,
  validate the target image and enter it; invalid images recover to the menu.
- `Common_Defs.h`: reject an MPE-enabled build for unsupported board revisions.
- `ISRs.c` and `IOH_EasyFlash.c`: compile VM-only DMA/IO2/poll hooks into the
  dedicated host. Those hooks are absent from the ordinary firmware images.

No C64 menu code, menu assets, EEPROM map, stock MinimalBoot configuration,
cartridge loader or Magic Desk 2 implementation is replaced. No historical
monolithic emulator patches or Custom GUI desktop services are included.

## Verification

After extracting DoomVM to a test SD-root directory, run:

```powershell
node mpe/tools/verify.mjs --build C:/MPE-build/latest.json --packages C:/DoomVM-SD --cxx C:/msys64/mingw64/bin/g++.exe
```

Use a C++17 host compiler; the indexed-DMA test currently uses Windows memory
mapping, so that test runs on Windows. Verification exercises the real shared
registry, image validation, file operations, ACK/replay, video conversion and
DMA failure handling. It checks DoomVM's actual manifest/module/client, direct
launch routing, malformed and missing packages, stock-file fallthrough,
relocated boot headers, memory boundaries and unchanged upstream assets. Format
fixtures are synthetic; they contain no withheld emulator or game payloads.

For maintainers with the unreleased development packages, `--all-packages`
adds their compatibility checks. Passing those checks is not release approval.
No other VM downloads or payloads are part of this review.

The verification report explicitly distinguishes host/image checks from physical
hardware acceptance. Before merge/release, test Doom gameplay and level transitions, PAL/NTSC transport,
input, sound, reset/menu return, missing SD and interrupted launch recovery,
normal/large CRTs including active bank swapping, stock networking/USB/MIDI,
REU/freezer/KERNAL functionality, settings retention and the firmware updater.
Compare with the unmodified build on the same hardware. See the accompanying
[current retest results](MPE-RETEST-2026-09-06.md) for measured evidence and
remaining checks. The [initial review1 results](MPE-REVIEW-RESULTS.md) are retained
as historical baseline evidence.
