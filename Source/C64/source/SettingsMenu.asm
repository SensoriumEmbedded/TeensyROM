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
   lda #<MsgSettingsMenu2SpaceRet
   ldy #>MsgSettingsMenu2SpaceRet
   jsr PrintString 
   lda #<MsgSettingsMenu3
   ldy #>MsgSettingsMenu3
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
   ldy #23 ;col
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
   ror  ;divide by two, carry bit holds half hour
   php  ;save carry bit on stack
   jsr PrintIntByte
   
   plp
   bcc++
   lda #'.'
   jsr SendChar   
   lda #'5'
   jsr SendChar   
++ lda #' '
   jsr SendChar
   lda #' '
   jsr SendChar

   ldx #6  ;row Special IO
   ldy #20 ;col
   clc
   jsr SetCursor
   lda #rsstNextIOHndlrName
   jsr PrintSerialString
  
   ldx #7  ;row Joy 2 Speed
   ldy #20 ;col
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
   ldy #20 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask  
   jsr PrintOnOff

   ldx #9 ;row Music State
   ldy #20 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudSIDPauseMask  
   eor #rpudSIDPauseMask  
   jsr PrintOnOff
   

WaitForSettingsKey:     
   jsr DisplayTime   
   jsr CheckForIRQGetIn
   beq WaitForSettingsKey

+  cmp #'a'  ;Power-up Time Zone Increment
   bne +
   ldx rwRegTimezone+IO1Port
   inx
   ;tz rnge is -12 to +14 from UTC (x2)
   cpx #29
   bne UpdTimeZone
   ldx #-24
   jmp UpdTimeZone

+  cmp #'A'  ;Power-up Time Zone Decrement
   bne +
   ldx rwRegTimezone+IO1Port
   dex
   cpx #-25
   bne UpdTimeZone
   ldx #28
UpdTimeZone
   stx rwRegTimezone+IO1Port
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

+  cmp #ChrF1  ;Exit (special for IRQ remote start return)
   bne +
   rts

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
   eor #rpudSIDPauseMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'f'  ;Synch Time now
   bne +
   jsr SynchEthernetTime
   jmp SettingsMenu ;force to reprint all in case ram reduced  

+  cmp #'g'  ;Toggle Music now
   bne +
   jsr ToggleSIDMusic
   jmp WaitForSettingsKey  

+  cmp #'h'  ;Test IO
   bne +
   jsr TestIO
   jmp WaitForSettingsKey  
   
+  cmp #'i'  ;Help Menu
   bne +
   jmp HelpMenu  ;return from there  
   
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

CursorToTest
   ldx #13 ;row 
   ldy #18 ;col
   clc
   jsr SetCursor   
   rts


