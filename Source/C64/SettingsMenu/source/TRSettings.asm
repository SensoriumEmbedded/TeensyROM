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



TRSettings:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgTRSettings
   ldy #>MsgTRSettings
   jsr PrintString 

;print kernal file name:
   lda #rCtlMakeKernalStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ldx #6 ;row
   ldy #2 ;col
   clc
   jsr SetCursor
   lda TblEscC+EscMenuMiscColor
   sta $0286  ;set text color
   jsr PrintSerialStringLoaded

;print REU file name:
   lda #rCtlMakeREUStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ldx #9 ;row
   ldy #2 ;col
   clc
   jsr SetCursor
   lda TblEscC+EscMenuMiscColor
   sta $0286  ;set text color
   jsr PrintSerialStringLoaded



ShowTRSettings:
 
WaitTRSettingsKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitTRSettingsKey

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
   jmp WaitTRSettingsKey   
   
MsgTRSettings:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: TeensyROM General ", ChrReturn, ChrReturn
   
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a/A", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Special IO:", ChrReturn

   !tx EscC,EscSourcesColor,  " Kernal replace file:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  " REU Pre-load file:", ChrReturn, ChrReturn, ChrReturn

   !tx EscC,EscNameColor,  " User Interface:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b/B", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "     Joystick2 Speed:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "g", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Show File Extensions:", ChrReturn

   !tx 0 
