; MIT License
; 
; Copyright (c) 2023 Travis Smith
; 
; Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
; and associated documentation files (the "Software"), to deal in the Software without 
; restriction, including without limitation the rights to use, copy, modify, merge, publish, 
; distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
; the Software is furnished to do so, subject to the following conditions:
; 
; The above copyright notice and this permission notice shall be included in all copies or 
; substantial portions of the Software.
; 
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
; BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
; NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
; DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


;!!!!!!!!!!!!!!!!!!!!These need to match Teensy Code: Menu_Regs.h !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   MaxItemDispLength = 35
   MaxItemsPerPage   = 19
   NumHotKeys        =  5


   ;enum IO1_Registers  //offset from 0xDE00
   ;skipping 0: Used by many others and accessed on reset
   rwRegStatus         =  1 ;// Indicates busy when waiting for FW to complete something
   rRegStrAvailable    =  2 ;// Stream PRG: zero when inactive/complete 
   rRegStreamData      =  3 ;// Stream PRG: next byte of data to transfer, auto increments when read
   wRegControl         =  4 ;// RegCtlCommands: execute specific functions
   rRegPresence1       =  5 ;// HW detect: 0x55
   rRegPresence2       =  6 ;// HW detect: 0xAA
   rRegLastHourBCD     =  7 ;// Last TOD: Hours read
   rRegLastMinBCD      =  8 ;// Last TOD: Minutes read
   rRegLastSecBCD      =  9 ;// Last TOD: Seconds read
   rWRegCurrMenuWAIT   = 10 ;// enum RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItemOnPage  = 11 ;// Item sel/info: (zero based) select Menu Item On Current Page for name, type, execution, etc
   rwRegCursorItemOnPg = 12 ;// Item sel/info: (zero based) Highlighted/cursor Menu Item On Current Page
   rRegNumItemsOnPage  = 13 ;// Item sel/info: num items on current menu page
   rwRegPageNumber     = 14 ;// Item sel/info: (one based) current page number
   rRegNumPages        = 15 ;// Item sel/info: total number of pages
   rRegItemTypePlusIOH = 16 ;// Item sel/info: regItemTypes: type of item, bit 7 indicates there's an assigned IOHandler (from TR mem menu) 
   rwRegMIDISettings   = 17 ;// EEPROM stored: MIDI Settings reg, see RegMIDISettingsMasks
   rwRegMIDISettings2  = 18 ;// EEPROM stored: MIDI Settings reg #2, see RegMIDISettingsMasks2
   rwRegPwrUpDefaults  = 19 ;// EEPROM stored: power up default reg, see RegPowerUpDefaultMasks
   rwRegPwrUpDefaults2 = 20 ;// EEPROM stored: power up default reg#2, see RegPowerUpDefaultMasks2
   rwRegTimezone       = 21 ;// EEPROM stored: signed char for timezone: UTC +/-12 
   rwRegNextIOHndlr    = 22 ;// EEPROM stored: Which IO handler will take over upone exit/execute/emulate
   rwRegSerialString   = 23 ;// Write selected item (RegSerialStringSelect) to select/reset, then Serially read out until 0 read.
   wRegSearchLetterWAIT= 24 ;// Put cursor on first menu item with letter written
   rRegSIDInitHi       = 25 ;// SID Play Info: Init address Hi
   rRegSIDInitLo       = 26 ;// SID Play Info: Init Address Lo
   rRegSIDPlayHi       = 27 ;// SID Play Info: Play Address Hi
   rRegSIDPlayLo       = 28 ;// SID Play Info: Play Address Lo
   rRegSIDDefSpeedHi   = 29 ;// SID Play Info: Default CIA interrupt timer speed Hi
   rRegSIDDefSpeedLo   = 30 ;// SID Play Info: Default CIA interrupt timer speed Lo
   rwRegSIDCurSpeedHi  = 31 ;// SID Play Info: Current CIA interrupt timer speed Hi
   rwRegSIDCurSpeedLo  = 32 ;// SID Play Info: Current CIA interrupt timer speed Lo
   wRegSIDSpeedChange  = 33 ;// SID Play Control: Change speed as indicated by RegSIDSpeedChanges
   rwRegSIDSongNumZ    = 34 ;// SID Play Info: Current Song Number (Zero Based)
   rRegSIDNumSongsZ    = 35 ;// SID Play Info: Number of Songs in SID (Zero Based)
   wRegVid_TOD_Clks    = 36 ;// C64/128 Video Standard and TOD clock frequencies
   wRegIRQ_ACK         = 37 ;// IRQ Ack from C64 app
   rwRegIRQ_CMD        = 38 ;// IRQ Command from TeensyROM
   rwRegCodeStartPage  = 39 ;// TR Code Start page in C64 RAM
   rwRegCodeLastPage   = 40 ;// TR Code last page used in C64 RAM
   rwRegScratch        = 41 ;// Bi-Directional Scratch Register
   rwRegColorRefStart  = 42 ;// Color ref transfer eeprom<->C64, WAIT on Write
                            ;//offsets defined in enum ColorRefOffsets
   ;NextReg = rwRegColorRefStart+NumColorRefs,

   ; These are used for the MIDI2SID app, keep in synch or make separate handler
   StartSIDRegs        = 64 ;// start of SID Regs, matching SID Reg order ($D400)
   rRegSIDFreqLo1      = StartSIDRegs +  0 
   rRegSIDFreqHi1      = StartSIDRegs +  1
   rRegSIDDutyLo1      = StartSIDRegs +  2
   rRegSIDDutyHi1      = StartSIDRegs +  3
   rRegSIDVoicCont1    = StartSIDRegs +  4
   rRegSIDAttDec1      = StartSIDRegs +  5
   rRegSIDSusRel1      = StartSIDRegs +  6
                       
   rRegSIDFreqLo2      = StartSIDRegs +  7
   rRegSIDFreqHi2      = StartSIDRegs +  8
   rRegSIDDutyLo2      = StartSIDRegs +  9
   rRegSIDDutyHi2      = StartSIDRegs + 10
   rRegSIDVoicCont2    = StartSIDRegs + 11
   rRegSIDAttDec2      = StartSIDRegs + 12
   rRegSIDSusRel2      = StartSIDRegs + 13
                       
   rRegSIDFreqLo3      = StartSIDRegs + 14
   rRegSIDFreqHi3      = StartSIDRegs + 15
   rRegSIDDutyLo3      = StartSIDRegs + 16
   rRegSIDDutyHi3      = StartSIDRegs + 17
   rRegSIDVoicCont3    = StartSIDRegs + 18
   rRegSIDAttDec3      = StartSIDRegs + 19
   rRegSIDSusRel3      = StartSIDRegs + 20
                       
   rRegSIDFreqCutLo    = StartSIDRegs + 21
   rRegSIDFreqCutHi    = StartSIDRegs + 22
   rRegSIDFCtlReson    = StartSIDRegs + 23
   rRegSIDVolFltSel    = StartSIDRegs + 24
   EndSIDRegs          = StartSIDRegs + 25
                       
   rRegSIDStrStart     = StartSIDRegs + 26
   ;  9: 3 chars per voice (oct, note, shrp)
   ;  1: Out of voices indicator
   ;  3: spaces betw
   ; 14 total w// term:  ON# ON# ON# X
   rRegSIDOutOfVoices  = StartSIDRegs + 38
   rRegSIDStringTerm   = StartSIDRegs + 39

   IO1Size             = StartSIDRegs + 40  ;//last entry, sets size
   ;;;;;;;;;;;;;;;;;;  end IO1_Registers  ;;;;;;;;;;;;;;;;;;;;;;;;;

