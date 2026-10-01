# MPE host and packaged runtimes

The [October 1 installer bundle](../../bin/MPE/README.md) supplies MPE.TRH,
the stock registration files and a synthetic diagnostic. The independently
installed host lets stock TeensyROM's text menu launch packaged HamsterOS and
MGC1 .MPE games/VMs. Each package contains its ARM engine, C64 receiver and
content. The full host supplies loading, renderer callbacks, desktop/native APP
services, files, packets, input, bounded transfer, PCM8 speech and return.

## Installation

Copy `bin/MPE/MPE.TRH` and `bin/MPE/VMS` to the SD root, preserving paths.
`MPE-DEMO.MPE` is optional for the diagnostic. Select MPE.TRH in the stock SD
browser and confirm installation. Fully power the C64 off and on afterward.
F8, 0 should identify MHS MPE, ABI 2, services $20.

There is one host slot. Only MPE.TRH needs installing for MPE use; installing
VMBoot.TRH afterward replaces it. VMBoot remains useful separately for generic
stock VM demos and GRANTS diagnostics. Installing the TRH without its three
`VMS/MPE` files does not register .MPE. The manifest is:

```
VM1
MPE
mpe
engine.mvm
client.crt
END
```

Launch your separately supplied `Sys/HAMSTEROS.MPE` to use HamsterOS, or select
a game directly from stock. Do not install the private `/VMS/HAMSTEROS`
registration on stock. MPE packages run from SD; desktop files and native APP
content can also use USB. An .MPE entry can still show the stock Unk label;
registered launch routing precedes the ordinary unknown-file fallback.

A game launched from HamsterOS returns there after pressing and releasing the
cartridge Menu/reset button. Holding Menu for two seconds returns to stock.
Direct stock-menu games and native PRG/ordinary CRT/disk-image launches return
to stock. IEC native launches retain the host and can return to HamsterOS.
Firmware flashing from HamsterOS is unavailable in this extension setup.

## ABI and memory

The public descriptor uses existing out-of-tree discriminator bit 5 (32, $20),
ABI 2 and the 76-byte shared prefix. Private MPE callbacks stay inside the host;
they are separate from the stock public DMA and EXIT interfaces. The host checks
the authorized stock launch record, manifest, selected MGC1 container and CRCs,
then loads that package's embedded engine and receiver. Renaming MPE to CRT is
incorrect; the registration module/client do not replace a package's runtime.

The host reserves 64 KiB of ITCM and supports private package code at
`0x10000..0x30000`. Its published code floor is 32,244 bytes. It preserves the
192 KiB module data region at `0x20014000..0x20044000`, the guest RAM arena,
16 KiB host heap and 48 KiB stack. The 315 KiB image fits the 384 KiB extension
slot. The top 256 bytes of RAM2 remain available for failure/crash records,
including the private 96 KiB constants profile.

The MPE transport validates bounds, mailbox exclusions and PAL/NTSC budgets,
holds payloads until completion, gives border transfers priority over storage,
retains finite deadlines and releases the bus on failure. This does not make
private MPE callbacks interchangeable with stock c64_write/c64_status.

## Qualification, October 1, 2026

The accepted installer SHA-256 is
`3883fab8ad17461d677ab40331cbcc7b8dc8f83e152d38c415952c507b864f73`.
It was built and tested with stock main
`018641a1fce70eba94c49c940ae677758cde0288`, after PR #45 merged. This PR branch
has since been refreshed onto main `82dc4d5`, which also merged #48; the installer
bytes remain the accepted build. No #48 fix is proposed.

Software qualification covers stock preflight/registration, MGC1 corruption and
read failures, desktop control and native APP lifecycle, SD/USB/IEC services,
Ethernet lifecycle, PCM8/Prism state machines, the emitted C64 receiver, all 14
pinned engines' admission and simulated PAL/NTSC Monkey transport/input/saves.
The refreshed PR branch passes the Node test suite with seven platform/tool
skips, and native extension-loader conformance.

After receiving the hash-verified TEENSY2 setup and HamsterOS/Doom/Menu-return
instructions, the owner reported "its all working" on the previously identified
NTSC C64 with SIDKick Pico. [HARDWARE-TEST.json](../../bin/MPE/HARDWARE-TEST.json)
preserves that report and the earlier September 30 Monkey-only observations.
This accepts the delivered setup, not complete-game, PAL hardware, C128 or
separately enumerated optional USB/Ethernet checks. The installed board slot was
not read back. [VERIFICATION.json](../../bin/MPE/VERIFICATION.json) binds the
software evidence and hardware report to the installer hash.

The bundle contains no games, ROMs, saves, private renderer implementation or
stock firmware image. Component copyright/license notices are retained under
`bin/MPE/Notices`.
