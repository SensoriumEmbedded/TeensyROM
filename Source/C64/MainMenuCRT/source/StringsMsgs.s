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

MsgBanner:    
   !tx NameColor, ChrClear, ChrPurple, ChrToLower, ChrRvsOn, "             TeensyROM ", 0
MsgSource:    
   !tx SourcesColor, "Src: ", 0 
MsgMainOptions1:
   !tx ChrRvsOn, EscC,EscOptionColor, "F1", ChrRvsOff, SourcesColor,  " Teensy Mem  "
   !tx ChrRvsOn, EscC,EscOptionColor, "F5", ChrRvsOff, SourcesColor,  " USB Drive  "
   !tx ChrLtRed, "Pg " 
   !tx 0
;page x/y printed here
MsgMainOptions2:
   !tx ChrReturn
   !tx ChrRvsOn, EscC,EscOptionColor, "F3", ChrRvsOff, SourcesColor,  " SD Card     "
   !tx ChrRvsOn, EscC,EscOptionColor, "F7", ChrRvsOff, MenuMiscColor,  " Help"
   !tx 0

MsgWriteNFCTag:
   !tx ChrReturn, SourcesColor, "Write NFC Tag:", ChrReturn
   !tx 0
   
MsgPlaceNFCTag:
   !tx ChrReturn, EscC,EscOptionColor, " Place tag in NFC reader,"
   !tx ChrReturn, "  then press any key to write", ChrReturn, SourcesColor
   !tx 0

MsgRemoveNFCTag:
   !tx ChrReturn, ChrReturn, EscC,EscOptionColor, "Remove tag from reader, then"
   !tx 0

MsgSetAutoLaunch:
   !tx ChrReturn, SourcesColor, "Set Power-up Auto Launch:", ChrReturn
   !tx 0

MsgSIDInfo1:
   !tx ChrReturn, SourcesColor, "SID Info Page:", ChrReturn, ChrReturn
   !tx " File Information for", NameColor  ;MenuMiscColor
   !tx 0
MsgSIDInfo2:
   !tx ChrReturn, ChrReturn, SourcesColor, " This Machine: ", NameColor
   !tx 0
MsgSIDInfo3:
   !tx "0Hz TOD", ChrReturn
   !tx ChrReturn, SourcesColor, " Settings:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "+/-", ChrRvsOff, ChrFillLeft, SourcesColor, "Sub-Song Number:", ChrReturn   
   !tx "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR", ChrRvsOff, ChrFillLeft, SourcesColor, "Adj Play Speed:", ChrReturn   
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, SourcesColor, "Speed Ctrl Type:", ChrReturn   
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, SourcesColor, "Default Play Speed", ChrReturn
   !tx "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "p/F4", ChrRvsOff, ChrFillLeft, SourcesColor, "Play/Pause SID", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1/2/3", ChrRvsOff, ChrFillLeft, SourcesColor, "Mute/Unmute Voice #: ", NameColor, ChrRvsOn,"123", ChrReturn
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, SourcesColor, "Border Effect On/Off", ChrReturn
   !tx "      ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "s", ChrRvsOff, ChrFillLeft, SourcesColor, "Set main background SID", ChrReturn
   !tx 0
   
