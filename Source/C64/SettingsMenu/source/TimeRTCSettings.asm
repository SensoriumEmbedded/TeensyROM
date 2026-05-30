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



TimeRTCMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgTimeRTCMenu
   ldy #>MsgTimeRTCMenu
   jsr PrintString 
   
WaitTimeRTCMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitTimeRTCMenuKey

;+  cmp #'1' ;increment color parameter number 
;   bmi +   ;skip if below '1'
;   cmp #'1'+NumColorRefs
;   bpl +   ;skip if above NumColorRefs
;   sec       ;set to subtract without carry
;   sbc #'1'   ;make zero based
;   tax
;   ldy TempTblEscC, x
;   iny
   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitTimeRTCMenuKey   
   
MsgTimeRTCMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Time/Real Time Clock ", ChrReturn, ChrReturn
   !tx EscC,EscNameColor,  " Format/Location:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "12/24 hour clock:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Local Time Zone:", EscC,EscNameColor, " UTC", ChrReturn
   
   !tx ChrReturn, EscC,EscNameColor,  " RTC Adjustment:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Synch RTC via Ethernet now", ChrReturn   
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RTC Minutes U/D", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RTC Hours U/D", ChrReturn
   !tx 0 
