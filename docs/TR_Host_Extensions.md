
# TeensyROM Host Extensions (TR+ only)
Host Extensions are a feature that allows others to write and experiment with their own TeensyROM code, without modifying the core TeensyROM firmware. An extension is installed right from the TeensyROM menu, sits alongside the core firmware, and can be removed again at any time. Your TeensyROM keeps working exactly as before, with or without one installed.

## Table of contents
  * [What it is](#what-it-is)
  * [How it works](#how-it-works)
    * [Installing an extension](#installing-an-extension)
    * [Using an extension](#using-an-extension)
    * [Checking or removing the installed extension](#checking-or-removing-the-installed-extension)
    * [Writing your own](#writing-your-own)
  * [Mean Hamster Power Engine (MPE)](#mean-hamster-power-engine-mpe)
    * [Getting MPE.TRH and .mpe files](#getting-mpetrh-and-mpe-files)

## Thank you very much to:
* [**Kelly Fox**](https://github.com/kfox) for envisioning and implementing TeensyROM Host Extensions!
* [**John Swiderski**](https://github.com/ziggystar12) of Mean Hamster Software for the incredible work on MPE, the first Host Extension, and for the inspiration behind it.

## What it is
* A Host Extension is a separate firmware program that runs on your TeensyROM+, alongside the core TeensyROM firmware.
  * The TR+ sets aside its own dedicated area of flash memory for one extension, so the core firmware is never touched.
  * Extensions are distributed as `.TRH` files.
* While an extension is running, it has the full TeensyROM+ hardware at its disposal: the C64 bus (including DMA), SD card, Ethernet, and more.
  * This opens the door to things the core firmware was never designed to do, such as running entirely new game engines.
* Extensions are optional. The TeensyROM firmware comes with none installed, and works exactly as it always has.
* Only one extension can be installed at a time.
* TR+ (PCB v0.4) only: extensions rely on the TR+ bus-mastering DMA hardware.

## How it works
The TR+ flash holds the core TeensyROM firmware and one extension, side by side. The SD card holds the extension's `.TRH` file, its registration, and the files it plays:

![TR Host Extensions components](/media/Extensions/TR_Host_Extensions_Components.svg)

### Installing an extension
* Be sure your TeensyROM+ is using firmware version 0.9 or later.
  * See update instructions [here](General_Usage.md#firmware-updates) if an update is needed.
* Using a C64 Ultimate or Ultimate64? Set **`Cartridge Preference` to `External`** and **`Bus Operation Mode` to `Writes`** in its settings, as for the other TR+ DMA features.
* Copy the `.TRH` file to your SD card or USB drive.
* In the TeensyROM menu, select the `.TRH` file in the file browser.
* TeensyROM checks the file, shows the extension's name, and installs it.
  * The screen goes blank briefly while it writes. **Do not power off during this time.**
  * TeensyROM restarts when it's done.
* Installing a different extension replaces the one already installed.
* Firmware updates keep the installed extension in place.

### Using an extension
* An extension usually comes with a small registration folder for your SD card, which tells TeensyROM which file types belong to it (for example, `.mpe` files for MPE).
* Selecting one of those files from the SD card in the file browser hands the C64 over to the extension.
* To get back to the TeensyROM menu at any time, press the TeensyROM menu button, or reboot your C64.

### Checking or removing the installed extension
* In the TeensyROM Main Menu, select **F8** to go to the Settings Menu, then **0** for "Installed Extension".
  * This page shows the name of the installed extension, or "None installed".
* Press `u` to uninstall it, then `y` to confirm. TeensyROM restarts afterwards.
  * Also use `u` if the page says `None installed; slot not blank.` That means an earlier install or uninstall didn't finish.
* ***Important:*** Uninstall any extension before loading an older firmware without extension support. That firmware can't update itself while an extension is installed.
  * See [Firmware updates](General_Usage.md#firmware-updates) for more on updating.

### Writing your own
* Want to build your own extension? Start with the developer guide: [Writing your own extension host](Architecture/Extension-Hosts.md).
  * It includes a complete, minimal example extension to build from.

## Mean Hamster Power Engine (MPE)
The [Mean Hamster Power Engine (MPE)](https://meanhamster.com/games/mpe-power-engine) from Mean Hamster Software is the first Host Extension. It lets your C64 play games never released for it: NES, Game Boy, Doom, classic point-and-click adventures and more.

MPE is developed and distributed by Mean Hamster Software, separately from TeensyROM.

### Getting MPE.TRH and .mpe files
* **TeensyROM firmware:** MPE needs TeensyROM+ firmware 0.9 or later. Get the official TeensyROM release [from here](/bin/TeensyROM).
* **MPE.TRH and setup files:** download `MPE-Host.zip` from the latest release of [MHS-GAME-SUPPORT](https://github.com/ziggystar12/MHS-GAME-SUPPORT/releases/latest).
  * It includes `MPE.TRH`, the SD card registration files, and HamsterOS with its matching apps.
  * `MPE.TRH` is the engine itself. Each `.mpe` file is a game, or a game system player (such as NES), packaged to run on it.
  * Once `MPE.TRH` is installed and the registration files are on your SD card, select any `.mpe` file in the TeensyROM SD file browser to launch it.
    * Game ROMs (`.nes`, `.gb`, etc.) can be launched directly too: each one is automatically associated with its matching engine.
  * Follow Mean Hamster's [MPE host guide](https://github.com/ziggystar12/MHS-GAME-SUPPORT/blob/main/docs/MPE-HOST.md) for setup and launching games.
* **.mpe game files:** games and game conversion are available from the [Power Engine for C64](https://meanhamster.com/games/mpe-power-engine) page on MeanHamster.com.
  * Original game ROMs for MPE's NES, Game Boy and Game Gear players aren't included; those are available elsewhere.

<br>


[Back to main ReadMe](/README.md)
