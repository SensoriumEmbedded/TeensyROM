# General TeensyROM usage

## Table of contents
  * [TeensyROM connections and Restart button](#teensyrom-connections-and-restart-button)
  * [Main Menu Options](#main-menu-options)
  * [Loading programs and emulating ROMs](#loading-programs-and-emulating-roms)
  * [The Settings Menu](#the-settings-menu)
  * [Selecting and associating Special IO](#selecting-and-associating-special-io)
  * [Firmware updates](#firmware-updates)

## TeensyROM connections and Restart button:
  * With the power off, attach the TeensyROM to the Expansion port of your Commodore64 or 128 machine
  * Power on, the TeensyROM main menu should be displayed
  * The Restart button can be pressed at any time to return to the TeensyROM main menu from another program
  * Additional external hardware connection points:
* ![TeensyROM connections](/media/TR_Connections.png)

## Main Menu Options/Navigation (as of FW v0.5.15)
  * Menu navigation: Use the keyboard or a Joystick connected to Control Port 2
    * **CRSR or Joystick Up/Down:** Move cursor up/down the list of files on the currently selected source device
    * **CRSR or Joystick Left/Right:** Page up/down the list of files
    * **Return or Joystick Fire button:** Select/run the highlighted file or enter sub-directory
  * Additional Keyboard commands:
    * **Up Arrow:** Up 1 directory level
    * **Home:** Move cursor to first item in directory
    * **a-z:** Search current directory for first item beginning with letter pressed
    * **1-4:** Hot Keys to launch the following:
      * 1: Cynthcart
      * 2: Station64
      * 3: CCGMS
      * 4: TeensyROM ASID Player
    * **F1:** Display files stored in Teensy Memory via firmware
    * **F3:** Display files on an attached SD card
    * **F5:** Display files on an attached USB Drive
    * **F7:** Show Help Menu
    * **F2:** Exit to regular BASIC startup screen, TeensyROM deactivated
      * Re-activate TeensyROM by pressing button
    * **F4:** Toggle background SID on/off
    * **F6:** Show SID Informartion
    * **F8:** Show Settings Menu
    * **Space Bar:** 
    * **Left Arrow:** Write NFC tag that will launch the highlighted file
  * Screen contents:
    * **File Source and dir path** is shown in the upper left corner
    * **Page number and number of pages** is shown in the lower right corner
    * **Current time** is shown in the upper right (if ethernet connected/synching).  Otherwise the time will start at midnight upon startup.
    * **File type** is to the right of each file/dir displayed. If "Unk" (unknown) then it is not a recognized/supported file type.
      * **'+'** in front of file type means it is pre-associated with Special IO emulation needed for function (MIDI, Swiftlink network) 
    * **Quick Help** is displayed at the bottom of the screen, hit space for detail help screen.

## Loading files and emulating ROMs
  * Files can be launched from any of the available sources, including any subdirectory
  * When a file is selected, load/run status is displayed on the screen.  Usually this will flash by too quickly to read, but in the case of an error, it will pause to provide additional information.
  * File types supported:
    * ***.PRG and .P00 files:** 
      * Super fast-loads any PRG into C64/128 RAM and executes
        * Same result as LOAD"file",x,1 and RUN (but much faster)
      * Must be single PRG file, not multi-part
    * ***.CRT files:**
      * Emulates/supports most popular cartridge types:
        * All 8khi, 8klo, 16k, and C128 "Generic" carts
        * EasyFlash, Magic Desk, Ocean, Dinamic, Zaxxon/Super Zaxxon
        * Epyx Fast Load, Game System 3, SuperGames, FunPlay/PowerPlay
        * Swiftlink internet, MIDI (Passport, Datel, Sequential, & Namesoft)
      * Additional CRT support info
        * Max file size is ~650MB (impacts large EZF files), also no EasyFlash eapi support at this time
        * If your favorite game isn't yet supported (though most are at this point), [send me a note](mailto:travis@sensoriumembedded.com) and I'll look at prioritizing it.
        * The [OneLoad Games Collection (v5)](https://www.youtube.com/watch?v=lz0CJbkplj0) is a great/free source for thousands of CRT files/games
    * ***.SID files:**
      * Play SID file: ~90% of known SID files are supported
      * Adjusts playback speed based on machine and SID type (NTSC/PAL)
      * SID info screen contains SID header information and playback controls
    * ***.KLA/KOA files:**
      * Displays Koala multi-color picture
    * ***.ART/AAS/HPI files:**
      * Displays Art Studio Hi-Res picture
    * ***.HEX files:**
      * Used for TeensyROM firmware updates (see below)

## The Settings Menu (as of FW v0.5.12)
  * Keyboard commands available from the Settings Menu:
    * These commands modify settings stored in the Teensy, and are recalled on power-up
      * **a/A:** Set local Time Zone for system/screen clock (applied next Ethernet time synch)
      * **b/B:** Select Special IO to apply when launching a PRG/CRT (see below)
      * **c/C:** Set Port 2 Joystick repeat speed for menu navigation from 0 (very slow) to 15 (super fast) 
      * **d:** Toggle auto Internet Time Synch on power-up
      * **e:** Toggle auto background music on power-up
      * **f:** Toggle NFC Reader enabled on power-up
      * **g:** Toggle RW Ready timing delay (to improve Hi-ROM games on Reloaded MKII and C64c)
    * These commands only change settings for this session and execute immediately
      * **h:** Reboot TeensyROM to execute updated power-up settings
      * **i:** Perform Internet Time Synch
      * **j:** Toggle background music on/off
      * **k:** Execute Self-Test (takes ~4 seconds)
        * Tests the TeensyROM ability to rapidly read from emulated ROM
        * Helpful in testing out HW and debug, but should not fail in normal use
        * The is not an exhaustive test, but may be expanded later
      * **l:** Display Help menu
      * **Space Bar:** Return to Main Menu
      * **Return:** Screen refresh.  Handy for updating the temperature reading 
  * Other information on the Settings screen
    * Build Date/Time
      * In addition to the FW version number, the date/time of the build is also logged.
      * This is helpful for tracking custom builds & sub-releases
    * Teensy Frequency and Temperature
      * As mentioned in the main Readme, the Teensy is slightly overclocked to 816MHz in this aplication
      * External cooling is not required for this speed. However, in abundance of caution, a heatsink is specified in the BOM for this project.
      * The max spec is 95C, and there is an automatic shutdown at 90C.
      * Even in extended use, I've never seen the internal temperature exceded 75C.
    * Free RAM space available for large .CRT files

## Selecting and associating Special IO
  * What is it?
    * "Special IO" in this context means additional HW that is emulated to assist with SW function.
    * This emulation runs at the same time as a selected program/cartridge
    * Examples of these are Swiftlink/Modem, and MIDI interfaces
    * This HW uses the IO1 ($DE00) address space and interrupts to pass information to/from the program running on the C64/128.
  * How is it used/selected?
    * The easy method is to use the software supplied with the TeensyROM with a '+' sign in fron of the type.
      * These will automatically associate the needed IO to emulate Swiftlink Internet or the required MIDI interface.
      * Currently this includes the following:
        * **CCGMS Terminal** Pre-configured and associated with Swiftlink interface to [Ethernet connection](Ethernet_Usage.md)
        * **Cynthcart** Assiciated with the Datel MIDI interface to [USB MIDI Host/Device](MIDI_Usage.md)
        * **Station 64** Assiciated with the Passport MIDI interface [USB MIDI Host/Device](MIDI_Usage.md)
    * Special IO can also be selected in the settings menu
      * This setting will be applied at the time of launching any program or generic cartridge
      * The setting stays in memory and will be re-loaded for any app until changed
    * If a selected CRT file is associated with different Special IO (ie Epyx, EZFlash etc), that Special IO will be loaded instead

## Firmware updates
  There are multiple ways to update the TeensyROM firmware, choose one of the following:

### **From SD Card or USB Thumb Drive**
  * This method is only available in FW v0.4 and higher.
    * Older versions will have to use one of the other methods one time to update
  * Get the .hex file containing the latest major (x.x) or minor(x.x.x) release [from here](/bin/TeensyROM)
  * Copy the file to a USB Thumb drive or SD card
    * This can be done via the traditional method of moving the card to a capable computer
    * or over USB using the provided C64Transfer Windows app
  * In the TeensyROM USB or SD Menu, select the Firmware  .hex file
  * A new screen will open and ask you to confirm that you want to update
    * Check that the file name shown is correct
    * 'y' to confirm/continue, 'n' to abort
    * The update process takes about 2 minutes and goes through several stages.
    * ***Important*** You must leave your C64/128 powered up during the update
      * *Interrupting this process could render your TeensyROM unuseable*
    * When the update completes succesfully, your computer will reset and the new version of TeensyROM will be shown
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
    * Alternately the 'U' command will also work: "u sensoriumembedded.com/tinyweb64"
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
    * Alternately, the Teensyloader stand alone file can be downloaded via a link further down the page
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

<br>

[Back to main ReadMe](/README.md)

