# General TeensyROM usage

## Hardware connection/button:
  * With the power off, attach the TeensyROM to the Expansion port of your Commodore64 or 128 machine
  * Power on, the TeensyROM main menu should be displayed
  * The button can be pressed at any time to return to the TeensyROM main menu from another program

## Main Menu Options
  * Keyboard commands available from the main menu:
    * **F1:** Display files stored in Teensy Memory by firmware
    * **F3:** Display files on an attached SD card
    * **F5:** Display files on an attached USB Drive
    * **F7:** Display files sent via USB from the Windows utility
    * **F2:** Exit to regular BASIC startup screen, TeensyROM deactivated
      * Re-activate TeensyROM by pressing button
    * **F4:** Toggle background music on/off
    * **F6:** Go to Settings Menu
    * **F8:** Enter the MIDI2SID application
    * **A-P:** Select a file to load/run/emulate
    * **Cursor Up/Down:** Page up/down the list of files on the currently selected source device

## Loading programs and emulating ROMs
  * Files can be launched from any of the 4 sources, including any subdirectory of an SD or USB drive
    * Maximum path+file name length=300 characters
  * File types supported:
    * ***.PRG files:** 
      * Super fast-loads any PRG into RAM and executes
        * Same result as LOAD"file",x,1 and RUN
      * Must be single PRG file, not multi-part
        * The [OneLoad Games Collection](https://www.youtube.com/watch?v=qNxJwLujaN8) is a good source of single file PRG and CRTs
    * ***.CRT files:**
      * Emulates 8khi, 8klo, 16k, and C128 ROMs
      * Emulates all "Normal" ROM carts and several special HW types
        * Epyx, Swiftlink, MIDI etc
        * More special HW CRT support is in development.
          * If your favorite game isn't yet supported, [send me a note](mailto:travis@sensoriumembedded.com) and I'll look at prioritizing it.
    * ***.HEX files:**
      * Used for TeensyROM firmware updates (see below)

## The Settings Menu
  * Keyboard commands available from the Settings Menu:
    * These commands modify settings stored in the Teensy, recalled on power-up
      * **F1:** Toggle auto Internet Time Synch on power-up
      * **F3:** Toggle auto background music on power-up
      * **F5:** Set Time Zone for system/screen clock
      * **F7:** Select Special HW to apply when launching a PRG/CRT
    * These commands only change settings for this session and execute immediately
      * **F2:** Perform Internet Time Synch
      * **F4:** Toggle background music on/off
      * **F6:** Go to Main Menu
      * **F8:** Execute Self-Test
        * Tests the TeensyROM ability to rapidly read ~655k bytes from emulated ROM
        * Helpful in testing out HW and debug, but should not fail in normal use
        * The is not an exhaustive test, but may be expanded on later
      * **Return:** Screen refresh.  Handy for updating the temperature reading 
  * Other information on the Settings screen
    * Build Date/Time
      * In addition to the FW version number, the date/time of the build is also logged.
      * This is helpful for tracking custom builds & pre-releases
    * Teensy Frequency and Temperature
      * As mentioned in the main Readme, the Teensy is slightly overclocked to 816MHz in this aplication
      * External cooling is not required for this speed. However, in abundance of caution, a heatsink is specified in the BOM for this project.
      * The max spec is 95C, and there is a panic shutdown at 90C.
      * Even in extended use, I've never seen the internal temperature exceded 75C.
    * Open source reminder, URL to the Github location

## Selecting/associating "Special HW"
  * What is it?
    * "Special HW" in this context means additional HW that is emulated to assist with SW function.
    * This emulation runs at the same time as a selected program/cartridge software
    * Examples of these are Swiftlink/Modem, MIDI, and Epyx cartridge emulators.
    * This HW uses the IO1 ($DE00) address space and interrupts to pass information to/from the program running on the C64/128.
  * How is it used?
    * Special HW can be selected using F7 in the settings menu
    * This setting will be applied at the time of launching any program or cartridge
    * The setting stays in memory and will be re-loaded for any app until changed
    * If a CRT file is associated with different Special HW (ie Epyx, etc), that Special HW will be loaded instead

## Firmware updates
  There are 3 ways to update the TeensyROM firmware

### **From SD Card or USB Thumb Drive** (easiest method)
  * This method is only available in FW v0.4 and higher.
    * Older versions will have to use another method one time to update
  * Get the .hex file of the latest released version [from here](/bin/TeensyROM)
  * Copy the file to a USB Thumb drive or SD card
  * Attach the card/drive to the TeensyROM and power up your C64/128
  * In the USB or SD Menu, select the Firmware  .hex file
  * A new screen will open and ask you to confirm that you want to update
    * Double check that the file name shown is correct
    * 'y' to confirm/continue, 'n' to abort
    * The update process takes about 1 minute and goes through several stages.
    * ***Important*** You must leave your C64/128 powered up during the update
      * *Interrupting this process could render your TeensyROM unuseable*
    * When the update completes succesfully, your computer will reset and the new version of TeensyROM will be shown
    * If there are any problems, take note of any messages shown before pressing any key to return to the main menu.

### **Using the Teensy aplication** (requires computer with USB connection)
  * Get the .hex file of the latest released version [from here](/bin/TeensyROM/)
  * Download and instal the [Teenyduino/TeensyLoader app](https://www.pjrc.com/teensy/td_download.html)
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

