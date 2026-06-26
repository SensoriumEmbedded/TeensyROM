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



InfoOtherMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgInfoOtherMenu
   ldy #>MsgInfoOtherMenu
   jsr PrintString 

   ;update static settings
   
   ;current ethernet settings:
   lda #rCtlMakeEthLocalIPWAIT
   ldx #5 ;row
   ldy #19 ;col
   jsr PrintFileName

   lda #rCtlMakeEthLocalSubMskWAIT
   ldx #6 ;row
   ldy #19 ;col
   jsr PrintFileName

   lda #rCtlMakeEthLocalGatewWAIT
   ldx #7 ;row
   ldy #19 ;col
   jsr PrintFileName
   
   lda #rCtlMakeInfoStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ldx #11 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   lda TblEscC+EscMenuMiscColor
   sta $0286  ;set text color
   lda #rsstSerialStringBuf ; Build info from rCtlMakeInfoStrWAIT
   jsr PrintSerialString

   lda #<MsgMachInfo1
   ldy #>MsgMachInfo1
   jsr PrintString 
   lda #rsstMachineInfo
   jsr PrintSerialString
   lda #<MsgMachInfo2
   ldy #>MsgMachInfo2
   jsr PrintString 



ShowInfoOtherSettings:
   ;update dynamic settings
   
WaitInfoOtherMenuKey:
   ;main wait loop
   jsr DisplayTime   
   jsr GetIn    
   beq WaitInfoOtherMenuKey

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
   jmp WaitInfoOtherMenuKey   
   
MsgInfoOtherMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Info: General", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " Current Ethernet IP Values:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscSourcesColor, "     IP Address:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscSourcesColor, "     Gateway IP:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscSourcesColor, "    Subnet Mask:", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " TeensyROM/Machine info:", ChrReturn
   !tx 0 

MsgMachInfo1:
   !tx ChrReturn, EscC,EscSourcesColor, "  C64/128 clocks: ", EscC,EscNameColor
   !tx 0
MsgMachInfo2:
   !tx "0Hz TOD", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor, " For additional information, see:", ChrReturn
   !tx EscC,EscNameColor, " github.com/SensoriumEmbedded/TeensyROM"
   !tx 0

