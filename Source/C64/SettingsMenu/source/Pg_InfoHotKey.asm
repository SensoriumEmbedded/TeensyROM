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


!set HKStartRow = 6

InfoHotKeyMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgInfoHotKeyMenu
   ldy #>MsgInfoHotKeyMenu
   jsr PrintString 

   lda #rCtlMakeHotKey1WAIT
   ldx #HKStartRow ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeHotKey2WAIT
   ldx #HKStartRow+3 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeHotKey3WAIT
   ldx #HKStartRow+6 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeHotKey4WAIT
   ldx #HKStartRow+9 ;row
   ldy #2 ;col
   jsr PrintFileName

   lda #rCtlMakeHotKey5WAIT
   ldx #HKStartRow+12 ;row
   ldy #2 ;col
   jsr PrintFileName


ShowInfoHotKeySettings:
   ;update dynamic settings
   
WaitInfoHotKeyMenuKey:
   ;main wait loop
   jsr DisplayTime   
   jsr GetIn    
   beq WaitInfoHotKeyMenuKey

   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitInfoHotKeyMenuKey   
   
MsgInfoHotKeyMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Info: HotKeys", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor, " HotKey file assignments:", ChrReturn
   !tx EscC,EscSourcesColor, "  Hot Key #1: ('!' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx                       "  Hot Key #2: ('\"' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx                       "  Hot Key #3: ('#' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx                       "  Hot Key #4: ('$' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx                       "  Hot Key #5: ('%' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx 0 

 
