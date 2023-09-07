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
   jsr PrintBanner 
   lda #<MsgSettingsMenu1
   ldy #>MsgSettingsMenu1
   jsr PrintString 
   lda #<MsgSettingsMenu2
   ldy #>MsgSettingsMenu2
   jsr PrintString 

   lda #rCtlMakeInfoStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg
   ldx #18 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   lda #NameColor
   jsr SendChar
   lda #rsstSerialStringBuf ; Build info from rCtlMakeInfoStrWAIT
   jsr PrintSerialString

ShowSettings:
   lda #NameColor
   jsr SendChar

   ldx #5 ;row Time Zone
   ldy #24 ;col
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
   ;x=sign char, y=abs val
+  txa
   jsr SendChar
   tya
   jsr PrintIntByte
   lda #' '
   jsr SendChar

   ldx #6  ;row Special IO
   ldy #21 ;col
   clc
   jsr SetCursor
   lda #rsstNextIOHndlrName
   jsr PrintSerialString
  
   ldx #7  ;row Joy 2 Speed
   ldy #21 ;col
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

   ldx #8 ;row Synch Time
   ldy #21 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask  
   jsr PrintOnOff

   ldx #9 ;row Music State
   ldy #21 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudMusicMask  
   jsr PrintOnOff
   

WaitForSettingsKey:     
   jsr DisplayTime   
   jsr GetIn
   beq WaitForSettingsKey

+  cmp #'a'  ;Power-up Time Zone Increment
   bne +
   ldx rwRegTimezone+IO1Port
   inx
   cpx #15
   bne ++
   ldx #-12
++ stx rwRegTimezone+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'A'  ;Power-up Time Zone Decrement
   bne +
   ldx rwRegTimezone+IO1Port
   dex
   cpx #-13
   bne ++
   ldx #14
++ stx rwRegTimezone+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'b'  ;Special IO Increment
   bne +
   ;inc rwRegNextIOHndlr+IO1Port ;inc causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   inx
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over overflow
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'B'  ;Special IO Decrement
   bne +
   ;dec rwRegNextIOHndlr+IO1Port ;dec causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   dex
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over underflow
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'c'  ;Joystick 2 Speed Increment
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   clc
   adc #$10
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'C'  ;Joystick 2 Speed Decrement
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   sec       ;set to subtract without carry
   sbc #$10   
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

;--------------------Shift/non-shift mean the same from here on...---------------------
+  and #$7f  ;Force to lower case

   cmp #'d'  ;Power-up Synch Time toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudNetTimeMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'e'  ;Power-up Music State toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudMusicMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'f'  ;Synch Time now
   bne +
   jsr SynchEthernetTime
   jmp WaitForSettingsKey  

+  cmp #'g'  ;Toggle Music now
   bne +
   jsr ToggleSIDMusic
   jmp WaitForSettingsKey  

+  cmp #'h'  ;Test IO
   bne +
   jsr TestIO
   jmp WaitForSettingsKey  
   
+  cmp #ChrSpace  ;Exit
   bne +
   rts

+  cmp #ChrReturn  ;force refresh full screen (& temp read)
   bne +
   jmp SettingsMenu  

+  jmp WaitForSettingsKey  

TestIO:
   jsr CursorToTest    
   lda #<MsgTesting
   ldy #>MsgTesting
   jsr PrintString 

   ldx #$03  ;outer loop count, each takes a couple seconds
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
   ldx #13 ;row 
   ldy #18 ;col
   clc
   jsr SetCursor   
   rts


