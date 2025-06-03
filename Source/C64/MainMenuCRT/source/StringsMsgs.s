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
   
   
; ******************************* Strings/Messages ******************************* 

MsgBanner:  ;set color before clearing for char poke default  
   !tx EscC,EscNameColor, ChrClear, ChrPurple, ChrToLower, ChrRvsOn, "             TeensyROM ", 0
MsgSource:    
   !tx EscC,EscSourcesColor, "Src: ", 0 
MsgMainOptions1:
   !tx ChrRvsOn, EscC,EscOptionColor, "F1", ChrRvsOff, EscC,EscSourcesColor,  " Teensy Mem  "
   !tx ChrRvsOn, EscC,EscOptionColor, "F5", ChrRvsOff, EscC,EscSourcesColor,  " USB Drive  "
   !tx ChrLtRed, "Pg " 
   !tx 0
;page x/y printed here
MsgMainOptions2:
   !tx ChrReturn
   !tx ChrRvsOn, EscC,EscOptionColor, "F3", ChrRvsOff, EscC,EscSourcesColor,  " SD Card     "
   !tx ChrRvsOn, EscC,EscOptionColor, "F7", ChrRvsOff, EscC,EscMenuMiscColor,  " Help"
   !tx 0

MsgWriteNFCTag:
   !tx ChrReturn, EscC,EscSourcesColor, "Write NFC Tag:", ChrReturn
   !tx 0
   
MsgPlaceNFCTag:
   !tx ChrReturn, EscC,EscOptionColor, " Place tag in NFC reader,"
   !tx ChrReturn, "  then press any key to write", ChrReturn, EscC,EscSourcesColor
   !tx 0

MsgRemoveNFCTag:
   !tx ChrReturn, ChrReturn, EscC,EscOptionColor, "Remove tag from reader, then"
   !tx 0

MsgSetAutoLaunch:
   !tx ChrReturn, EscC,EscSourcesColor, "Set Power-up Auto Launch:", ChrReturn
   !tx 0

MsgSIDInfo1:
   !tx ChrReturn, EscC,EscSourcesColor, "SID Info Page:", ChrReturn, ChrReturn
   !tx " File Information for", EscC,EscNameColor  ;EscC,EscMenuMiscColor
   !tx 0
MsgSIDInfo2:
   !tx ChrReturn, ChrReturn, EscC,EscSourcesColor, " This Machine: ", EscC,EscNameColor
   !tx 0
MsgSIDInfo3:
   !tx "0Hz TOD", ChrReturn
   !tx ChrReturn, EscC,EscSourcesColor, " Settings:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "+/-", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Sub-Song Number:", ChrReturn   
   !tx "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Adj Play Speed:", ChrReturn   
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Speed Ctrl Type:", ChrReturn   
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Default Play Speed", ChrReturn
   !tx "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "p/F4", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Play/Pause SID", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1/2/3", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Mute/Unmute Voice #: ", EscC,EscNameColor, ChrRvsOn,"123", ChrReturn
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Border Effect On/Off", ChrReturn
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "s", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Set main background SID", ChrReturn
   !tx 0
   
MsgHelpMenu:   
   !tx ChrReturn, EscC,EscSourcesColor, "Help Menu:", ChrReturn
   !tx ChrReturn, " Main Menu Navigation:", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "U/D", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Cursor up/dn", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "L/R", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Page up/dn", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Return/Fire", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Select file or dir", ChrReturn, ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrUpArrow, ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Up directory", ChrReturn
   !tx   "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a-z", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Next entry starting with letter", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Home", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Beginning of current dir", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrLeftArrow, ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor,    "Write NFC tag: Highlighted File", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrQuestionMark, ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Write NFC tag: Random in Dir", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "A", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Set Auto-Launch to Highlighted", ChrReturn
   !tx   "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1-5", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Hot Keys (see gen usage doc)", ChrReturn
   !tx ChrReturn
   !tx EscC,EscSourcesColor    ;, " Available here and on Main Menu:", ChrReturn
   !tx "   Source Select:   ", EscC,EscMenuMiscColor, "Other:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F1", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Teensy Mem"
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F2", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Exit to BASIC", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F3", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "SD Card   "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F4", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "SID on/off", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F5", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "USB Drive "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F6", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "SID Information", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F7", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Help      "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F8", ChrRvsOff, ChrFillLeft, EscC,EscMenuMiscColor, "Settings Menu", ChrReturn
   !tx ChrReturn
   !tx 0

MsgNoHW:
   !tx ChrReturn, ChrReturn, ChrToLower, ChrYellow, "TeensyROM hardware not detected!!!", ChrReturn, 0

TblMsgMenuName: ;must match enum RegMenuTypes order/qty
   !word MsgMenuSD
   !word MsgMenuTeensy
   !word MsgMenuUSBDrive
MsgMenuSD:
   !tx "SD Card", 0
MsgMenuTeensy:
   !tx "Teensy Mem", 0
