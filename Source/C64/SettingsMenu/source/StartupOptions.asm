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



StartupOptionsMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgStartupOptionsMenu
   ldy #>MsgStartupOptionsMenu
   jsr PrintString 
   
WaitStartupOptionsMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitStartupOptionsMenuKey

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
   jmp WaitStartupOptionsMenuKey   
   
MsgStartupOptionsMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Startup Options ", ChrReturn, ChrReturn

   !tx EscC,EscNameColor,  "Upon Main Menu Startup:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "   Synch RTC via net:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "   Play selected SID:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "i", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   " Enable TCP Listener:", ChrReturn, ChrReturn
   !tx EscC,EscNameColor,  "Upon TeensyROM Startup:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "m", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Auto-Launch File:", ChrReturn

   !tx 0 
