
# FW Release Version history:

### 0.7 Release 2025/12/04
* Note: EEPROM Settings will reset with this FW update
* New Remote Command/Control Options:
  * All remote commands now available via these interfaces (in addition to USB Device port)
    * Ethernet (TCP) Listenner Interface
      * Supported by updated [TR Win App](https://github.com/SensoriumEmbedded/TRWinApp)
      * TCP Listenning option in settings
      * Implemented in full and minimal builds
    * USB-A Host port communication
      * Supported by new [TRControl](https://github.com/SensoriumEmbedded/TeensyROMControl) library
        * Use with any USB enabled microcontroller (ie Arduino, RPi, etc)
      * "NFC Enabled" settings item changed to "Host Ser Dev".  Selects from "None", "NFC", or "TRCont"
  * Added ability to remote launch TeensyROM Flash based programs and read directory
  * NDJSON support option for remote dir listing
* RetroMate v1.02 (Games Menu), adds 1351 proportional mouse support (@stefanwessels)
* "SNKvsCapcom Stronger Edition" support:
  * Updated Magic Desk 2 to CRT HW ID #85, 79 still remains as well
  * Bank Swap cache increased to 16 blocks 
  * Swap data sent to USB Serial for monitoring
* Programmable Hot Keys 1-5, set with !-% when target is highlighted
  * Defaults same as previously hard coded
  * !-% (shift 1-5) to set hot key to to highlighted file on TR, SD, or USB
  * Hot key paths stored in EEPROM for persistance
  * Thanks for the prompt @the1Domo @mrc333777 @Richard @William Manganaro @Niclas @Divertigo @Avrilcadabra
* Swiftlink improvements for q-link compatibility:
  * Tx interrupt capability 
  * Corrected Swiftlink reset via status write
* NFC Re-launch tag algo: 
  * Re-launch of normal/non-random card is blocked until another card is used or menu button pressed.
  * Does allow re-launch of "random" tags (after 1 sec of no tag present).  
  * Thank you! @borisschneiderjohne @jochwat68 @0ldskull 
* Show/Hide File Extension option (default hidden) 
  * Shows regardless for unknown types and directories
  * Thank you! @parkamonster
* If selected filetype is unknown assume it's a PRG and attempt to launch (@idolpx)
* Ethernet Init and Time sych verbosity improvements
  * Displays info to screen from settings menu (time synch), on startup (if synch/listenner enabled), or from terminal ATC.
* RW Read Delay option removed, using 135nS for all (has been default)
* DMA pause unique timing for system pause (During !BA) vs large CRT bank switching (immediate after IOW)
* Increased max files per dir from 3000 to 4000

### 0.6.8 Release 2025/07/25
* Swiftlink/Turbo-232 speed & compatibility improvements
  * New baud rate control implementation
    * Swiftlink baud rate control supports up to 38Kbps
    * New Turbo-232 register added to allow speeds up to 230Kbps!
  * Supported and tested with [**C64OS**](https://c64os.com/c64os/) 
    * [v1.08 beta now available](https://www.c64os.com/c64os/beta)
    * Thank you to @Greg Nacu for all your guidance/support/feedback!
  * Simple Swiftlink Terminal (SST) addition
    * Stripped down Swift/Turbo interface to demonstrate high speed polling or interrupt based receiving
    * Not feature rich, but very fast.  Try a connection using polling at 230k baud.  :)
    * [Source code included](https://github.com/SensoriumEmbedded/TeensyROM/tree/main/Source/C64/SimpSwiftTerm/source) for developer reference and incorporation into other C64 apps.
  * Other improvements
    * Flow Control: RTS and Transmitter Ctl via Command Reg
    * Polling mode and interrupt Improvements
      * Polling allow fast data receiving without the overhead & timing requirements of an interrupt/handler
    * Proper "chip reset" on write to status reg
    * Fix: clear receive buffer and drop any current client connection on Swift/Turbo startup
      * Issue  introduced in FW 0.6.7, Thank you @Stefan
    * NMI Timeout increased from 300->7000uS for slow NMI handlers
    * Non-AT command just returns "error" instead of "AT not found"
    * Special IO name changed to "Swift-Turbo/Modem"
    * Turbo232 CRT type detection
    * Debug build options to force baud, spy registers, etc via USB Serial
* TR Main Menu additions
  * Retromate Internet Chess (Games dir)
    * Play Chess on-line against bots or other players
    * Written for the TeensyROM utilizing the high speed polling Turbo-232 interface
    * Thank you @Stefan Wessels for your incredible contribution!
      * Email: swessels@email.com
      * Source on GitHub: https://github.com/StewBC/retromate
  * 2SID version of SID Wizard and SW User Manual (MIDI_ASID dir)
    * Thank you to Hermit for these great programs and for allowing this inclusion in the TR!
    * Also @Voynich for the suggestion
  * Super Simple Terminal (Utilities dir)
    * See description above  
  * LOAD"*",8,1 and RUN  (Utilities dir)
    * Handy for quick run/load from IEC drive/emulator, if connected
    * Can choose this for autolaunch to immediately launch into floppy based program on system power-up.
* File type support additions
  * Magic Desk 2  .CRT support
    * Uses DMA pause feature for large (2MB) file size
    * Comparatively new CRT format, [documented here.](https://github.com/crystalct/MagicDesk2)
    * Required for ["SNK vs CAPCOM - Stronger Edition"](https://www.youtube.com/watch?v=dNT9yBsi1Dk)
  * .NFO and .MD files now viewable as text 
    * Thank you @hExx for the recommendation.
* Other fixes
  * Auto-Set Special IO to None when enabling NFC in settings menu to avoid conflict/slowdown
  * Exit to BASIC (F2) didn't always include "Special IO" selected handler, fixed
  * Check for for 1-3 character extensions, not just 3
  * Teensyduino environment updated to 1.60b4
    
### 0.6.7 Release 2025/06/14
* Note: EEPROM settings will be reset with this FW version (color settings added)
* TeensyROM UI Customizable color scheme
  * Color setting page addition/features:
    * 'C' (capital C) from any Directory Menu to enter page
    * Choose any of the 16 available colors for 7 different color parameters.
    * 6 presets available, or create your own scheme
  * Color scheme saved in TR EEPROM for recollection  after power down/reboot
  * Some general color consolidation/standardization so all are customizable
* Very Large CRT file support: (Beta)
  * Example files in this category "A Pig Quest", "Eye of the Beholder" and "SNK vs Capcom"
  * CRT Files >850Kb employ a bank swapping from SD card (only) mechanism
    * First 850Kb stored in TR RAM as usual, remainder are marked for swapping
    * Uses the DMA signal to halt the CPU for ~3mS during an un-cached bank swap.
      * No actual DMA (bus mastering) takes place
    * Swaps are fast and typically only take place during "scene changes" in games.
      * Should be imperceptible to user experience.
  * 8ea 8K bank RAM cache with lookup to re-use already cached banks without pausing
  * Uses "old school" REU type of DMA assertion for fast pausing and no additional CPU execution
    * Not reliable on some systems (Most C128s and a low percentage of NTSC systems)
    * DMA Pause check utility included in Test+Diags dir to test specific system DMA reliability
      * Written in BASIC with test hooks in TR BASIC commands IO Handler
  * Many large CRT files have been tested with this scheme, all are working smoothly (as long as host C64 passes DMA check)
  * File sizes of <850KB continue to work as they do today, all served directly out of RAM.
  * See full CRT implementation details [here](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/CRT_Implementation.md).
  * Thank yous:
    * @Boris Schneider-Johne for the general idea behind this capability, very much appreciate the brainstorming!
    * @DigitalMan, @Hexx, @William Manganaro, and @JTHonn for the testing and feedback!
* ASID Player: (rev to 1.3)
  * New ASID packet types decoding/application:
    * Write Order: Ability to customize the order in which regs are written to the SID
      * 'w' to Stop Forced Reg Write Order
    * Framerate: Expected Vid, Speed Mult, Buffering requested, Frame Delta uS
      * Parameters printed to screen, Frame rate applied to buffer/timer.
    * Expected SID types: Chip index & chip type printed to screen
    * Thank you @tubesockor for the new packet specification, SFII builds, and general guidance/reviews!
      * Tested with SID Factory II advance build, supporting release coming soon.
  * 2nd/3rd SID address defaults from $d420/$d440 to $df00/None (for commonality with Cynthcart 2nd SID address)
  * Fix: Start/Stop Message Tokens used incorrect addressing
* Serial/Remote interface:
  * GetDirectoryCommand now checks for directory existence and returns error codes for specific issues.
  * GetFileCommand now checks for storage availability and returns error codes for specific issues.  
  * Fix: Remote CopyFile() now properly overwrites instead of appending.
    * Thank you @Hexx for these updates/pushes
  * 'v' version check available in minimal mode (as well as normal)
  * Print name of remote launched file/path to USB serial
  * Remote tokens to control DMA system pause via USB serial
  * Remote token to set TR UI colors remotely
* Swiftlink compatibility updates:
  * Improved compatibility with the Hayes Modem standard and [**C64OS**](https://c64os.com/c64os/)
    * Thank you to @Greg Nacu for all your guidance/support/feedback!
    * TR can now be used for C64OS Networking beginning w/ C64OS v1.08
  * AT Command updates/additions:
    * ATEx  Turn Echo on/off (1/0)
    * ATVx  Verbose mode on/off (1/0)
      * impacts response codes for all commands (keywords vs numeric) 
        * also text suppression for atdt, atc, ate, and atv commands
    * ATI  Host Info: "TeensyROM" and "Firmware v<version #>"
    * ATDT<Server><:Port>
      * Ethernet init here instead of on SwiftLink startup
      * Allow quotes before/after server name or port
    * ATZ  (soft reset) restores echo and verbose mode, regardless of argument
    * ATH  (Hook) Dummy function to return OK, +++ command already disconnects
    * AT? help list update
    * All AT commands now use standard response codes when complete
      * Key word or Number depending on Verbose setting
        * Key words sent as upper case ASCII (aka lower case PETSCII)
      * Followed by a single carriage return
  * Hardware handshaking
    * Status register DCD (Carrier Detect) bit now readable for connect status
* SID Player:
  * Fix: Correctly initialize timer for multi-speed (2x/4x/6x/8x) SIDs
    * Thank you @DivertigO for the find and testing
  * Fix: Corrected row# for set background SID "Done" message
* TR BASIC Commands:
  * Allow TLOAD to non-$0801 locations
    * Will error if conflict with TR BASIC Ext code ($c000-d000)
  * Thank you @RMR for the find and fix
* General
  * CIA TimeOfDay clock check BASIC program added to TR Test+Diags directory
  * Build option to force NTSC & skip auto check (DbgForceNTSC)
  * Build option in assy prints init status on TR menu/CRT start (DbgVerbose)
  * IO1 check now in CRT, but only if DbgVerbose enabled
  * Debug and Swiftlink messages indicated at startup when enabled

### 0.6.6 Release 2025/02/15
* SID File Player updates
  * Individual voice muting during SID file playback!
    * Voice 1/2/3 mute status/control on SID Info page
    * Thank you @Avrilcadabra for the idea, testing, **and implementation methodology** 
  * Fine control and percentage based display for SID speed changes
    * Log or Linear control options, display to 0.01%
  * Capability to remote control individual SID voice muting and play speed.
    * Thank you @hExx for the testing and forthcoming **TeensyROM UI implementation.**
* New Feature: Text and PETSCII file viewer
  * Directly open .TXT (ASCII) or .SEQ (PETSCII) files for viewing
  * Navigate through pages/files, customize colors
  * 15 sample text/PETSCII files added to new main menu sub-dir
    * Includes text viewer Usage/instructions doc
  * Thanks to @William Manganaro for the suggestion/testing
* 24 hour clock display option
  * 12/24 hour display selectable in Settings Menu
  * Setting saved in EEPROM, stays persistent.
    * Note: This FW will reset EEPROM to defaults 
  * Thank you for the idea: @Lefty
* Other updates
  * Auto launch selection key changed from shift-return to 'A' (shift-a)

### 0.6.5 Release 2025/01/20
* NFC Loading System updates:
  * Support for **"Mifare Classic 1k"** NFC tags
    * No pre-formatting required, usable as received.
    * Tags of this type are often included with NFC reader module purchases
    * As with "Ultralight" (NTAG213/215/216) tags, "Classic 1k" tags can be written from a phone or the TeensyROM itself
  * Bug Fix: Reset tag re-use timer *after* launch in case random dir takes a long time
    * Random tags occasionally caused repeated random load/run loop.
    * Thanks @Falcon for the find/testing
* Program (PRG) loader and BASIC filesystem
  * Setting current device number to 8 so that file browsers will point at IEC for dir read, etc
  * Load function key vector if JiffyDOS Kernal is present
  * Main menu /Utilities directory updates/additions:
    * "Exit to BASIC" for NFC taggable method to enter BASIC
    * "TeensyROM Menu Cart..." for NFC taggable method to enter TR menu
  * Thank you to @Boris Schneider-Johne for all the recommendations/support/testing on these items!
  
### 0.6.4 Release 2025/01/03
* NFC Loading System updates:
  * NFC Random selection from a specified directory:
    * "?" to write tag for random via currently selected directory (in TR, SD, or USB)
    * Recommend using random with directories containing <100 items. Function must load full directory to pick one at random, so large directories will take longer to pick/launch from.  
    * Thanks to @distressed74  @Avrilcadabra  @Makers Mashup  @AndyDavis @William Manganaro for the thoughts/feedback
  * Can now re-scan/launch same NFC tag as previously scanned, if it is away from the reader for >1 full second
    * Allowance to enable re-use of same "random" tag repeatedly
* MIDI/ASID updates:
  * Fix for SIDWizzard "Jam Mode" (tracker playback simultaneous w/ MIDI enabled/receiving)
    * No longer setting StatusIRQReq bit on MIDI out msgs
    * Thanks to @DivertigO for identifying/testing
  * New ASID commands parsed and parameters/values displayed (but not yet applied)
* Emulation/loading improvements:
  * GMod2 .CRT support (Bank switching only)
    * Thanks for the prompt @Mike351 and sorted CRT files @AmokPhaze101 
  * Clear cart signature from $8004 of RAM if present (Thanks Artur Rataj for the info/recommendation)
    * malicious setting written by some games (ie "Hero") that can stay resident and prevent restarts
* Autolaunch improvements for diagnostic use:
  * Autolaunch capability from SD file: /autolaunch.txt
    * Example autolaunch.txt file with instructions added to /docs directory
    * Takes priority over EEPROM set autolaunch, when enabled
    * This is a means to enable diagnostics autolaunch *without* a functional C64
  * Autolaunch now launches CRTs without booting to TR menu first
    * Allows diagnostics booting *without* full C64 functionality.
    * For example, can Autolaunch DesTestMax on a C64 with missing RAM chips!
  * Thanks for the ideas/input Factor of Matt (https://factorofmatt.com/)
* Bus Snoop (new feature):
  * Serial 'b' command to snoop bus (Addr, Data, R/nW) to insure all bits toggle (Debug tool)
    * Monitors 100,000 bus cycles and reads the Address & Data bus to log the state of each bit and sends a summary out the USB Serial port.  
    * This will help identify "stuck bits" on the bus, a failure that won't allow any thing to run/display, and is hard to narrow down without an oscilloscope and a block of time.
  * In the future, this feature can grow into a detailed bus analyzer with buffer/filters, or monitor the bus for SID writes to do visualizations and other SID logging/recording. Lots of possibilities...
* HW Fab 0.3 preparation
  * Disallow FW downgrade below this version for Fab 0.3 PCBs
  * Determines HW Fab # based on Dot-Clk read routine
* Timing/compatibility improvement:
  * New timing param: nS_VICDHold (365nS Default), separated out from nS_DataHold (non-VIC cycle)
  * nS_DataHold (non-vic hold) increased from 365 to 390nS
    * To accommodate Mega65 and Reloaded Mk II by default without special FW needed
    * Lots of validation testing done to ensure cross compatibility.  Thanks for all the help @DigitalMan
  * RW Ready Delay on by default in EEPROM (this FW resets EEPROM to defaults)
* Aesthetic/information only:
  * Bottom of settings menu changed to git URL from Travis/Sensorium
  * C64 Splash screen updated to 2025
  * Deprecated old Form based Win App
  * Removed #define nfcScanner (now permanent part of build)
  * Doc/help screen updates for NFC random tag

### 0.6.3_Mega65 special Release 2024/12/04
* Special FW release for Mega65 machines running the C64 Core
  * Also fine for other machines, please communicate if any issues found

### 0.6.3 Release 2024/12/02
* ASID Player updates:
  * Framework for new ASID packets: Reg Write Order/timing, Control/Framerate, and SID Types
  * ASID Player Frame timer select T/t for up/down list
* Bug Fix:  C64 Clock was running slow in 50Hz regions
* Built-in menu file updates:
  * Additions:
    * DesTestMAX:  Desmond's RAM Test added to test/diag menu 
      * with permission from Matt matt@factorofmatt.com
    * SID-Wizard V1.92 + Sequential MIDI
    * Jupiter Lander (Cracked version)
  * Removed YYZ from menu, conflicts w/ TR app as of 0.6.2 (will be back)
  * File re-arrangement, MIDI/ASID programs in sub-dir
* Hot keys 1-5 updated to new path/locations
* fBusSnoop hook in ISR for future bus snooping functions
* Support for future PCB revision: Data buffer dir control via pin28, fixes UltiMax on C128
* Serial command updates:
  * 'v' serial command for checking FW Ver, build date, temp, free mem
  * 'f' cmd (debug) Shows number of menu files

### 0.6.2 Release 2024/10/06
* New feature: *Custom BASIC Commands*
  * Communicate with/though your TeensyROM from new BASIC language commands
  * TSAVE, TLOAD, TDIR, TPUT, TGET, plus many other commands available from C64 BASIC
  * See [BASIC commands usage document](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/Custom_BASIC_Commands.md) for details
  * Demo video available [here.](https://youtu.be/5qShZjLOG5s)
  * Thank you to @hExx and @Avrilcadabra for the brainstorming!
* New feature: *Selectable Auto-Launch on startup*
  * Allows launch of any file/program on power-up
    * Great for diagnostics execution and headless opperation
  * Shift-Return sets Auto-Launch to currently highlighted file
    * File can be from any source (SD/USB/TR)
  * Disable from settings menu
  * After auto-boot, menu button to return to main menu.
  * Hold reset on power-up to skip (until LED comes on)
  * Thank you to @misterfox and @Avricadabra for the recommendation!
* ASID player improvements:
  * New feature: *Individual voice muting (1/2/3)*
    * Voice # group red when muted
    * Allows user to hear individual voices to "disect" ASID input
    * Thank you to @Avricadabra for the idea
  * Bit bucket reg to send Ctl Reg of muted voices
  * SID address up *or down* via function keys F1/F2, F3/F4, F5/F6
* Documentation updates:
  * Help screen: added left arrow, shift-ret, and 1-4
  * ASID player [usage doc](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/ASID_Player.md)
  * [BASIC commands usage document](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/Custom_BASIC_Commands.md)
* Other fixes/improvements:
  * Doing full BASIC init before PRG launch
    * Previous partial init caused some programs to launch incorrectly (Thanks to @Mad for identifying)
  * File access on boot improvements:
    * USBFileSystemWait to make sure USB is on line before accessing on boot
    * SDFullInit retries SD.begin if media presence detected
  * Start TOD clock earlier in startup seq in case of remote/auto launch
  * Fix row number to print "Done" for Set main background SID
  * No longer printing all screen messages to serial (unless Debug flag is set)
  * ASM build batch files updated to add "PROGMEM" via custom bin2header.py
  * Note: EEPROM setting will reset with this update

### 0.6.1 Release 2024/09/16
* ASID Player usage documentation created, [available here](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/ASID_Player.md)
* ASID player updates (v1.1)
  * Added customized character set  to make indicators round, bar graph characters, and dot for zero value on spinners
  * Beta feature added:  Frame timer to re-time ASID playback for timing improvement
    * Usage documented in ASID Player doc linked above
    * Commands added to enable/disable timer and set buffer size
    * Micro timing adjustments of no more than 1uS per 12 frames to keep buffer primed but not overflowed
    * Buffer fill level displayed on screen
    * 50Hz 1x and Auto modes selectable
      * In auto mode, the playback interval will be measured at beginning and used as a seed time for the rest of the playback.
        * This method works fairly well, but is dependent on USB packet intervals which are occasionally a bit off (thus the need for the timer). 
        * In the future, DeepSID (and perhaps others) will provide a "recipe" packet to specify the exact interval and eliminate the need to time the packets and improve seed accuracy.
          * DeepSID uses Web MIDI, which appears to have the worst jitter. SID Factory II, ASIDXP, and SIDPlay are *much* better
        * 1x and 2x speed SIDs (PAL or NTSC) play reliably, 4x usually has a couple small packet errors during the full song, but typically not audibly detectable.  
        * Above 4x and other asynchronous formats don't do well with the frame timer. Leave it "off" for these
      * 50Hz 1x mode works best for SIDs of that speed and seed the timer with an exact value regardless of timed value
    * Thank you to @hExx, @Avrilcadabra, @tubesockor, @iZero, and @Slaygon for you testing/feedback/ideas around this feature!
* MIDI2SID Freq calc corrected for PAL/NTSC 
  * Based on system typed detected
  * Thank you @Avrilcadabra for the prompt

### 0.6 Release 2024/07/10
* New feature: Dual boot w/ minimal image
  * ***Increases max CRT file size from 626k to 875k (40% increase)***
    * ~40 known games in this range, such as Briley Witch 
  * Automatically launches minimal boot image when a file in this range is selected
  * Return to main menu by pushing the button on the TeensyROM
  * Build instructions created for full dual-boot hex file creation
  * limitations
    * Files of this size load from SD card only
    * NFC disabled while loaded, push button to return/re-enable
  * Thanks to @DigitalMan for the prompts on this!
* SID sub-tune navigation capability
  * +/- for next/prev on SID summary page
    * Displays song # and # songs
  * SID starts on sub-song identified in SID header
  * Serial command for setting remotely
  * Thanks to @hExx, @Avrilcadabra, and @DivertigO for the prompts!

### 0.5.16 sub-release 2024/07/03
* MIDI/ASID updates
  * Hosted MIDI Device changed to "BigBuffer" model
    * Now supports DirtyWave m8 tracker/sequencer
  * Added $d500 and $d600 to available SID addresses in ASID player
* General optimization
  * NFC poling auto-paused during Swiftlink, MIDI, and ASID operations
    * Avoids slow-down of these features when NFC is enabled 
    * NFC (if enabled) will not respond when these apps are running
      * Use button to return to main menu and resume NFC poling/usage

### 0.5.15 sub-release 2024/05/24
* New feature: Integrated high performance ASID Player
  * TeensyROM specific implementation to accelerate data streaming and ASID packet decoding
    * No dropouts or slowdowns due to decoding limitations
  * Smoothly stream data from ChipSynthC64, DeepSID, or other ASID sources.
  * Play streams with up to 3 SIDs played simultaneously
    * Individually selectable address for each SID on system
  * Displays text messages embedded in ASID data  
  * Enter via main TeensyROM menu, or use Hot key '4' to enter immediately.
  * Easy to use interface
    * Real time indicators for Each SID and SID1 individual register writes
      * Decoder screen shows description of each register indicator
    * Help screen shows all available keyboard commands
    * Screen enable option for maximum noise reduction
    * Mute playback capability
* New feature: .D64, .D71, and .D81 file type support
  * Single file load only, multi-load not supported.
* TeensyROM UI support
  * GoodSIDToken send via serial on succesful SID load
  * File open for writing retry in GetFileStream (by [**MetalHexx**](https://github.com/MetalHexx))
* Other additions/fixes
  * .OCP and .PIC graphic file viewer support (Art Studio files, used by OneLoad64)
  * MIDI2SID prg source code added to repo
  * Fix: 3 main menu filenames changed for NFC tag path compatibility
  * Fix: Current file path retained on menu restart when loading default SID from SD/USB

### 0.5.14 sub-release 2024/03/28
* New feature: Selectable default/background SID
  * Select/play any SID normally, then 's' from SID Info screen (F6) to set it as default background SID
    * Selection stored in EEPROM for future boot recollection
  * SID can be TR built-in or from SD/USB
    * If file not present on boot, loads "Sleep Dirt" from TR
  * Feature requested by: ][avok
* NFC Loading System Updates:
  * Added ability create/read NFC tags for built-in TeensyROM files (in addition to SD/USB)
    * Uses TR: path prefix to designate
  * Feature requested by: Richard
* New feature: Hot Keys to launch some programs
  * Available from any main "Src:" menu screen:
    * '1': Cynthcart
    * '2': Station64
    * '3': CCGMS
  * Feature requested by: Niclashoyer
* General updates
  * "Also sprach Zarathustra" and "When I'm 64" added to TR built-in SIDs
  * F4 in Setting menu toggles SID playback (same as 'j')
  * Fixed race condition when starting Cynthcart in PAL mode with NFC enabled.
  * Note: All EEPROM settings will be reset with this update

### 0.5.13 sub-release 2024/03/06
* NFC Loading System Updates:
  * Tag write improvements:
    * Prompt to remove nfc tag after writing to prevent auto-execute
    * General messaging clarification
    * Checks card type to reject Mifare Classic
    * Always re-init and clear last UID after write operation
    * Retries on tag verification read before write
  * NFC re-initialized (if enabled) on menu button press
    * Easier way to force init than power cycle, also insures reader stays in synch
    * Note: If NFC is enabled but not attached to USB, power-up and button pushes will be slow to respond
  * Check for "C64" at start of path and remove if path not present.  
    * Keeps compatibility with TapTo w/ System ID
  
### 0.5.12 sub-release 2024/02/23
* New feature: NFC Loading System!
  * Use NFC tags to instantly launch you CRTs/PRGs/SIDs/etc.
  * Write new tags right from your C64
  * See documentation [here](/docs/NFC_Loader.md).
  * A huge thank you to [**StatMat**](https://github.com/Stat-Mat) for sharing his vision and support
  * Thanks also to the TapTo project for the inspiration! 
* General updates
  * Skipping RAM test at boot for faster boot and reduced screen garbage time.
    * As used in the OneLoad64 collection, code provided by [**StatMat**](https://github.com/Stat-Mat)
  * EEPROM setting reset to defaults
    * Hold menu button for 10 seconds until LED starts flashing
    * Upon release, setting will be reset and TeensyROM is rebooted
  * Settings Menu 
    * "Reboot TeensyROM" option to apply changed defaults without power-cycle
    * "NFC Enabled" to enable attached NFC reader
    * "RW Ready Dly" to improve Hi-ROM game graphics on Reloaded MKII and C64c
      * Thanks to **alterationx10** for the testing!
    * Free RAM now displayed to indicate max CRT file size
  * YYZ.sid edited to *not* zero out time registers on SID init
    * Thank you to **][avok** for reporting this issue
* Remote Launch UI support updates
  * Readback game preview file additions by [**MetalHexx**](https://github.com/MetalHexx)

### 0.5.11 sub-release 2024/02/06
* New feature: Picture viewer
  * Koala multi-color and Art Studio Hi-res files viewable/supported
  * File Extension association:
    * .kla, .koa:  Koala multi-color
    * .art, .aas, .hpi: Art Studio Hi-Res
  * Compatible with output from [Retropixels online](https://www.micheldebree.nl/retropixels/)
    * Create a C64 viewable file from any source format:
      * Drag source picture in to Retropixels site
      * Adjust picture parameters & types
      * Save as Koala Painter or Art Studio
      * Transfer to TeensyROM SD or USB and select to view
  * Commands available while viewing:
    * '+' & '-' to view next/prev picture in directory
    * CRSR Up/Dn to change border color (+multicolor background)
    * Any other key to exit viewer
  * Added /Pictures dir to Main TR Menu w/ 16 sample pics
* Swiftlink/Browser updates:
  * Entity References now detected/parsed
    * \&gt; ('>') and \&nbsp; (' ') implemented
    * Other ERs ignored for now
  * HTML tag \<tr> = return added
* Remote Launch support updates
  * BadSIDToken sent on SID load error
* General/Housekeeping:
  * Updated nS_DataHold time from 350 to 365 to accommodate Reloaded MK2 board
    * Thanks to **alterationx10** for the testing!
  * Timing control via serial improvement (Dbg_SerTimChg)
  * Removed 2 redundant pic .prgs (Fractal/Emb Head)

### 0.5.10 sub-release 2024/01/13
* SID Player updates:
  * SID conflict check range reduced to $6000-70ff, set on compile
    * ~1000 additional SIDs now playable
  * RSID and Play address 0 support 
    * ~1300 additional SIDs now playable
  * About 90% of ~57k known SIDs now playable
  * Check for conflicts with IO1 space
  * Show play address on load screen
  * Show addr range, init addr, play addr, and clock type on SID info page    
  * Added YYZ (Rush) to SID Covers local menu
* Remote Launch updates from MetalHexx
  * CopyFileCommand and DeleteFileCommand via USB Serial
* Swiftlink/Browser updates:
  * Expanded/detailed to browser local help menu (? command)
* System Timezone update
  * Timezone UTC offset resolution now 30 minutes (VK5LN Michael recommendation)
  * hour offset range is -12.0 to +14.0  step 0.5
  * TZ will need adjusting from previous value after this update
* Housekeeping:
  * New IO1 regs: rwRegCodeStartPage, rwRegCodeLastPage (set on TR app start)
  * IO1 reg re-organization and size reduction
  * MIDI2SID recompile for reg change

### 0.5.9 sub-release 2023/12/27
* SID Player updates:
  * SID info page added
    * Shows current SID file info: Filename, Name, Author, Released, Clock
    * Current Machine info: PAL/NTSC and Time of Day clk frequency
    * Settings: 
      * CRSR keys: Adjust playback Speed
      * d: Set default playback speed
      * F4: Toggle SID On/Off
      * b: Border effect On/Off
  * Default playback speeds (based on machine and SID info) dialed in with oscilloscope
* Main menu changes:
  * F6 changed to Show SID info (was Settings menu)
  * F8 changed to Settings menu (was MIDI2SID)
  * Help screen (still F7) updated to reflect key changes
  * MIDI2SID app moved to stand-alone prg in Multimedia directory

### 0.5.8 sub-release 2023/12/13
* Swiftlink/Browser updates: ([Web Browser Usage](/docs/Browser_Usage.md) is updated)
  * 'd' command to list download directory contents
    * Select Link # to launch directly from browser
  * Overwrite option/prompt if requested download filename exists
  * Bookmark option 'bu' to list bookmark names w/ URLs
  * 'br# [name]' command added to rename bookmarks
  * Include Downloaded file links in history and bookmarks
  * "Digitalman TeensyROM Demo" link added as bookmark #2
  * Mem mgt improvements for direct launch
* Remote Launch updates:
  * Remote command/file/dir enhancements from [**MetalHexx**](https://github.com/MetalHexx)
    * To support new Remote Command GUI, in development by MetalHexx
  * Launch with forced reset ability added
    * enables remote launch at any time or launch from another app
* "SID Cover Tunes" directory and Files added to main TR menu

### 0.5.7 sub-release 2023/11/21
* Remote Launch updates:
  * Command to Remotely pause/unpause SID playback
  * IRQ command incorporated to protect from false IRQs

### 0.5.6 sub-release 2023/11/19
* New feature: Remote Launch
  * Ability to launch stored files remotely via USB connection 
    * includes SID playback, PRGs, CRTs, etc 
  * Works with updated Win App (0.4 just released)
    * Look for massively improved v2 Win App from hExx in the future :)
  
* Swiftlink
  * Bug fix: wasn't sending 0s in normal mode (corrupted xmodem downloads)
  * Browser mode minor html tag tweaks
  
### 0.5.5 sub-release 2023/11/12
* Swiftlink/Browser updates:
  * Port selection available for host or ip address (host:port/path)
  * Char count for pause Improvements
  * command summary all lower case to make readable in upper/gfx mode
  * Force lower case for Browser command list

### 0.5.4 sub-release 2023/11/8
* Note: This FW release resets stored EEPROM settings to defaults
* Swiftlink/Browser updates:
  * Bookmark favorite sites for quick access later.
  * Previously visited sites saved in a browsing queue
  * Download files directly to specified path on USB or SD 
  * TinyWeb premier, downloads and <petscii %9b> type tags

### 0.5.3 sub-release 2023/10/16
* Swiftlink updates:
  * ATBROWSE command to enter Browser mode from teminal program such as CCGMS
  * Links enumerated per page
  * www.frogfind.com filterring all pages/searches
  * PETSCII conversion improvement
  * Browser Mode Commands implemented
    * S <Term>: Web Search
    * U <URL>: Go to URL
    * #1-9: Jump to link #
    * Return: Continue when paused
    * X: Exit Browser mode

* SID Player
  * Compensate for non-standard SID load address

### 0.5.2 sub-release 2023/10/10
* SID Player
  * Determining Vid standard (NTSC/PAL) and mains freq (50/60Hz) on start
  * Changed SID play interrupt from raster to timer based
  * '+' and '-' to change SID speed from main Menu
  * Playback speed set based on SID and Machine type
  * stopped border color tweak for now since IRQ is not raster
  * Banking out BASIC and Kernal during SID play and init
  * check for SID/TR mem conflict (eventually enable TR code relocation)
  * SID file type association/selectability

* Swiftlink updates:
  * ATSEARCH command initial implementation (more to come)
  * Using www.frogfind.com to get search results

* Arhitecture/General:
  * Changed back to 8k cart from 16k for main menu
  * Changed C64 RAM location from $2400 to $6000
  * Loading SID from Flash after main app startup instead of transfer from cart
  * Removed USB Host menu since file x-fer is direct to USB/SD
  * Menu tweak: F7 for help instead of Space
  * Removed IO1 regs rRegStrAddrLo/Hi, just get from stream
  * First Self modifying code (smc) for smcSIDPlay and smcSIDInit

### 0.5.1 sub-release 2023/10/1
* MIDI CC messaging now passthrough/absolute instead of relative/calculated
* Enabled USB MIDI Device In messages: 
  * ControlChange, ProgramChange, and PitchChange
* Revamped Windows File Transfer app support (see [WinApp Release History](../WinApp/WinApp_Release_History.md))

## 0.5: built 2023/09/17
* Main UI  improvements:
  * Many ROMs added to main menu (with room for more)
  * 1 level sub-dirs from TR Mem menu
  * Cursor based navigation/selection
  * Joystick 2 menu control: Joystick speed in Settings/EEPROM
  * Display parent dir Path at top
  * Display page number and num pages in Menu/directory
  * alphabetize the directory list   
  * Home key for top of current directory
  * Search in dir for first letter a-z
  * Up arrow for up directory
  * Associate special IO HW with Teensy mem menu items: '+' next to type on TR Mem Menu
  * Longer file names and directory sizes
  * Messaging added for load/xfer/parse opperations
  * Settings menu re-do
  * Help Menu added, space to access   Fx options live in Help Menu
    
* New File/type support:
  * .P00: now supported
  * .CRT: added support for the following types: EasyFlash, Magic Desk, Ocean, Dinamic, Zaxxon/Super Zaxxon, Game System 3, SuperGames, FunPlay/PowerPlay
    * Note: max file size ~650MB, no EasyFlash eapi support

* Swiftlink/Ethernet improvements:
  * Modified built-in CCGMS to start with Swiftlink/38.4k by default
  * added "at?" to list all AT commands
  * upper or lower case accepted in ANSCII or Graphics modes
  * correct backspace handling

* MIDI improvements: 
  * disabled some unused usbDevMIDI(in) commands causing probs using Cakewalk with Sta64/Cynthcart

## 0.4: built 2023/07/29
* Enabled TeensyROM as a MIDI USB Device. 
  * USB MIDI Host is still present, can use both at the same time.
* Update TeensyROM Firmware from SD card or USB Drive. 

## 0.3: built 2023/07/15
   
## 0.2: 2023/03/16
   
## 0.1: 2023/02/09
   
## Initial commit: 2023/01/11/23