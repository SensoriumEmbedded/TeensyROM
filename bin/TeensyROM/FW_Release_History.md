
# FW Release Version history:

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