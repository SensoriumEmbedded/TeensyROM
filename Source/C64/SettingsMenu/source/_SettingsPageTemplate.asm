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

   ;update static settings


ShowEthernetSettings:
   ;update dynamic settings
   
WaitEthernetMenuKey:
   ;main wait loop
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
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Ethernet ", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor, " HotKey file assignments:", ChrReturn

   !tx EscC,EscSourcesColor, "  Hot Key #1:", ChrReturn, ChrReturn
   !tx                       "  Hot Key #2:", ChrReturn, ChrReturn

   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Time/RTC, Network Info ", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor,  " Format/Location:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "12/24 hour clock:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b/B", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Local Time Zone:", EscC,EscNameColor, " UTC", ChrReturn

   !tx 0 
