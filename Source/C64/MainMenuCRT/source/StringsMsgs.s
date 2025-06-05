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
   !tx EscC,EscNameColor, ChrClear, EscC,EscTRBannerColor, ChrToLower, ChrRvsOn, "             TeensyROM ", 0
MsgSource:    
   !tx EscC,EscSourcesColor, "Src: ", 0 
MsgMainOptions1:
   !tx ChrRvsOn, EscC,EscOptionColor, "F1", ChrRvsOff, EscC,EscSourcesColor,  " Teensy Mem  "
   !tx ChrRvsOn, EscC,EscOptionColor, "F5", ChrRvsOff, EscC,EscSourcesColor,  " USB Drive  "
   !tx EscC,EscMenuMiscColor, "Pg " 
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
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1/2/3", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Mute/Unmute Voice #: ", ChrRvsOn,"123", ChrReturn ;123 colors get set in PrintVoiceMutes 
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
   !tx EscC,EscSourcesColor, " github.com/SensoriumEmbedded/TeensyROM"
   !tx 0

MsgFWVerify:
   !tx ChrReturn, ChrReturn, EscC,EscOptionColor, "Please Confirm:"
   !tx ChrReturn, "Update TeensyROM FirmWare? y/n "
   !tx 0

MsgFWInProgress:
   !tx EscC,EscNameColor, "Yes", ChrReturn, ChrReturn
   !tx "Firmware update in progress!", ChrReturn
   !tx "It will take about 2 minutes to complete" ;, ChrReturn
   !tx "DO NOT TURN OFF POWER WHILE UPDATING!!!", ChrReturn, EscC,EscMenuMiscColor
   !tx 0

MsgFWUpdateFailed:
   !tx EscC,EscOptionColor, ChrReturn, "FW Update failed"
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
   !tx "Failed ", 0
MsgDone:
   !tx EscC,EscNameColor, "Done", 0
MsgHasHandler:
   !tx ChrCRSRLeft, "+", 0

TblEscC:  ;order matches enum ColorRefOffsets
          ;string escape token (EscC) next character cross-reference
        ;Local Default     EEPROM default  Description
   !byte PokeBlack       ; PokeBlack      ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeDrkGrey     ; PokePurple     ;EscBorderColor      = 1 ; Screen Border
   !byte PokeDrkGrey     ; PokePurple     ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeWhite       ; PokeOrange     ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeLtGrey      ; PokeYellow     ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeDrkGrey     ; PokeLtBlue     ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeMedGrey     ; PokeLtGreen    ;EscNameColor        = 6 ; FIle names & other text

TblItemType: ;must match regItemTypes (rtNone, rtBin16k, etc) order!
   ;4 bytes each, no term
   !tx 0, "   "  ; rtNone        = 0
   !tx 0, "Unk"  ; rtUnknown     = 1
   !tx 0, "Dir"  ; rtDirectory   = 2 
   !tx 0, "D64"  ; rtD64         = 3
   !tx 0, "D71"  ; rtD71         = 4  
   !tx 0, "D81"  ; rtD81         = 5  
   !tx 0, "Prg"  ; rtFilePrg     = 6    //alway first valid executable file type
   !tx 0, "Crt"  ; rtFileCrt     = 7  
   !tx 0, "Hex"  ; rtFileHex     = 8  
   !tx 0, "P00"  ; rtFileP00     = 9  
   !tx 0, "SID"  ; rtFileSID     = 10 
   !tx 0, "Kla"  ; rtFileKla     = 11 
   !tx 0, "Art"  ; rtFileArt     = 12  
   !tx 0, "Txt"  ; rtFileTxt     = 13  
   !tx 0, "Seq"  ; rtFilePETSCII = 14  
   !tx 0, "16k"  ; rtBin16k      = 15  
   !tx 0, "8Hi"  ; rtBin8kHi     = 16  
   !tx 0, "8Lo"  ; rtBin8kLo     = 17  
   !tx 0, "128"  ; rtBinC128     = 18 
