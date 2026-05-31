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

MsgBanner1:  ;set color before clearing for char poke default  
   !tx EscC,EscNameColor, ChrClear, EscC,EscTRBannerColor, ChrToLower, ChrRvsOn, EscC,EscArgSpaces+7, 0
MsgBanner2:  
   !tx " Settings", 0
   

;
;MsgSIDInfo1:
;   !tx ChrReturn, EscC,EscSourcesColor, "SID Info Page:", ChrReturn, ChrReturn
;   !tx " File Information for", EscC,EscNameColor  ;EscC,EscMenuMiscColor
;   !tx 0
;MsgSIDInfo2:
;   !tx ChrReturn, ChrReturn, EscC,EscSourcesColor, " This Machine: ", EscC,EscNameColor
;   !tx 0
;MsgSIDInfo3:
;   !tx "0Hz TOD", ChrReturn
;   !tx 0
;   

MsgMenuPageSelections:
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "<= CRSR =>", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Next/Previous page", EscC,EscNameColor, " ("
   !tx 0 
MsgMenuExitSelection:
   !tx ")", ChrReturn, EscC,EscArgSpaces+7, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Space", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Exit to Main menu"
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
