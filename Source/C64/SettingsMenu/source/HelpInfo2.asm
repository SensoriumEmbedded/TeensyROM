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



HelpMenu2:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgHelpMenu2
   ldy #>MsgHelpMenu2
   jsr PrintString 
   
jmp WaitHelpMenuKey ;same as other help screen, just waiting for common keys  
   
MsgHelpMenu2:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Help: Main Menu Alt File Select ", ChrReturn, ChrReturn

   !tx EscC,EscNameColor, " Highlighted File Alt Functions:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrLeftArrow,    EscC,EscSourcesColor, " Write NFC tag: Highlighted File", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, ChrQuestionMark, EscC,EscSourcesColor, " Write NFC tag: Random in Dir", ChrReturn
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, "1-5",           EscC,EscSourcesColor, " Hot Key: Launch", ChrReturn
   !tx EscC,EscArgSpaces+3, EscC,EscOptionColor, "!-%",           EscC,EscSourcesColor, " Hot Key: File Select", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, "A",             EscC,EscSourcesColor, " Auto-Launch File Select", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, "M",             EscC,EscSourcesColor, " Meatloaf Mount/Launch Dxx", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, "K",             EscC,EscSourcesColor, " Kernal Replace File Select", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscOptionColor, "R",             EscC,EscSourcesColor, " REU Pre-load File Select", ChrReturn
   !tx ChrReturn, ChrReturn
   !tx EscC,EscNameColor, " For additional help, see:", ChrReturn
   !tx EscC,EscSourcesColor, " github.com/SensoriumEmbedded/TeensyROM"
   !tx 0
