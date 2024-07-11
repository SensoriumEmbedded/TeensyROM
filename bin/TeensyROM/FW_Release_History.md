
# FW Release Version history:

### 0.6 Release 7/10/24
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

### 0.5.16 sub-release 7/3/24
* MIDI/ASID updates
  * Hosted MIDI Device changed to "BigBuffer" model
    * Now supports DirtyWave m8 tracker/sequencer
  * Added $d500 and $d600 to available SID addresses in ASID player
* General optimization
  * NFC poling auto-paused during Swiftlink, MIDI, and ASID operations
    * Avoids slow-down of these features when NFC is enabled 
    * NFC (if enabled) will not respond when these apps are running
      * Use button to return to main menu and resume NFC poling/usage

### 0.5.15 sub-release 5/24/24
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

### 0.5.14 sub-release 3/28/24
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

### 0.5.13 sub-release 3/6/24
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
  
### 0.5.12 sub-release 2/23/24
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

### 0.5.11 sub-release 2/6/24
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

### 0.5.10 sub-release 1/13/23
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

### 0.5.9 sub-release 12/27/23
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

### 0.5.8 sub-release 12/13/23
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

### 0.5.7 sub-release 11/21/23
* Remote Launch updates:
  * Command to Remotely pause/unpause SID playback
  * IRQ command incorporated to protect from false IRQs

### 0.5.6 sub-release 11/19/23
* New feature: Remote Launch
  * Ability to launch stored files remotely via USB connection 
    * includes SID playback, PRGs, CRTs, etc 
  * Works with updated Win App (0.4 just released)
    * Look for massively improved v2 Win App from hExx in the future :)
  
* Swiftlink
  * Bug fix: wasn't sending 0s in normal mode (corrupted xmodem downloads)
  * Browser mode minor html tag tweaks
  
### 0.5.5 sub-release 11/12/23
* Swiftlink/Browser updates:
  * Port selection available for host or ip address (host:port/path)
  * Char count for pause Improvements
  * command summary all lower case to make readable in upper/gfx mode
  * Force lower case for Browser command list

### 0.5.4 sub-release 11/8/23
* Note: This FW release resets stored EEPROM settings to defaults
* Swiftlink/Browser updates:
  * Bookmark favorite sites for quick access later.
  * Previously visited sites saved in a browsing queue
  * Download files directly to specified path on USB or SD 
  * TinyWeb premier, downloads and <petscii %9b> type tags

### 0.5.3 sub-release 10/16/23
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

### 0.5.2 sub-release 10/10/23
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

### 0.5.1 sub-release 10/1/23
* MIDI CC messaging now passthrough/absolute instead of relative/calculated
* Enabled USB MIDI Device In messages: 
  * ControlChange, ProgramChange, and PitchChange
* Revamped Windows File Transfer app support (see [WinApp Release History](../WinApp/WinApp_Release_History.md))

## 0.5: built 9/17/23  14:01:48
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

## 0.4: built 7/29/23  22:56:39
* Enabled TeensyROM as a MIDI USB Device. 
  * USB MIDI Host is still present, can use both at the same time.
* Update TeensyROM Firmware from SD card or USB Drive. 

## 0.3: built 7/15/23  18:56:01
   
## 0.2: 3/16/23
   
## 0.1: 2/9/23
   
## Initial commit: 1/11/23