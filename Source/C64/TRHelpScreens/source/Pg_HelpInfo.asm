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
   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitHelpMenuKey   
   
MsgHelpMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Directory Menu Source/Navigation ", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor, " Source Select/other:", EscC,EscArgSpaces+3, ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "F1 ", EscC,EscSourcesColor, "Teensy Mem"
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, "F2 ", EscC,EscSourcesColor, "Exit to BASIC", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "F3 ", EscC,EscSourcesColor, "SD Card"
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, "F4 ", EscC,EscSourcesColor, "SID Pause/Play", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "F5 ", EscC,EscSourcesColor, "USB Drive"
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, "F6 ", EscC,EscSourcesColor, "SID Info Page", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "F7 ", EscC,EscSourcesColor, "Help Pages"
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, "F8 ", EscC,EscSourcesColor, "Settings Menu", ChrReturn, ChrReturn

   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "12345 ", EscC,EscSourcesColor, "Hot Key Launch", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor, " Source Navigation:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, "CRSR/Joy2 U/D ", EscC,EscSourcesColor, "Cursor up/dn", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, "CRSR/Joy2 L/R ", EscC,EscSourcesColor, "Page up/dn", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, "Return/Fire ", EscC,EscSourcesColor, "Launch file/enter dir", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrUpArrow,    EscC,EscSourcesColor, " Up directory", ChrReturn
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, "a-z ",         EscC,EscSourcesColor, "Next entry starting with letter", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, "Home ",        EscC,EscSourcesColor, "Beginning of current dir", ChrReturn
   !tx ChrReturn
   !tx 0
