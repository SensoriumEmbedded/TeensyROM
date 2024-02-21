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
   !tx ChrRvsOn, OptionColor, "F1", ChrRvsOff, SourcesColor,  " Teensy Mem  "
   !tx ChrRvsOn, OptionColor, "F5", ChrRvsOff, SourcesColor,  " USB Drive  "
   !tx ChrLtRed, "Pg " 
   !tx 0
;page x/y printed here
MsgMainOptions2:
   !tx ChrReturn
   !tx ChrRvsOn, OptionColor, "F3", ChrRvsOff, SourcesColor,  " SD Card     "
   !tx ChrRvsOn, OptionColor, "F7", ChrRvsOff, MenuMiscColor,  " Help"
   !tx 0
   
MsgSIDInfo1:
   !tx ChrReturn, SourcesColor, "SID Info Page:", ChrReturn, ChrReturn
   !tx " File Information for", NameColor  ;MenuMiscColor
   !tx 0
MsgSIDInfo2:
   !tx ChrReturn, ChrReturn, SourcesColor, " This Machine:", NameColor, ChrReturn, "  "
   !tx 0
MsgSIDInfo3:
   !tx "0Hz TOD", ChrReturn
   !tx ChrReturn, SourcesColor, " Settings:", ChrReturn
   !tx "   ", OptionColor, ChrFillRight, ChrRvsOn, "CRSR", ChrRvsOff, ChrFillLeft, SourcesColor, "Adjust Play Speed: ", NameColor,"0x", ChrReturn   
   !tx "      ", OptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, SourcesColor, "Default Play Speed", ChrReturn
   !tx "     ", OptionColor, ChrFillRight, ChrRvsOn, "F4", ChrRvsOff, ChrFillLeft, SourcesColor, "Toggle SID On/Off", ChrReturn
   !tx "      ", OptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, SourcesColor, "Border Effect On/Off", ChrReturn, ChrReturn
   ;!tx " return to main menu, other menus?", ChrReturn
   !tx 0
   
MsgHelpMenu:   
   !tx ChrReturn, SourcesColor, "Help Menu:", ChrReturn
   !tx ChrReturn, " Main Menu Navigation:", ChrReturn
   !tx    "  ", OptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "U/D", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Cursor up/dn", ChrReturn
   !tx    "  ", OptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "L/R", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Page up/dn", ChrReturn
   !tx "     ", OptionColor, ChrFillRight, ChrRvsOn, "Return/Fire", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Select file or dir", ChrReturn, ChrReturn
   !tx "     ", OptionColor, ChrFillRight, ChrRvsOn, ChrUpArrow, ChrRvsOff, ChrFillLeft, MenuMiscColor, "Up directory", ChrReturn
   !tx   "   ", OptionColor, ChrFillRight, ChrRvsOn, "a-z", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Next entry starting with letter", ChrReturn
   !tx    "  ", OptionColor, ChrFillRight, ChrRvsOn, "Home", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Beginning of current dir", ChrReturn
   !tx ChrReturn, ChrReturn
   !tx SourcesColor, " Available here and on Main Menu:", ChrReturn
   !tx "   Source Select:   ", MenuMiscColor, "Other:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "F1", ChrRvsOff, ChrFillLeft, SourcesColor,  "Teensy Mem"
   !tx   "  ", OptionColor, ChrFillRight, ChrRvsOn, "F2", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Exit to BASIC", ChrReturn
               
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "F3", ChrRvsOff, ChrFillLeft, SourcesColor,  "SD Card   "
   !tx   "  ", OptionColor, ChrFillRight, ChrRvsOn, "F4", ChrRvsOff, ChrFillLeft, MenuMiscColor, "SID on/off", ChrReturn
               
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "F5", ChrRvsOff, ChrFillLeft, SourcesColor,  "USB Drive "
   !tx   "  ", OptionColor, ChrFillRight, ChrRvsOn, "F6", ChrRvsOff, ChrFillLeft, MenuMiscColor, "SID Information", ChrReturn
               
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "F7", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Help      "
   !tx   "  ", OptionColor, ChrFillRight, ChrRvsOn, "F8", ChrRvsOff, ChrFillLeft, MenuMiscColor, "Settings Menu", ChrReturn
   !tx ChrReturn, ChrReturn
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
   !tx OptionColor, "    Key=next value, shift+key=previous", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, SourcesColor, "  Time Zone:", NameColor, " UTC", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, SourcesColor, " Special IO:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "c", ChrRvsOff, ChrFillLeft, SourcesColor, " Joy2 Speed:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, SourcesColor, " Synch Time:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, SourcesColor, "   Play SID:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, SourcesColor, "NFC Enabled:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "g", ChrRvsOff, ChrFillLeft, SourcesColor, "RW Read Dly:", ChrReturn
   !tx MenuMiscColor, "   Immediate:", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "h", ChrRvsOff, ChrFillLeft, SourcesColor, "Re-boot TeensyROM", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "i", ChrRvsOff, ChrFillLeft, SourcesColor, "Synch Time via Ethernet", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "j", ChrRvsOff, ChrFillLeft, SourcesColor, "Pause/Play SID", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "k", ChrRvsOff, ChrFillLeft, SourcesColor, "Self Test", ChrReturn
   !tx "    ", OptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, SourcesColor, "Help Menu", ChrReturn
   ;!tx ChrReturn
MsgSettingsMenu2SpaceRet:
   !tx "  ", OptionColor, ChrFillRight, ChrRvsOn, "Space", ChrRvsOff, ChrFillLeft, SourcesColor,  "Back to Main menu", ChrReturn
   !tx 0 
MsgSettingsMenu3:
   !tx ChrReturn, ChrPurple   ;MenuMiscColor 
   !tx "      2024 by Travis S @ Sensorium"
   ;, ChrReturn
   ;!tx NameColor, " TeensyROM is 100% Open Source HW & SW!", ChrReturn
   ;!tx " github.com/SensoriumEmbedded/TeensyROM"
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
   !tx ChrReturn, OptionColor, "Press any key to return"
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
MsgHasHandler:
   !tx ChrCRSRLeft, AssignedIOHColor, "+", 0
   
TblItemType: ;must match regItemTypes (rtNone, rtBin16k, etc) order!
   ;4 bytes each, no term
   !tx NameColor, "   "  ; rtNone      = 0,
   !tx ChrDrkGrey,"Unk"  ; rtUnknown   = 1,
   !tx TypeColor, "16k"  ; rtBin16k    = 2,
   !tx TypeColor, "8Hi"  ; rtBin8kHi   = 3,
   !tx TypeColor, "8Lo"  ; rtBin8kLo   = 4,
   !tx TypeColor, "128"  ; rtBinC128   = 5,
   !tx TypeColor, "Dir"  ; rtDirectory = 6,
   ;file extension matching:
   !tx TypeColor, "Prg"  ; rtFilePrg   = 7,
   !tx TypeColor, "Crt"  ; rtFileCrt   = 8,
   !tx TypeColor, "Hex"  ; rtFileHex   = 9,
   !tx TypeColor, "P00"  ; rtFileP00   = 10,
   !tx TypeColor, "SID"  ; rtFileSID   = 11,
   !tx TypeColor, "Kla"  ; rtFileKla   = 12,
   !tx TypeColor, "Art"  ; rtFileArt   = 13,
