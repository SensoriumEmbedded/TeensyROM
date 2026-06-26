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

EthernetValColumn = 19   ;Column for values

EthernetMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgEthernetMenu
   ldy #>MsgEthernetMenu
   jsr PrintString 
   
  
   lda #rCtlMakeEthMACWAIT
   ldx #5 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthIPAcqTypeWAIT
   ldx #6 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthDHCPTOWAIT
   ldx #9 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthDHCPRespTOWAIT
   ldx #10 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthStatIPWAIT
   ldx #13 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthStatDNSIPWAIT
   ldx #14 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthStatGatewWAIT
   ldx #15 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName

   lda #rCtlMakeEthStatSubMskWAIT
   ldx #16 ;row
   ldy #EthernetValColumn ;col
   jsr PrintFileName
   
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
   !tx EscC,EscSourcesColor, ChrRvsOn, " Info: Ethernet ", ChrReturn, ChrReturn
   
   !tx EscC,EscTimeColor,  " General Settings:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, " MAC Address:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "     IP Type:", ChrReturn, ChrReturn  ;Static/DHCP
      
   !tx EscC,EscTimeColor,  " DHCP Specific:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "DHCP Timeout:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "DHCP Resp TO:", ChrReturn, ChrReturn
   
   !tx EscC,EscTimeColor,  " Static IP Specific:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "   Static IP:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "      DNS IP:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, "  Gateway IP:", ChrReturn
   !tx EscC,EscArgSpaces+5, EscC,EscSourcesColor, " Subnet Mask:", ChrReturn, ChrReturn
   
   !tx EscC,EscArgSpaces+4, "Modify values in terminal such as", ChrReturn
   !tx EscC,EscArgSpaces+6,   "CCGMS, use \"AT?\" for help"
   ;                         1234567890123456789012345678901234567890
   !tx 0 
