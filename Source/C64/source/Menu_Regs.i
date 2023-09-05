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

   MaxItemDispLength = 32
   MaxItemsPerPage   = 16

   ;;;;;;;;;;;;;;;;;;  start IO1_Registers  ;;;;;;;;;;;;;;;;;;;;;;;;;
   
   ;skipping 0: Used by many others and accessed on reset
   rwRegStatus         =  1 ;// Indicates busy when waiting for FW to complete something
   rRegStrAddrLo       =  2 ;// Stream PRG: lo byte of start address of the prg file being transfered to mem
   rRegStrAddrHi       =  3 ;// Stream PRG: Hi byte of start address
   rRegStrAvailable    =  4 ;// Stream PRG: zero when inactive/complete 
   rRegStreamData      =  5 ;// Stream PRG: next byte of data to transfer, auto increments when read
   wRegControl         =  6 ;// RegCtlCommands: execute specific functions
   rRegPresence1       =  7 ;// HW detect: 0x55
   rRegPresence2       =  8 ;// HW detect: 0xAA
   rRegLastHourBCD     =  9 ;// Last TOD: Hours read
   rRegLastMinBCD      = 10 ;// Last TOD: Minutes read
   rRegLastSecBCD      = 11 ;// Last TOD: Seconds read
   rWRegCurrMenuWAIT   = 12 ;// enum RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItemOnPage  = 13 ;// Item sel/info: (zero based) select Menu Item On Current Page for name, type, execution, etc
   rwRegCursorItemOnPg = 14 ;// Item sel/info: (zero based) Highlighted/cursor Menu Item On Current Page
   rRegNumItemsOnPage  = 15 ;// Item sel/info: num items on current menu page
   rwRegPageNumber     = 16 ;// Item sel/info: (one based) current page number
   rRegNumPages        = 17 ;// Item sel/info: total number of pages
   rRegItemTypePlusIOH = 18 ;// Item sel/info: regItemTypes: type of item, bit 7 indicates there's an assigned IOHandler (from TR mem menu) 
   rwRegPwrUpDefaults  = 19 ;// EEPROM stored: power up default reg, see RegPowerUpDefaultMasks
   rwRegTimezone       = 10 ;// EEPROM stored: signed char for timezone: UTC +/-12 
   rwRegNextIOHndlr    = 21 ;// EEPROM stored: Which IO handler will take over upone exit/execute/emulate
   rwRegSerialString   = 22 ;// Write selected item (RegSerialStringSelect) to select/reset, then Serially read out until 0 read.

   StartSIDRegs        = 23 ;// start of SID Regs, matching SID Reg order ($D400)
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

   ;;;;;;;;;;;;;;;;;;  end IO1_Registers  ;;;;;;;;;;;;;;;;;;;;;;;;;

;enum RegSerialStringSelect // rwRegSerialString
   rsstItemName        = 0
   rsstNextIOHndlrName = 1
   rsstSerialStringBuf = 2
   rsstVersionNum      = 3
   rsstShortDirPath    = 4
   
;enum RegPowerUpDefaultMasks
   rpudMusicMask     = 0x01 ; rwRegPwrUpDefaults bit 0=music on
   rpudNetTimeMask   = 0x02 ; rwRegPwrUpDefaults bit 1=synch net time

;enum RegStatusTypes  //rwRegStatus, match StatusFunction order
   rsChangeMenu         = 0x00  ;
   rsStartItem          = 0x01  ;
   rsGetTime            = 0x02  ;
   rsIOHWinit           = 0x03  ;C64 code is executing transfered PRG, change IO1 handler
   rsWriteEEPROM        = 0x04  ;
   rsMakeBuildCPUInfoStr= 0x05  ;
   rsUpDirectory        = 0x06  ;

   rsNumStatusTypes     = 0x07  ;

   rsReady              = 0x5a  ;//FW->64 (Rd) update finished (done, abort, or otherwise)
   rsC64Message         = 0xa5  ;//FW->64 (Rd) message for the C64, set to continue when finished
   rsContinue           = 0xc3  ;//64->FW (Wr) Tells the FW to continue with update

;enum RegMenuTypes //must match TblMsgMenuName order/qty
   rmtSD        = 0
   rmtTeensy    = 1
   rmtUSBHost   = 2
   rmtUSBDrive  = 3
    
;enum RegCtlCommands
   rCtlVanishROM          = 0
   rCtlBasicReset         = 1
   rCtlStartSelItemWAIT   = 2
   rCtlGetTimeWAIT        = 3
   rCtlRunningPRG         = 4 ; final signal before running prg, allows IO1 handler change
   rCtlMakeInfoStrWAIT    = 5 ; MakeBuildCPUInfoStr
   rCtlUpDirectoryWAIT    = 6

;enum regItemTypes //synch with TblItemType
   rtNone      = 0
   rtUnknown   = 1
   rtBin16k    = 2
   rtBin8kHi   = 3
   rtBin8kLo   = 4
   rtBinC128   = 5
   rtDirectory = 6
   ;file extension matching:
   rtFilePrg   = 7
   rtFileCrt   = 8
   rtFileHex   = 9
   rtFileP00   = 10
   
   
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End Teensy matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
