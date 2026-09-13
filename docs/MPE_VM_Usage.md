# MHS Power Engine (MPE) VM Usage

## Table of contents
  * [What it is](#what-it-is)
  * [Requirements](#requirements)
  * [Downloading and installing a VM](#downloading-and-installing-a-vm)
  * [Launching a VM](#launching-a-vm)
  * [Returning to the TeensyROM menu](#returning-to-the-teensyrom-menu)
  * [How it works](#how-it-works)

## What it is

The MHS Power Engine (MPE) lets downloadable VM engines — like DoomVM — run directly on TeensyROM+'s own ARM processor, with the C64 handling display, SID sound, and user input. It's an add-on to the stock TeensyROM interface: your normal menu, settings, and ordinary cartridge support are unchanged.

Created by [ziggystar12](https://github.com/ziggystar12) at [Mean Hamster Software](https://meanhamster.com/). See the [MHS Power Engine project](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine) for the latest available VMs and more information.

## Requirements

  * **TeensyROM+** (PCB v0.4) using its full bus-mastering DMA hardware. 
    * Original TeensyROM (PCB v0.2/v0.3) not supported for this feature.
  * **TR+ Firmware v0.9 or higher**.

## Downloading and installing a VM

  * VM packages are available at the [MHS Power Engine VM downloads](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/vms). 
    * Installing a new VM typically doesn't require a firmware update — though a VM engine update may occasionally need a [newer firmware version](../bin/TeensyROM/) to use new capabilities.
  * Extract a VM's complete download onto the SD card root, keeping its launcher file and `VMS/` directory together.
  * Check the specific VM package's own instructions for controls, optional music, and any other setup notes.
  * For emulator-style VMs (e.g. NES, Game Boy, Game Gear), you'll need to supply your own ROM files — these aren't included in the VM download for copyright reasons. Place them in the `ROMS` subfolder inside that VM's own `VMS/` directory (e.g. `VMS/NESVM/ROMS/`).

## Launching a VM

  * Select the VM's launcher file (e.g. `DOOMVM.crt`) through the normal menu browser, the same way you'd launch any other cartridge file.
  * A Hot Key or Auto-Launch can also be set to a VM's launcher file for one-press or automatic launching, same as any other file.
  * Some VMs act as emulators for their own file format (e.g. a NES VM for `.nes` ROMs) — for these, you can select a ROM file directly from the browser instead of the launcher file, and TeensyROM automatically routes it to the right VM based on its extension.

## Returning to the TeensyROM menu

  * Press Reset or the Menu button to exit the VM and return to the normal TeensyROM interface.

## How it works

  * A VM owns its execution memory exclusively while it's running. Normal TeensyROM services (USB, Ethernet, ordinary cartridge support) resume automatically once you return to the menu.
  * VM engines and their C64-side clients are distributed separately from the firmware — adding a new VM just means copying files to the SD card, with no firmware flashing involved. That makes it easier and faster than a firmware update, since nothing about the running firmware changes.
