# General TeensyROM usage

## Table of contents
  * [TeensyROM connections and Menu button](#teensyrom-connections-and-menu-button)
  * [Main Menu Options-Navigation](#main-menu-options-navigation)
  * [SD Card or USB Drive Setup](#sd-card-or-usb-drive-setup)
  * [Loading files and emulating ROMs](#loading-files-and-emulating-roms)
  * [The Settings Menu](#the-settings-menu)
  * [Selecting and associating Special IO](#selecting-and-associating-special-io)
  * [Firmware updates](#firmware-updates)
  * [Troubleshooting](#troubleshooting)

## TeensyROM connections and Menu button
  * With the power off, attach the TeensyROM to the Expansion port of your Commodore64 or 128 machine
  * Power on, the TeensyROM main menu should be displayed
  * The Menu button can be pressed at any time to return to the TeensyROM main menu from another program
  * Additional external hardware connection points:
    * ![TeensyROM connections](/media/TR_Connections.png)

## Main Menu Options-Navigation
  * (as of FW v0.8)
  * Menu navigation: Use the keyboard or a Joystick connected to Control Port 2
    * `CRSR or Joystick Up/Down` Move cursor up/down the list of files on the currently selected source device
    * `CRSR or Joystick Left/Right` Page up/down the list of files
    * `Return or Joystick Fire button` Select/run the highlighted file or enter sub-directory
  * Additional Keyboard commands:
    * `Up Arrow` Up 1 directory level
    * `a-z`  Search current directory for first item beginning with letter pressed
    * `Home` Move cursor to first item in directory
    * `Left Arrow` Write NFC Tag that will launch currently highlighted file
    * `?` Write NFC Tag that will launch a random file from the currently selected Directory
    * `<Shift-A>` Set Auto-Launch to currently highlighted file
    * `<Shift-M>` Transfer/Mount/Launch highlighted .Dxx file via attached [Meatloaf](https://github.com/idolpx/meatloaf) device
      * Requires Meatloaf using latest FW connected to the TR USB Host port.
    * `<Shift-K>` **(TR+ only)** Set highlighted file as the Kernal Replace image
    * `<Shift-R>` **(TR+ only)** Set highlighted file as the REU pre-load/save image
    * `1-5` Programmable Hot Keys to immediately launch a specified file
      * Defaults: `1` Cynthcart, `2` Station64 **(TR+ default: REU-Checker v1.0)**, `3` CCGMS, `4` TeensyROM ASID Player, `5` Jupiter Lander CRT
    * `!-%` (Shift `1-5`) Set corresponding Hot Key to currently highlighted file (on any media source)
    * `F1` Display files stored in Teensy Memory via firmware
    * `F3` Display files on an attached SD card
    * `F5` Display files on an attached USB Drive
    * `F7` Show Help Menu (2 pages: navigation/source keys, and per-file select options)
    * `F2` Exit to regular BASIC startup screen, TeensyROM deactivated
      * Re-activate TeensyROM by pressing button
    * `F4` Toggle background SID on/off
    * `F6` Show SID Information
    * `F8` Show Settings Menu
  * Screen contents:
    * **File Source and dir path** is shown in the upper left corner
    * **Page number and number of pages** is shown in the lower right corner
    * **Current time** is shown in the upper right.
      * Time is synched to the Teensy integrated RTC
      * If an RTC battery is present (built-in on TR+, or added to TR via [battery mod](RTC_Battery_Addition.md)), the time should be correct, no Ethernet needed.
      * Optionally, the time can be automatically synced via Ethernet on startup (see settings).
      * If neither of these are available, time can be set manually in the settings menu, or just let it start at midnight.
    * **File type** is to the right of each file/dir displayed. If "Unk" (unknown) then it is not a recognized/supported file type.
      * **'+'** in front of file type means it is pre-associated with [Special IO](#selecting-and-associating-special-io) emulation needed for function (MIDI, Swiftlink network) 
    * **Quick Help** is displayed at the bottom of the screen, use F7 for detailed help screen.

## SD Card or USB Drive Setup
  * General info/configuration
    * No TR code is stored on these external drives, so they are not required to start using your TeensyROM and all of its FW bundled programs.
    * SD Cards are somewhat favored over USB drives as they have slightly faster access times.
      * 32GB SD cards typically give the fastest directory read times
    * We've seen some issues with USB (or SD) drives with non-standard partitioning of the drive, especially manufacturer default partitions.  
      * If you have issues reading drive contents via the TR, try deleting the existing partition(s) (either via the 'diskman' Disk Management gui or using 'diskpart' and 'clean' via command line) then re-partition. After that, re-format in either FAT32 or FAT16 and load files.
    * Mac users: Before removing the SD card from your Mac, we recommend running the `dot_clean` command on the top-level directory of the volume, e.g. `dot_clean -m /Volumes/TEENSYSD`. This will remove all macOS-specific meta files from the SD card and reduce clutter.
  * Recommended Files/Games/SIDs
    * The [OneLoad64 Games Collection (v5)](https://www.youtube.com/watch?v=lz0CJbkplj0) is a wonderful (and free) source for thousands of files/games in CRT format, which is perfect for the TeensyROM
    * The [High Voltage SID Collection](https://hvsc.de/downloads) contains over 50,000 SID files, most of which are directly playable on the TeensyROM.
    * There's a great collection of Single Load Demos [located here](http://sensoriumembedded.com/tinyweb64/Demos/).
    * We recommend adding [this autolaunch.txt file](autolaunch.txt) to the root of an SD card in case you want to set up your TeensyROM as an unprompted diagnostics cartridge in the future.

    
## Loading files and emulating ROMs
  * Files can be launched from any of the available sources, including any subdirectory
  * When a file is selected, load/run status is displayed on the screen.  Usually this will flash by too quickly to read, but in the case of an error, it will pause to provide additional information.
  * File types supported:
    * **.PRG and .P00 files:** 
      * Super fast-loads any PRG into C64/128 RAM and executes
        * Same result as LOAD"file",x,1 and RUN (but much faster)
      * Must be single PRG file, not multi-part
    * **.CRT files:**
      * Emulates/supports most popular cartridge types:
        * All 8khi, 8klo, 16k, and C128 "Generic" carts
        * EasyFlash, Magic Desk, Ocean, Dinamic, Zaxxon/Super Zaxxon, GMod2
        * Epyx Fast Load, Game System 3, SuperGames, FunPlay/PowerPlay, Magic Desk 2
        * Ethernet (Swiftlink/Turbo-232), MIDI (Passport, Datel, Sequential, & Namesoft)
        * Action Replay, Super Snapshot V5 (freezer carts, **TR+ only**)
      * Additional CRT support info
        * Files larger than 850KB will automatically employ a bank-swap scheme 
          * These files must be run from an SD Card (not USB Stick)
          * Uses "old school" REU type of DMA assertion for fast pausing and no additional CPU execution
            * This method will not work on some systems (Most C128s and a low percentage of NTSC systems)
            * DMA Pause check utility included in Test+Diags dir to test specific system DMA reliability
          * Many large CRT files have been tested with this scheme, all are working smoothly (as long as host C64 passes DMA check)
          * See full CRT implementation details [here](CRT_Implementation.md).
        * On rev 0.2x PCBs, when using the C128 to emulate "Ultimax" carts (Deadtest, Jupiter Lander), some screen artifacts are visible. 
          * This issue only impacts UltiMax CRTs on C128s and is resolved in PCB rev 0.3
        * EasyFlash EAPI not currently supported
    * **.SID files:**
      * Play SID file: ~90% of known SID files are supported
      * Adjusts playback speed based on machine and SID type (NTSC/PAL)
      * SID info screen contains SID header information and playback controls
    * **.KLA/KOA files:**
      * Displays Koala multi-color picture
      * Compatible with output from [Retropixels online](https://www.micheldebree.nl/retropixels/)
    * **.ART/AAS/HPI/OCP/PIC files:**
      * Displays Art Studio Hi-Res picture
    * **.TXT/NFO/MD files:**
      * Displays ASCII text in the TR text viewer
      * See viewer usage/navigation doc via TR on your C64 in the /Text+PETSCII+Docs directory.
    * **.SEQ files:**
      * Displays PETSCII directly in the TR text viewer
      * Compatible with SEQ output from [lvllvl.com](https://lvllvl.com/) and [petscii.krissz.hu](https://petscii.krissz.hu/)
      * See viewer usage/navigation doc via TR on your C64
    * **.D64/.D71/.D81 files:**
      * Single File load/exec only (no multi-file/writebacks)
    * **.HEX files:**
      * Used for TeensyROM firmware updates (see below)

## The Settings Menu 
  * (as of FW v0.8)
  * Accessed via `F8` from the main menu. Organized into 9 indexed pages — settings changes take effect live and are stored in EEPROM (recalled on power-up).
  * Shared navigation, works from any page:
    * `CRSR Left/Right` Previous/Next page
    * `1-9` Jump directly to a page (see page # below)
    * `F1` Reboot TeensyROM (applies any power-up-only settings)
    * `Space Bar` Return to Main Menu

### 1. Index
  * Lists all 9 pages below for quick-jump access

### 2. Config: TeensyROM General
  * Emulation Selections:
    * `a/A` Cycle forward/backward through available Special IO to apply when launching a PRG/CRT (see [Selecting and associating Special IO](#selecting-and-associating-special-io))
      * Current Kernal Replace file is shown **(TR+ only)** — select via `<Shift-K>` from the file browser
      * Current REU Pre-load/save file is shown **(TR+ only)** — select via `<Shift-R>` from the file browser
  * User Interface/other:
    * `b` Cycle Serial device connected to USB Host Port: None, [NFC Reader](NFC_Loader.md), or [TR Controller](https://github.com/SensoriumEmbedded/TeensyROMControl)
      * Selected device is initialized on power-up/reboot
    * `c` Cycle Alt Button action **(TR+ only)**: `AutoLaunch` (default), `Pause/Unpause`, `TR Menu`, `Reboot TR`, or `None`
      * Only active when the button isn't already in use by a freezer cart or REU
    * `d/D` Increment/Decrement Joystick 2 repeat speed for menu navigation
    * `e` Toggle Show File Extensions
    * `f` Run Self Test (quickly verifies the TR can read its own emulated IO space)
      * See TR External Port and Expansion Port tests for more thorough testing.

### 3. Config: Startup Options
  * On Main Menu Startup:
    * `a` Toggle Play selected SID on startup (file shown; select from the SID Info page)
    * `b` Toggle Enable TCP Listener on port 2112 of the current IP, for remote control — see [ControlComms.md](ControlComms.md)
    * `c` Toggle Synch RTC via net on startup (Ethernet NTP time sync — see [Main Menu Options-Navigation](#main-menu-options-navigation) for how this interacts with a battery-backed RTC)
  * On TeensyROM Boot/Power-up:
    * `d` Toggle Auto-Launch Enable (file shown; select via `<Shift-A>` from the file browser)

### 4. Config: Menu Colors
  * Individual colors, each cycled through the 16 C64 colors:
    * `a/A` Screen Background
    * `b/B` Screen Border
    * `c/C` Top of screen banner color
    * `d/D` Time Display & Headings
    * `e/E` Input key option indicator
    * `f/F` General text/descriptions
    * `g/G` File names & Values
  * Presets:
    * `h` TR Default
    * `i` Black & White
    * `j` C64 Mono
    * `k` CGA
    * `l` The Blues
    * `m` Rainbow
  * `Return` Apply the selected colors

### 5. Config: MIDI Message Filters
  * Only message types marked "On" are passed through to the C64/128 — useful for DAWs or MIDI controllers that send extra packets that can overwhelm some C64 MIDI software (such as Cynthcart)
  * Packet types to transfer, each individually toggled on/off:
    * `a` Note Off/On (8x/9x)
    * `b` AfterTouch Poly (Ax)
    * `c` Control Change (Bx)
    * `d` Program Change (Cx)
    * `e` AfterTouch (Dx)
    * `f` Pitch Change (Ex)
    * `g` System Exclusive (F0)
    * `h` TimeCode QuarterFrame (F1)
    * `i` Song Position (F2)
    * `j` Song Select (F3)
    * `k` Tune Request (F6)
    * `l` Real Time System (F8-FF)

### 6. Config: Time Format/Real Time Clock
  * Format/Location:
    * `a` Toggle 12/24 hour clock display
    * `b/B` Set local Time Zone (UTC offset, half-hour increments)
  * RTC Adjustment:
    * `c` Synch RTC via Ethernet now (also displays the local IP address)
    * `d/D` RTC Hours down/up
    * `e/E` RTC Minutes down/up
    * `f/F` RTC Seconds down/up

### 7. Info: General
  * Current Ethernet IP Values: IP Address, Gateway IP, Subnet Mask
    * `a` Initialize/re-init Ethernet connection
  * TeensyROM/Machine info:
    * FW version and build date/time
    * Teensy clock speed and internal temperature
    * Unique Teensy hardware ID (also shown via the Version Info remote command — see [ControlComms.md](ControlComms.md))
    * Detected C64/128 clock rates (PAL/NTSC and CIA TOD) — handy for confirming what machine video/mains power standard the TR sees

### 8. Info: Ethernet
  * General Settings: MAC Address, IP Type (Static/DHCP)
  * DHCP Specific: DHCP Timeout, DHCP Response Timeout
  * Static IP Specific: Static IP, DNS IP, Gateway IP, Subnet Mask
  * This is a read-only display — to modify these values, use a terminal program (such as CCGMS) and the `AT?` command for help. See [Ethernet Usage](Ethernet_Usage.md).

### 9. Info: HotKeys
  * Shows the currently assigned file for each of the 5 programmable Hot Keys (#1-5)
  * Reassign from the main menu file browser using `!`/`"`/`#`/`$`/`%` (Hot Keys 1-5 respectively)

## Selecting and associating Special IO
  * What is it?
    * "Special IO" in this context means additional HW that is emulated to assist with SW function.
    * This emulation runs at the same time as a selected program/cartridge
    * Examples of these are Swiftlink/Modem, and MIDI interfaces
    * This HW uses the IO1 ($DE00) address space and interrupts to pass information to/from the program running on the C64/128.
  * How is it used/selected?
    * The easy method is to use the software supplied with the TeensyROM with a '+' sign in front of the type.
      * These will automatically associate the needed IO to emulate Swiftlink Internet or the required MIDI interface.
      * This includes the following, for example:
        * **CCGMS Terminal** Pre-configured and associated with Swiftlink interface to [Ethernet connection](Ethernet_Usage.md)
        * **Cynthcart** Associated with the Datel MIDI interface to [USB MIDI Host/Device](MIDI_Usage.md)
        * **Station 64** Associated with the Passport MIDI interface [USB MIDI Host/Device](MIDI_Usage.md)
    * Special IO can also be selected in the settings menu
      * This setting will be applied at the time of launching any program or generic cartridge
      * The setting stays in memory and will be re-loaded for any app until changed
      * We recommend disabling NFC if Special IO is selected to avoid interference.
    * If a selected CRT file is associated with different Special IO (i.e. Epyx, EasyFlash etc), that Special IO will be loaded instead

## Firmware updates
  There are multiple ways to update the TeensyROM firmware, choose one of the following:

### **From SD Card or USB Thumb Drive**
  * This method is only available in FW v0.4 and higher.
    * Older versions will have to use one of the other methods one time to update
  * Get the .hex file containing the latest major (x.x) or minor(x.x.x) release [from here](/bin/TeensyROM)
  * Copy the file to a USB Thumb drive or SD card
    * This can be done by the traditional method of moving the card to a capable computer
    * Or directly via USB using the **[TeensyROM UI](https://github.com/MetalHexx/TeensyROM-UI)** or Cross-platform **[Command Line Interface](https://github.com/MetalHexx/TeensyROM-CLI)**
  * In the TeensyROM USB or SD Menu, select the Firmware  .hex file
  * A new screen will open and ask you to confirm that you want to update
    * Check that the file name shown is correct
    * 'y' to confirm/continue, 'n' to abort
    * The update process takes about 2 minutes and goes through several stages.
    * ***Important*** You must leave your C64/128 powered up during the update
      * *Interrupting this process could render your TeensyROM unusable*
    * When the update completes successfully, your computer will reset and the new version of TeensyROM will be shown
    * If there are any problems, take note of any messages shown before pressing any key to return to the main menu.

### **Directly using TeensyROM via Ethernet connection**
  * Digitalman uses a similar approach in [this video](https://www.youtube.com/watch?v=PGRFLHmw0hY)
  * This method uses features added in FW v0.5.8
    * Older versions will have to use one of the other methods one time to update
  * Connect active Ethernet cable to TeensyROM
    * An inserted SD card (preferred) or USB Thumb drive is also required
  * Open CCGMS from the main menu and type "atbrowse"
  * There are two sites hosting TeensyROM firmware updates, use one of the default bookmarks to enter either of them
    * Bookmark #1 is for the TinyWeb64 site, entering "b1" will take you to sensoriumembedded.com/tinyweb64
    * Bookmark #2 is for Digitalman's site, so "b2" to go to digitalman.azurewebsites.net
    * Alternatively the 'U' command will also work: "u sensoriumembedded.com/tinyweb64"
  * From the main menu, link #5 will take you to the Firmware download section on either site
  * Choose the link # for the FW version you want to download/install
    * The FW file will take a minute or two to download, dots shown on screen to indicate progress
  * Use the "d" command to see the local downloaded files directory.
  * Choose the link # of the firmware .hex file downloaded in the previous step to directly launch the update.
    * Check the version and confirm as described in the section above.

### **Using the TeensyLoader application** (requires computer with USB connection)
  * Get the .hex file of the latest released version [from here](/bin/TeensyROM/)
  * Download and install the [Teenyduino/TeensyLoader app](https://www.pjrc.com/teensy/td_download.html)
    * Teensyduino requires arduino to run, which works fine.
    * Alternatively, the Teensyloader stand alone file can be downloaded via a link further down the page
  * Launch the Teensyloader/teensyduino app (teensy.exe)
  * Select File, Open HEX File and select the save TeensyROM .hex file
  * Connect the TeensyROM to your computer with a USB A to microB cable.
  * Plug the TeensyROM into your C64/128 and power it up to the main menu
  * Press the white button on the Teensy module itself, as shown in the app
  * The process only takes a few seconds to complete
    * ***Important*** You must leave your C64/128 powered up during the update
  * You should see the app process through erasing and programming before your C64/128 reboots with the new version

### **Using the arduino environment** (for custom builds/code)
  * See the [Software build document](/Source/BuildInfo.md) for details on this process.

## Troubleshooting

### Commodore C64 Ultimate
  * Check settings to be sure no other devices (such as a SID/UtiliSID or ACIA/Swiftlink mapping) are using the IO1 ($DE00) location. Some CRTs emulated by the TR can use the IO2 ($DF00) range as well.
  * Set the Cartridge Port to the default settings (Preference: Auto, Bus Operation: Quiet, Bus Sharing: Both for all)

### Resetting EEPROM Contents
  * To set *all* EEPROM values back to default settings:
    * With C64 off, press and hold down menu button on the TeensyROM
    * Power up the C64 (nothing will be displayed)
    * Keep TR menu button pressed for ~10 sec, until the LED starts blinking
    * Release the TR Menu button and wait for the TR menu to be displayed
    * TR EEPROM settings are now reset to default   

### Additional help
  * For additional support, please consider joining the [TeensyROM Discord](https://discord.gg/ubSAb74S5U)
  * Or contact Travis directly via [e-mail](mailto:travis@sensoriumembedded.com).
  
<br>

[Back to main ReadMe](/README.md)

