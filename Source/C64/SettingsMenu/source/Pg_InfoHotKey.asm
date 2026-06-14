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



InfoHotKeyMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgInfoHotKeyMenu
   ldy #>MsgInfoHotKeyMenu
   jsr PrintString 

   lda #rCtlMakeKernalStrWAIT
   ldx #5 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeKernalStrWAIT
   ldx #8 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeKernalStrWAIT
   ldx #11 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeKernalStrWAIT
   ldx #14 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeKernalStrWAIT
   ldx #17 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeInfoStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ldx #19 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   lda TblEscC+EscMenuMiscColor
   sta $0286  ;set text color
   lda #rsstSerialStringBuf ; Build info from rCtlMakeInfoStrWAIT
   jsr PrintSerialString

ShowInfoHotKeySettings:
   ;update dynamic settings
   
WaitInfoHotKeyMenuKey:
   ;main wait loop
   jsr DisplayTime   
   jsr GetIn    
   beq WaitInfoHotKeyMenuKey

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
   jmp WaitInfoHotKeyMenuKey   
   
MsgInfoHotKeyMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Info: HotKey/General", ChrReturn, ChrReturn
   ;!tx EscC,EscNameColor,  "HotKey file assignments:", ChrReturn
   
   !tx EscC,EscSourcesColor,  "  Hot Key #1:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  "  Hot Key #2:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  "  Hot Key #3:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  "  Hot Key #4:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  "  Hot Key #5:", ChrReturn, ChrReturn, ChrReturn
   
   ;!tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Synch RTC via net:", ChrReturn
   !tx 0 
