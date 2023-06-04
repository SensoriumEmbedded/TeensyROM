; MIT License
; 
; Copyright (c) 2023 Travis Smith
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

   
SettingsMenu:
   lda #<MsgBanner
   ldy #>MsgBanner
   jsr PrintString 
   lda #<MsgSettingsMenu
   ldy #>MsgSettingsMenu
   jsr PrintString 
   lda #<MsgCreditsInfo
   ldy #>MsgCreditsInfo
   jsr PrintString 

ShowSettings:
   lda #NameColor
   jsr SendChar

   ldx #5 ;row Synch Time
   ldy #23 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask  
   jsr PrintOnOff

   ldx #6 ;row Music State
   ldy #23 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudMusicMask  
   jsr PrintOnOff
   
   ldx #7 ;row Time Zone
   ldy #26 ;col
   clc
   jsr SetCursor
   ldx #'+'
   ldy rwRegTimezone+IO1Port
   tya
   and #$80
   beq +
   ;neg number
   tya
   eor#$ff  ;1's comp
   tay
   iny ;2's comp
   ldx #'-' 
+  txa
   jsr SendChar
   tya
   ;conv to bcd
   cmp #10
   bmi +     ;skip if below 10
   sec       ;set to subtract without carry
   sbc #10   ;subtract 10
   ora #$10
+  jsr PrintHexByte

   ldx #14 ;row Special IO
   ldy #19 ;col
   clc
   jsr SetCursor


 
   lda #rwRegNextIOHndlrName
   jsr PrintSerialString
  
   
   
   


WaitForSettingsKey:     
   jsr DisplayTime   
   jsr GetIn
   beq WaitForSettingsKey

   cmp #ChrF1  ;Power-up Synch Time toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudNetTimeMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTR
   jmp ShowSettings  

+  cmp #ChrF3  ;Power-up Music State toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudMusicMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTR
   jmp ShowSettings  

+  cmp #ChrF5  ;Power-up Time Zone increment
   bne +
   ldx rwRegTimezone+IO1Port
   inx
   cpx #15
   bne ++
   ldx #-12
++ stx rwRegTimezone+IO1Port
   jsr WaitForTR
   jmp ShowSettings  

+  cmp #ChrF2  ;Synch Time now
   bne +
   jsr SynchEthernetTime
   jmp WaitForSettingsKey  

+  cmp #ChrF4  ;Toggle Music now
   bne +
   jsr ToggleSIDMusic
   jmp WaitForSettingsKey  

+  cmp #ChrF6  ;Exit
   bne +
   rts

+  cmp #ChrF7  ;Special IO
   bne +
   ;inc rwRegNextIOHndlr+IO1Port ;inc causes Rd(old),Wr(old),Wr(new)   sequential writes=bad
   ldx rwRegNextIOHndlr+IO1Port
   inx
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over overflow
   jsr WaitForTR
   jmp ShowSettings  

+  cmp #ChrF8  ;Test IO
   bne +
   jsr TestIO
   jmp WaitForSettingsKey  

+  jmp WaitForSettingsKey  

TestIO:
   jsr CursorToTest    
   lda #<MsgTesting
   ldy #>MsgTesting
   jsr PrintString 

   ldx #$05  ;outer loop count, each takes a couple seconds
-- stx PtrAddrLo ;storage for outer counter
   jsr CursorToTest    
   lda PtrAddrLo
   jsr PrintHexNibble
   ;jsr PrintHexByte
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
   ldx PtrAddrLo
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

CursorToTest
   ldx #15 ;row 
   ldy #17 ;col
   clc
   jsr SetCursor   
   rts


