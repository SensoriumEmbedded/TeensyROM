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



EthernetMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgEthernetMenu
   ldy #>MsgEthernetMenu
   jsr PrintString 
   
WaitEthernetMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitEthernetMenuKey

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
   jmp WaitEthernetMenuKey   
   
MsgEthernetMenu:
   !tx EscC,EscSourcesColor,  "Ethernet Settings Page:", ChrReturn, ChrReturn
   !tx EscC,EscNameColor,  "Some settings:", EscC,EscOptionColor, " (up/down)", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "hello", ChrReturn
   !tx 0 