MsgHelpMenu:   
   !tx ChrReturn, SourcesColor, "Help Menu:", ChrReturn
   !tx ChrReturn, " Main Menu Navigation:", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "U/D", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Cursor up/dn", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "L/R", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Page up/dn", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Return/Fire", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Select file or dir", ChrReturn, ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrUpArrow, ChrRvsOff, ChrFillLeft, MenuMiscColor, "Up directory", ChrReturn
   !tx   "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a-z", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Next entry starting with letter", ChrReturn
   !tx    "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Home", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Beginning of current dir", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrLeftArrow, ChrRvsOff, ChrFillLeft, MenuMiscColor,    "Write NFC tag: Highlighted File", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrQuestionMark, ChrRvsOff, ChrFillLeft, MenuMiscColor, "Write NFC tag: Random in Dir", ChrReturn
   !tx "     ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "A", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Set Auto-Launch to Highlighted", ChrReturn
   !tx   "   ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1-5", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Hot Keys (see gen usage doc)", ChrReturn
   !tx ChrReturn
   !tx SourcesColor    ;, " Available here and on Main Menu:", ChrReturn
   !tx "   Source Select:   ", MenuMiscColor, "Other:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F1", ChrRvsOff, ChrFillLeft, SourcesColor,  "Teensy Mem"
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F2", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Exit to BASIC", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F3", ChrRvsOff, ChrFillLeft, SourcesColor,  "SD Card   "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F4", ChrRvsOff, ChrFillLeft, MenuMiscColor, "SID on/off", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F5", ChrRvsOff, ChrFillLeft, SourcesColor,  "USB Drive "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F6", ChrRvsOff, ChrFillLeft, MenuMiscColor, "SID Information", ChrReturn
               
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F7", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Help      "
   !tx   "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F8", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Settings Menu", ChrReturn
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
   !tx ChrReturn, SourcesColor, "Settings Menu:", ChrReturn
   !tx MenuMiscColor 
   !tx "   Power-On Defaults:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1", ChrRvsOff, ChrFillLeft, SourcesColor, "12/24hr clk:", NameColor, " 12", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a/A", ChrRvsOff, ChrFillLeft, SourcesColor, "  Time Zone:", NameColor, " UTC", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b/B", ChrRvsOff, ChrFillLeft, SourcesColor, " Special IO:", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, SourcesColor, " Joy2 Speed:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, SourcesColor, " Synch Time:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, SourcesColor, "   Play SID:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, SourcesColor, "NFC Enabled:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "g", ChrRvsOff, ChrFillLeft, SourcesColor, "RW Read Dly:", ChrReturn
   !tx MenuMiscColor, "   Immediate:", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "h", ChrRvsOff, ChrFillLeft, SourcesColor, "Re-boot TeensyROM", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "i", ChrRvsOff, ChrFillLeft, SourcesColor, "Synch Time via Ethernet", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "j", ChrRvsOff, ChrFillLeft, SourcesColor, "Pause/Play SID", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "k", ChrRvsOff, ChrFillLeft, SourcesColor, "Self Test", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, SourcesColor, "Disable Auto-Launch", ChrReturn
   !tx "    ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "m", ChrRvsOff, ChrFillLeft, SourcesColor, "Help Menu", ChrReturn
   ;!tx ChrReturn
MsgSettingsMenu2SpaceRet:
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Space", ChrRvsOff, ChrFillLeft, SourcesColor,  "Back to Main menu", ChrReturn
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
   !tx "DO NOT TURN OFF POWER WHILE UPDATING!!!", ChrReturn, MenuMiscColor
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
   !tx TimeColor, " Waiting:", 0
MsgTesting:
   !tx NameColor, "Testing", 0
MsgPass:
   !tx "Passed ", 0
MsgFail:
   !tx ChrRed, "Failed ", 0
MsgDone:
   !tx NameColor, "Done", 0
MsgHasHandler:
   !tx ChrCRSRLeft, AssignedIOHColor, "+", 0

TblEscC:  ;string escape token (EscC) next xharacter cross-reference
   !byte PokePurple    ;  EscBorderColor      = 0
   !byte PokeBlack     ;  EscBackgndColor     = 1
   !byte PokeOrange    ;  EscTimeColor        = 2
   !byte PokeGreen     ;  EscMenuMiscColor    = 3
   !byte PokeLtGrey    ;  EscAssignedIOHColor = 4
   !byte PokeRed  ;PokeYellow    ;  EscOptionColor      = 5
   !byte PokeLtBlue    ;  EscSourcesColor     = 6
   !byte PokeBlue      ;  EscTypeColor        = 7
   !byte PokeLtGreen   ;  EscNameColor        = 9

TblItemType: ;must match regItemTypes (rtNone, rtBin16k, etc) order!
   ;4 bytes each, no term
   !tx NameColor, "   "  ; rtNone        = 0
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