MsgMenuUSBDrive:
   !tx "USB Drive", 0

MsgSettingsMenu1:
   !tx ChrReturn, EscC,EscSourcesColor, "Settings Menu:", ChrReturn
   !tx EscC,EscMenuMiscColor 
   !tx "   Power-On Defaults:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "12/24hr clk:", EscC,EscNameColor, " 12", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a/A", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Time Zone:", EscC,EscNameColor, " UTC", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b/B", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Special IO:", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Joy2 Speed:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Synch Time:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "   Play SID:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "NFC Enabled:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "g", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RW Read Dly:", ChrReturn
   !tx EscC,EscMenuMiscColor, "   Immediate:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "h", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Re-boot TeensyROM", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "i", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Synch Time via Ethernet", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "j", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Pause/Play SID", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "k", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Self Test", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Disable Auto-Launch", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "m", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Help Menu", ChrReturn
   ;!tx ChrReturn
MsgSettingsMenu2SpaceRet:
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Space", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Back to Main menu", ChrReturn
   !tx 0 
MsgSettingsMenu3:
   !tx ChrBlue," github.com/SensoriumEmbedded/TeensyROM"
   !tx 0

MsgFWVerify:
   !tx ChrReturn, ChrReturn, ChrOrange, "Please Confirm:"
   !tx ChrReturn, "Update TeensyROM FirmWare? y/n "
   !tx 0

MsgFWInProgress:
   !tx ChrLtRed, "Yes", ChrReturn, ChrReturn
   !tx "Firmware update in progress!", ChrReturn
   !tx "It will take about 2 minutes to complete" ;, ChrReturn
   !tx "DO NOT TURN OFF POWER WHILE UPDATING!!!", ChrReturn, EscC,EscMenuMiscColor
   !tx 0

MsgFWUpdateFailed:
   !tx ChrLtRed, ChrReturn, "FW Update failed"
   !tx 0

MsgAnyKey:
   !tx ChrReturn, EscC,EscOptionColor, "Press any key to return"
   !tx 0

MsgOn:
   !tx "On ", 0
MsgOff:
   !tx "Off", 0
MsgWaiting:
   !tx EscC,EscTimeColor, " Waiting:", 0
MsgTesting:
   !tx EscC,EscNameColor, "Testing", 0
MsgPass:
   !tx "Passed ", 0
MsgFail:
   !tx ChrRed, "Failed ", 0
MsgDone:
   !tx EscC,EscNameColor, "Done", 0
MsgHasHandler:
   !tx ChrCRSRLeft, AssignedIOHColor, "+", 0

TblEscC:  ;string escape token (EscC) next character cross-reference
   !byte PokePurple    ;  EscBorderColor      = 0
   !byte PokeBlack     ;  EscBackgndColor     = 1
   !byte PokeWhite   ;PokeOrange    ;  EscTimeColor        = 2
   !byte PokeMedGrey ;PokeGreen     ;  EscEscC,EscMenuMiscColor    = 3
   !byte PokeLtGrey    ;  EscAssignedIOHColor = 4
   !byte PokeRed     ;PokeYellow    ;  EscOptionColor      = 5
   !byte PokeDrkGrey ;PokeLtBlue    ;  EscSourcesColor     = 6
   !byte PokeBlue      ;  EscTypeColor        = 7
   !byte PokeBrown   ;PokeLtGreen   ;  EscNameColor        = 8

TblItemType: ;must match regItemTypes (rtNone, rtBin16k, etc) order!
   ;4 bytes each, no term
   !tx TypeColor, "   "  ; rtNone        = 0
   !tx ChrDrkGrey,"Unk"  ; rtUnknown     = 1
   !tx TypeColor, "Dir"  ; rtDirectory   = 2 
   !tx TypeColor, "D64"  ; rtD64         = 3
   !tx TypeColor, "D71"  ; rtD71         = 4  
   !tx TypeColor, "D81"  ; rtD81         = 5  
   !tx TypeColor, "Prg"  ; rtFilePrg     = 6    //alway first valid executable file type
   !tx TypeColor, "Crt"  ; rtFileCrt     = 7  
   !tx TypeColor, "Hex"  ; rtFileHex     = 8  
   !tx TypeColor, "P00"  ; rtFileP00     = 9  
   !tx TypeColor, "SID"  ; rtFileSID     = 10 
   !tx TypeColor, "Kla"  ; rtFileKla     = 11 
   !tx TypeColor, "Art"  ; rtFileArt     = 12  
   !tx TypeColor, "Txt"  ; rtFileTxt     = 13  
   !tx TypeColor, "Seq"  ; rtFilePETSCII = 14  
   !tx TypeColor, "16k"  ; rtBin16k      = 15  
   !tx TypeColor, "8Hi"  ; rtBin8kHi     = 16  
   !tx TypeColor, "8Lo"  ; rtBin8kLo     = 17  
   !tx TypeColor, "128"  ; rtBinC128     = 18 
