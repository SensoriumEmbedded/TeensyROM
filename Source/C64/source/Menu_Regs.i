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

   MaxItemNameLength = 28

   ;;;;;;;;;;;;;;;;;;  start IO1_Registers  ;;;;;;;;;;;;;;;;;;;;;;;;;
   
   rRegStatus          =  0 ;// Busy when doing SD/USB access.  note: loc 0(DE00) gets written to at reset
   rRegStrAddrLo       =  1 ;// lo byte of start address of the prg file being transfered to mem
   rRegStrAddrHi       =  2 ;// Hi byte of start address
   rRegStrAvailable    =  3 ;// zero when inactive/complete 
   rRegStreamData      =  4 ;// next byte of data to transfer, auto increments when read
   wRegControl         =  5 ;// RegCtlCommands: execute specific functions
   rRegPresence1       =  6 ;// for HW detect: 0x55
   rRegPresence2       =  7 ;// for HW detect: 0xAA
   rRegLastHourBCD     =  8 ;// Last TOD Hours read
   rRegLastMinBCD      =  9 ;// Last TOD Minutes read
   rRegLastSecBCD      = 10 ;// Last TOD Seconds read
   rWRegCurrMenuWAIT   = 11 ;// enum RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItem        = 12 ;// select Menu Item for name, type, execution, etc
   rRegNumItems        = 13 ;// num items in menu list
   rRegItemType        = 14 ;// regItemTypes: type of item 
   rwRegPwrUpDefaults  = 15 ;// power up default reg, see bit mask defs
   rwRegTimezone       = 16 ;// signed char for timezone: UTC +/-12 
   rwRegNextIOHndlr    = 17 ;// Which IO handler will take over upone exit/execute/emulate
   rwRegFWUpdStatCont  = 18 ;// FW update Status/Control, see RegFWUpdCommands
   rwRegSerialString   = 19 ;// Write selected item (RegSerialStringSelect) to select/reset, then Serially read out until 0 read.

   StartSIDRegs        = 20 ;// start of SID Regs, matching SID Reg order ($D400)
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
   
;enum RegPowerUpDefaultMasks
   rpudMusicMask     = 0x01 ; rwRegPwrUpDefaults bit 0=music on
   rpudNetTimeMask   = 0x02 ; rwRegPwrUpDefaults bit 1=synch net time

;enum RegStatusTypes  //rRegStatus
   rsChangeMenu         = 0x00  ;
   rsStartItem          = 0x01  ;
   rsGetTime            = 0x02  ;
   rsIOHWinit           = 0x03  ;C64 code is executing transfered PRG, change IO1 handler
   rsWriteEEPROM        = 0x04  ;
   rsMakeBuildCPUInfoStr= 0x05  ;
   
   rsNumStatusTypes     = 0x06  ;

   rsReady              = 0x5a
   ;rsError              = 0x48,

;enum RegMenuTypes //must match TblMsgMenuName order/qty
   rmtSD        = 0
   rmtTeensy    = 1
   rmtUSBHost   = 2
   rmtUSBDrive  = 3
  
;enum RegFWUpdCommands
   rFWUSCC64Message       = 0x5a ; //message for the C64, set to continue when finished
   rFWUSCC64Finish        = 0xa5 ; //update finished (done, abort, or otherwise)
   rFWUSCContinue         = 0x3c ; //Tells the FW to continue with update
  
;enum RegCtlCommands
   rCtlVanishROM          = 0
   rCtlBasicReset         = 1
   rCtlStartSelItemWAIT   = 2
   rCtlGetTimeWAIT        = 3
   rCtlRunningPRG         = 4 ; final signal before running prg, allows IO1 handler change
   rCtlMakeInfoStrWAIT    = 5 ; MakeBuildCPUInfoStr

;enum regItemTypes //synch with TblItemType
   rtNone      = 0
   rtUnknown   = 1
   rtBin16k    = 2
   rtBin8kHi   = 3
   rtBin8kLo   = 4
   rtBinC128   = 5
   rtDirectory = 6
   rtFilePrg   = 7
   rtFileCrt   = 8
   rtFileHex   = 9
   
   
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End Teensy matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
