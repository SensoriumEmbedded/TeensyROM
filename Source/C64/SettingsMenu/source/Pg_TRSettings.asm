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


SetValColumn = 29   ;Column for TR setting values

TRSettings:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgTRSettings
   ldy #>MsgTRSettings
   jsr PrintString 

;print kernal file name:
   lda #rCtlMakeKernalStrWAIT
   ldx #8 ;row
   ldy #2 ;col
   jsr PrintFileName
   
;print REU file name:
   lda #rCtlMakeREUStrWAIT
   ldx #11 ;row
   ldy #2 ;col
   jsr PrintFileName


ShowTRSettings:
   lda TblEscC+EscNameColor
   sta $0286  ;set text color

   ldx #5  ;row Special IO
   ldy #19 ;col
   clc
   jsr SetCursor
   lda #rsstNextIOHndlrName
   jsr PrintSerialString
  
   ldx #16  ;row Joy 2 Speed
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   ;and #rpudJoySpeedMask ;no need, upper 4 bits
   lsr
   lsr
   lsr
   lsr
   jsr PrintIntByte
   lda #' '
   jsr SendChar

   ldx #17 ;row Show Extension
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudShowExtension  
   jsr PrintOnOff
   
   ldx #15 ;row Host Serial Control
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2HostSerCtlMask  ;already shifted to be 2x the value 0-2
   tax
   lda TblMsgHostSerCtl,x
   ldy TblMsgHostSerCtl+1,x
   jsr PrintString


WaitTRSettingsKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitTRSettingsKey

+  cmp #'a'  ;Special IO Increment
   bne +
   ;inc rwRegNextIOHndlr+IO1Port ;inc causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   inx
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over overflow
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  

+  cmp #'A'  ;Special IO Decrement
   bne +
   ;dec rwRegNextIOHndlr+IO1Port ;dec causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   dex
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over underflow
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  

+  cmp #'c'  ;Joystick 2 Speed Increment
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   clc
   adc #$10
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  

+  cmp #'C'  ;Joystick 2 Speed Decrement
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   sec       ;set to subtract without carry
   sbc #$10   
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  

+  cmp #'d'  ;Show File Extensions
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudShowExtension
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  

+  cmp #'b'  ;Choose Serial control device
   bne +
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2NFCEnabled
   beq ++ ;skip if not NFC
   ;disable Special IO if enabling NFC:
   ldx #IOH_None 
   stx rwRegNextIOHndlr+IO1Port
   jsr WaitForTRWaitMsg
   ldx #rpud2TRContEnabled ;TRCont is next
   jmp Updrpud2
++ lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2TRContEnabled
   beq ++ ;skip if not TR Control interface
   ldx #0 ;none is next
   jmp Updrpud2
++ ;not NFC or TRCont, currently none
   ldx #rpud2NFCEnabled ;NFC is next
Updrpud2
   stx smcNewscd+1;x reg contains new serial control device
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2HostSerCtlMaskInv ;preserve the other bits
smcNewscd
   ora #0
   sta rwRegPwrUpDefaults2+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowTRSettings  
   
+  cmp #'f'  ;Reboot TeensyROM
   bne +
   lda #139  ; 155 default minus bit 4
   sta $d011   ;blank the display   
   lda #rCtlRebootTeensyROM 
   sta wRegControl+IO1Port
   ;no need to wait, TR/C64 will be rebooting...
   jmp WaitTRSettingsKey  
   
+  cmp #'e'  ;Self Test IO
   bne +
   jsr TestIO
   jmp WaitTRSettingsKey     
   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitTRSettingsKey   
   
TestIO:
   jsr CursorToTest    
   lda #<MsgTesting
   ldy #>MsgTesting
   jsr PrintString 

   ldx #$02  ;init outer loop count, each takes a couple seconds
-- stx smcTestIOCnt+1 ;storage for outer counter
   ldx #$00
   ldy #$00
-  lda rRegPresence1+IO1Port
   cmp #$55
   bne +
   lda rRegPresence2+IO1Port
   cmp #$AA
   bne +
   dex
   bne -
   dey
   bne -
smcTestIOCnt
   ldx #0   ;outer loop count, each takes a couple seconds
   dex
   bne --

   jsr CursorToTest    
   lda #<MsgPass
   ldy #>MsgPass
   jsr PrintString 
   rts

+  jsr CursorToTest   
   lda #<MsgFail
   ldy #>MsgFail
   jsr PrintString 
   rts

CursorToTest:
   ldx #18 ;row test status
   ldy #22 ;col
   clc
   jsr SetCursor   
   rts   
   
MsgTesting:
   !tx EscC,EscNameColor, "Testing", 0
MsgPass:
   !tx "Passed ", 0
MsgFail:
   !tx "Failed ", 0
   
MsgTRSettings:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: TeensyROM General ", ChrReturn, ChrReturn
   
   !tx EscC,EscTimeColor,  " Emulation Selections:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a/A", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Special IO:", ChrReturn, ChrReturn

   !tx EscC,EscSourcesColor,  "  Kernal replace file: ('K' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor,  "  REU Pre-load/save file: ('R' to sel)", ChrReturn, ChrReturn, ChrReturn, ChrReturn
   ;                           1234567890123456789012345678901234567890

   !tx EscC,EscTimeColor,  " User Interface/other:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Hosted Serial Device:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c/C", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "     Joystick2 Speed:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Show File Extensions:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Run Self Test", ChrReturn   
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Reboot TeensyROM" , ChrReturn
   !tx 0 

TblMsgHostSerCtl: ;must match RegPowerUpDefaultMasks2 bits
   !word MsgHostSerCtlNone
   !word MsgHostSerCtlNFC
   !word MsgHostSerCtlController
MsgHostSerCtlNone:
   !tx "None  ", 0
MsgHostSerCtlNFC:
   !tx "NFC   ", 0
MsgHostSerCtlController:
   !tx "TRCont", 0
