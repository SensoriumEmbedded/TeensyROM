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



HelpMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgHelpMenu
   ldy #>MsgHelpMenu
   jsr PrintString 
   
WaitHelpMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitHelpMenuKey

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
   jmp WaitHelpMenuKey   
   
MsgHelpMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Help: Main Menu Source/Navigation ", ChrReturn, ChrReturn

   !tx EscC,EscNameColor, " Source Select/other:", EscC,EscArgSpaces+3, ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F1", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Teensy Mem"
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F2", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Exit to BASIC", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F3", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "SD Card"
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F4", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "SID on/off", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F5", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "USB Drive"
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F6", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "SID Information", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F7/F8", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Help/Settings", ChrReturn
   ;!tx EscC,EscArgSpaces+8, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "F8", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Settings Menu"
   !tx ChrReturn
   !tx EscC,EscNameColor, " Directory Menu Navigation:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "U/D", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Cursor up/dn", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "CRSR/Joy2", ChrRvsOff, ChrFillLeft, ChrFillRight, ChrRvsOn, "L/R", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Page up/dn", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Return/Fire", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Select file or dir", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, ChrUpArrow, ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Up directory", ChrReturn
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a-z", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Next entry starting with letter", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Home", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Beginning of current dir", ChrReturn
   !tx ChrReturn
   !tx 0
