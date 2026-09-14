# MHS Power Engine (MPE) VM Usage

## Table of contents
  * [What it is](#what-it-is)
  * [Requirements](#requirements)
  * [Downloading and installing a VM](#downloading-and-installing-a-vm)
  * [Launching a VM](#launching-a-vm)
  * [Returning to the TeensyROM menu](#returning-to-the-teensyrom-menu)
  * [How it works](#how-it-works)
  * [Building the MPE firmware](#building-the-mpe-firmware)

## What it is

The MHS Power Engine (MPE) lets downloadable VM engines — like DoomVM — run directly on TeensyROM+'s own ARM processor, with the C64 handling display, SID sound, and user input. It's an add-on to the stock TeensyROM interface: your normal menu, settings, and ordinary cartridge support are unchanged.

Created by [ziggystar12](https://github.com/ziggystar12) at [Mean Hamster Software](https://meanhamster.com/). See the [MHS Power Engine project](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine) for the latest available VMs and more information.

## Requirements

  * **TeensyROM+** (PCB v0.4) using its full bus-mastering DMA hardware. 
    * Original TeensyROM (PCB v0.2/v0.3) not supported for this feature.
  * **TR+ firmware with MPE enabled**. This update pairs TeensyROM **0.8.0.8** with MPE host **1.2.23**, supporting the current **NESVM 1.2.0** and **DoomVM 1.2.1** packages. The TeensyROM version alone does not identify the included MPE host.

## Downloading and installing a VM

  * The matching NESVM 1.2.0 and DoomVM 1.2.1 packages are available in the [MHS Power Engine 1.2.23-r2 release](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/releases/tag/v1.2.23-r2). See [VM downloads](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/vms) for later packages. The GUI firmware offered there is a separate firmware choice; this repository builds Travis's text interface.
    * Installing a new VM typically doesn't require a firmware update — though a VM engine update may occasionally need a [newer firmware version](../bin/TeensyROM/) to use new capabilities.
  * Extract a VM's complete download onto the SD card root, keeping its launcher file and `VMS/` directory together.
  * Check the specific VM package's own instructions for controls, optional music, and any other setup notes.
  * For emulator-style VMs (e.g. NES, Game Boy, Game Gear), supply your own ROM files, apart from any demo explicitly distributed with its package. NESVM includes the authorized Crossbow demo. Place additional ROMs in the `ROMS` subfolder inside that VM's own `VMS/` directory (e.g. `VMS/NESVM/ROMS/`), or select them directly from the SD browser as described below.

## Launching a VM

  * Select the VM's launcher file (e.g. `DOOMVM.crt`) through the normal menu browser, the same way you'd launch any other cartridge file.
  * A Hot Key or Auto-Launch can also be set to a VM's launcher file for one-press or automatic launching, same as any other file.
  * Select a ROM directly from the SD browser to route `.nes` to NESVM, `.gb` / `.gbc` / `.gc` to GBVM, or `.gg` to GGVM. The corresponding compatible VM package must be installed under `/VMS`. Missing, corrupt, ambiguous, or unsupported packages produce an error instead of treating a known console ROM as a PRG. These extension routes do not imply that every console package is included in this release.
  * The GUI firmware's self-contained MGC1 `.MPE` launch flow is not enabled in this text-interface integration. Use the installed VM package and its launcher or supported direct ROM route.

## Returning to the TeensyROM menu

  * Press Reset or the Menu button to exit the VM and return to the normal TeensyROM interface.

## How it works

  * A VM owns its execution memory exclusively while it's running. Normal TeensyROM services (USB, Ethernet, ordinary cartridge support) resume automatically once you return to the menu.
  * VM engines and their C64-side clients are distributed separately from the firmware — adding a new VM just means copying files to the SD card, with no firmware flashing involved. That makes it easier and faster than a firmware update, since nothing about the running firmware changes.
  * The independent MPE image links the supplied complete host library, which includes the broader MPE runtime and Prism/Prism+ display services. Travis's text menu and ordinary cartridge image are built from this repository's public source. NESVM 1.2.0 can use Prism+; DoomVM 1.2.1 uses its own display modes. See the [library documentation](../mpe/library/README.md) and [license notices](../THIRD-PARTY-NOTICES.md) for the source and relinking boundaries.

## Building the MPE firmware

Use `npm run build:mpe` for the three-image MPE build. For example, with your installed Arduino paths:

```powershell
npm run build:mpe -- --arduino-cli "C:/Tools/arduino-cli.exe" --arduino-data "C:/Arduino15" --arduino-user "C:/Arduino" --out "C:/MPE-build"
```

The first `--` passes the remaining arguments through npm to the MPE builder. Node.js 24 or later and Teensyduino core 1.61.0 are required. Keep the output path short on Windows. The existing `build`, `build:tr`, and `build:tr-plus` commands continue to use the separate two-image builder. See [MPE 1.2.23 migration and verification](MPE-Update-1.2.23.md) for output identity, compatibility, and checks.