;enum RegIRQCommands       //rwRegIRQ_CMD
   ricmdNone           = 0 ; no command, always 0 (init)
   ricmdAck1           = 1 ; Ack1 response from C64 IRQ routine
   ricmdLaunch         = 2 ; Launch app (set up before IRQ assert)
   ricmdSIDPause       = 3 ; SID pause/play
   ricmdSIDInit        = 4 ; re-init current SID (sub song # change)
   ricmdSetSIDSpeed    = 5 ; Apply CIA timer reg values (rRegSIDCurSpeedHi/Lo)
   ricmdSIDVoiceMute   = 6 ; Apply SID Voice Mute Settings

;enum ColorRefOffsets       //Order matches TblEscC:
   EscBackgndColor     = 0 ; Black   Screen Background
   EscBorderColor      = 1 ; Purple  Screen Border
   EscTRBannerColor    = 2 ; Purple  Top of screen banner color
   EscTimeColor        = 3 ; Orange  Time Display & Waiting msg
   EscOptionColor      = 4 ; Yellow  Input key option indication
   EscSourcesColor     = 5 ; LtBlue  General text/descriptions
   EscNameColor        = 6 ; LtGreen FIle names & other text
   NumColorRefs        = 7 ; Number of color references

   ;local use only, could re-factor for permanent change:
   EscMenuMiscColor    = EscNameColor    ; Was Green: Menu alt color
   EscTypeColor        = EscSourcesColor ; Was Blue : File types(only)

;enum  RegSIDSpeedChanges  //wRegSIDSpeedChange
   rsscIncMajor        = 1 ; inc major % units
   rsscDecMajor        = 2 ; dec major % units
   rsscIncMinor        = 3 ; inc minor % units
   rsscDecMinor        = 4 ; dec minor % units
   rsscSetDefault      = 5 ; Set Default Speed
   rsscToggleLogLin    = 6 ; Toggle control type
  
;enum RegSerialStringSelect // rwRegSerialString
   rsstItemName        = 0  ; Name of selected item
   rsstNextIOHndlrName = 1  ; IOHandler Name selected in rwRegNextIOHndlr
   rsstSerialStringBuf = 2  ; build SerialStringBuf prior to selecting
   rsstVersionNum      = 3  ; version string for main banner 
   rsstShortDirPath    = 4  ; printable current path
   rsstSIDInfo         = 5  ; Info on last SID loaded
   rsstMachineInfo     = 6  ; Info on current machine vid/TOD clk (set when SID loaded)
   rsstSIDSpeed        = 7  ; Current SID playback speed
   rsstSIDSpeedCtlType = 8  ; Current SID Speed Control Type (Log/Lin)
   
;enum RegPowerUpDefaultMasks
   rpudSIDPauseMask    = 0b00000001 ; rwRegPwrUpDefaults bit 0, 1=SID music paused
   rpudNetTimeMask     = 0b00000010 ; rwRegPwrUpDefaults bit 1, 1=synch RTC to net time
   rpudShowExtension   = 0b00000100 ; rwRegPwrUpDefaults bit 2, 1=show file extensions
   rpudClock12_24hr    = 0b00001000 ; rwRegPwrUpDefaults bit 3, 1=24 hour clock displayed
   rpudJoySpeedMask    = 0b11110000 ; rwRegPwrUpDefaults bits 4-7=Joystick2 speed setting

;enum RegPowerUpDefaultMasks2
   ;bit 0 unused
   rpud2HostSerCtlMask    = 0b00000110 ; mask of all host serial control devices
   rpud2HostSerCtlMaskInv = 0b11111001 ; Inverted mask of all host serial control devices
   rpud2NFCEnabled        = 0b00000010 ; rwRegPwrUpDefaults2 bit 1, 1=NFC Enabled
   rpud2TRContEnabled     = 0b00000100 ; rwRegPwrUpDefaults2 bit 2, 1=TRCont Enabled
   rpud2TRAutoLaunch      = 0b01000000 ; rwRegPwrUpDefaults2 bit 6, 1=Auto-Launch Enabled
   rpud2TRTCPListen       = 0b10000000 ; rwRegPwrUpDefaults2 bit 7, 1=TCP Listen Enabled
   ;bits 4:3 for future hosted serial devices(?)
   ;bit 5 unused

;enum RegMIDISettingsMasks
   ;eepAdMIDISettings, rwRegMIDISettings
   rMIDISetNoteOffOnEn           = 0b00000001 ; 
   rMIDISetAfterTouchPolyEn      = 0b00000010 ; 
   rMIDISetControlChangeEn       = 0b00000100 ; 
   rMIDISetProgramChangeEn       = 0b00001000 ; 
   rMIDISetAfterTouchEn          = 0b00010000 ; 
   rMIDISetPitchChangeEn         = 0b00100000 ; 
   rMIDISetSystemExclusiveEn     = 0b01000000 ; 
   rMIDISetTimeCodeQuarterFrameEn= 0b10000000 ; 

;enum RegMIDISettingsMasks2
   ;eepAdMIDISettings2, rwRegMIDISettings2
   rMIDISet2SongPositionEn       = 0b00000001 ; 
   rMIDISet2SongSelectEn         = 0b00000010 ; 
   rMIDISet2TuneRequestEn        = 0b00000100 ; 
   rMIDISet2RealTimeSystemEn     = 0b00001000 ; 
   ;bits 7:4 unused, initialized to 0

;enum RegStatusTypes  //rwRegStatus, match StatusFunction order
   rsChangeMenu         = 0x00  ;
   rsStartItem          = 0x01  ;
   rsSetRTCfromNet      = 0x02  ;
   rsC64TODfromRTC      = 0x03  ;
   rsIOHWSelInit        = 0x04  ;C64 code is executing transferred PRG, change IO1 handler
   rsWriteEEPROM        = 0x05  ;
   rsMakeBuildCPUInfoStr= 0x06  ;
   rsUpDirectory        = 0x07  ;
   rsSearchForLetter    = 0x08  ;
   rsLoadSIDforXfer     = 0x09  ;
   rsNextPicture        = 0x0a  ;
   rsLastPicture        = 0x0b  ;
   rsWriteNFCTagCheck   = 0x0c  ;
   rsWriteNFCTag        = 0x0d  ;
   rsNFCReEnable        = 0x0e  ;
   rsSetBackgroundSID   = 0x0f  ;
   rsSetAutoLaunch      = 0x10  ;
   rsClearAutoLaunch    = 0x11  ; no longer used
   rsNextTextFile       = 0x12  ;
   rsLastTextFile       = 0x13  ;
   rsIOHWNextInit       = 0x14  ; no longer used
   rsMountDxxFile       = 0x15  ;
   rsHotKeySetLaunch    = 0x16  ;
   rsNetListenInit      = 0x17  ;
   rsSetKERNALBin       = 0x18  ;
   rsKERNALPreStart     = 0x19  ;
   rsSetREUFile         = 0x1a  ;
   rsMakeFilenameStr    = 0x1b  ;
   rsRTCAdjust          = 0x1c  ;
   
   rsNumStatusTypes     = 0x1d  ;

   rsReady              = 0x5a  ;//FW->64 (Rd) update finished (done, abort, or otherwise)
   rsC64Message         = 0xa5  ;//FW->64 (Rd) message for the C64, set to continue when finished
   rsContinue           = 0xc3  ;//64->FW (Wr) Tells the FW to continue with update

;enum RegMenuTypes //must match TblMsgMenuName order/qty, also used by UI/serial for DriveType
   rmtUSBDrive  = 0
   rmtSD        = 1
   rmtTeensy    = 2
   
   rmtNumTypes  = 3
    
;enum RegCtlCommands
   rCtlVanishROM            =  0
   rCtlBasicReset           =  1 ; No longer Used!
   rCtlStartSelItemWAIT     =  2
   rCtlSetRTCfromNetWAIT    =  3 ; Synchs the Teensy RTC with time acquired from Ethernet
   rCtlC64TODfromRTCWAIT    =  4 ; Sets the IO1 TOD regs with the current RTC Time
   rCtlRunningPRG           =  5 ; final signal before running prg, allows IO1 handler change
   rCtlMakeInfoStrWAIT      =  6 ; MakeBuildCPUInfoStr
   rCtlUpDirectoryWAIT      =  7
   rCtlLoadSIDWAIT          =  8 ; load .sid file to RAM buffer and prep for x-fer
   rCtlNextPicture          =  9 
   rCtlLastPicture          = 10 
   rCtlRebootTeensyROM      = 11 
   rCtlWriteNFCTagCheckWAIT = 12
   rCtlWriteNFCTagWAIT      = 13
   rCtlNFCReEnableWAIT      = 14
   rCtlSetBackgroundSIDWAIT = 15
   rCtlSetAutoLaunchWAIT    = 16
   rCtlClearAutoLaunchWAIT  = 17
   rCtlNextTextFile         = 18
   rCtlLastTextFile         = 19
   rCtlMountDxxFileWAIT     = 20
   rCtlHotKeySetLaunch      = 21
   rCtlNetListenInitWAIT    = 22
   rCtlSetKERNALBinWAIT     = 23
   rCtlKERNALPreStartWAIT   = 24
   rCtlSetREUFileWAIT       = 25
   rCtlReturnToMainMenu     = 26
   
   rCtlMakeStrWAIT_First    = 27 ; FIRST of a linear series that uses MakeFilenameStr
      rCtlMakeKernalStrWAIT    = 27 
      rCtlMakeREUStrWAIT       = 28 
      rCtlMakeSIDStrWAIT       = 29 
      rCtlMakeAutoLStrWAIT     = 30 
      rCtlMakeHotKey1WAIT      = 31 
      rCtlMakeHotKey2WAIT      = 32 
      rCtlMakeHotKey3WAIT      = 33 
      rCtlMakeHotKey4WAIT      = 34 
      rCtlMakeHotKey5WAIT      = 35 
   rCtlMakeStrWAIT_Last     = 35 ; LAST of a linear series that uses MakeFilenameStr

   rCtlRTCAdjWAIT_First     = 36; FIRST of a linear series that uses RTCAdjust
      rCtlRTCAdj_Hrs_Up_WAIT   = 36
      rCtlRTCAdj_Hrs_Dn_WAIT   = 37
      rCtlRTCAdj_Min_Up_WAIT   = 38
      rCtlRTCAdj_Min_Dn_WAIT   = 39
      rCtlRTCAdj_Sec_Up_WAIT   = 40
      rCtlRTCAdj_Sec_Dn_WAIT   = 41
   rCtlRTCAdjWAIT_Last      = 41; LAST of a linear series that uses RTCAdjust


;enum regItemTypes //synch with TblItemType
   rtNone        = 0
   rtUnknown     = 1
   rtDirectory   = 2 
   rtD64         = 3
   rtD71         = 4  
   rtD81         = 5  
   rtFilePrg     = 6    ;//always first valid executable file type
   rtFileCrt     = 7  
   rtFileHex     = 8  
   rtFileP00     = 9  
   rtFileSID     = 10 
   rtFileKla     = 11 
   rtFileArt     = 12  
   rtFileTxt     = 13
   rtFilePETSCII = 14
   rtBin16k      = 15  
   rtBin8kHi     = 16  
   rtBin8kLo     = 17  
   rtBinC128     = 18 
   rtFileREU     = 19

   IOH_None      = 0  ;only part of enumIOHandlers used...
   
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End Teensy matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
