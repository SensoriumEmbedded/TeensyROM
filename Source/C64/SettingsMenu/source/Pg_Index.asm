; MIT License
; 
; Copyright (c) 2026 Travis Smith
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



IndexMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgIndexMenu
   ldy #>MsgIndexMenu
   jsr PrintString 

   ;update static settings

ShowIndexSettings:
   ;update dynamic settings
   
WaitIndexMenuKey:
   ;main wait loop
   jsr DisplayTime   
   jsr GetIn    
   beq WaitIndexMenuKey



+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitIndexMenuKey   
   
MsgIndexMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config/Info Page Index ", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " Jump to page:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "This Index Page", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "2", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Config: TeensyROM General", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "3", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Config: Startup Options", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "4", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Config: Menu Colors", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "5", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Config: MIDI Message Filters", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "6", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Config: Time Format/RTC", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "7", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Info: General", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "8", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Info: Ethernet", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "9", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Info: HotKeys", ChrReturn

   !tx 0 
