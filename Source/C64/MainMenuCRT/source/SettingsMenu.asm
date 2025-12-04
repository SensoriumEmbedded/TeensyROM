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

SetValColumn = 29   ;Column for power on defaults settings
   
SettingsMenu:
   jsr PrintBanner ;SourcesColor
   lda #<MsgSettingsMenu1
   ldy #>MsgSettingsMenu1
   jsr PrintString 

   lda #rCtlMakeInfoStrWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ldx #21 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   lda TblEscC+EscMenuMiscColor
   sta $0286  ;set text color
   lda #rsstSerialStringBuf ; Build info from rCtlMakeInfoStrWAIT
   jsr PrintSerialString
   
   lda #<MsgSettingsMenu3
   ldy #>MsgSettingsMenu3
   jsr PrintString 

ShowSettings:
   lda TblEscC+EscNameColor
   sta $0286  ;set text color

   ldx #4  ;row Special IO
   ldy #SetValColumn-10 ;col
   clc
   jsr SetCursor
   lda #rsstNextIOHndlrName
   jsr PrintSerialString
  
   ldx #5  ;row Joy 2 Speed
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

   ldx #6 ;row Time Zone
   ldy #SetValColumn+3 ;col
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
  
   ldx #7  ;row 12/24 hour clock
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr
   beq + ;branch if 12 hour
   lda #'2'
   jsr SendChar   
   lda #'4'
   jmp ++
+  lda #'1'
   jsr SendChar
   lda #'2'
++ jsr SendChar

   ldx #8 ;row Synch Time
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask  
   jsr PrintOnOff

   ldx #9 ;row Music State
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudSIDPauseMask  
   eor #rpudSIDPauseMask  
   jsr PrintOnOff
   
   ldx #10 ;row Show Extension
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudShowExtension  
   jsr PrintOnOff
   
   ldx #11 ;row Host Serial Control
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2HostSerCtlMask  ;already shifted to be 2x the value 0-2
   tax
   lda TblMsgHostSerCtl,x
   ldy TblMsgHostSerCtl+1,x
   jsr PrintString
   
   ldx #12 ;row TCP Listen 
   ldy #SetValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2TRTCPListen  
   jsr PrintOnOff
   

WaitForSettingsKey:     
   jsr DisplayTime   
   jsr CheckForIRQGetIn
   beq WaitForSettingsKey

+  cmp #'a'  ;Special IO Increment
   bne +
   ;inc rwRegNextIOHndlr+IO1Port ;inc causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   inx
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over overflow
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'A'  ;Special IO Decrement
   bne +
   ;dec rwRegNextIOHndlr+IO1Port ;dec causes Rd(old),Wr(old),Wr(new)   sequential writes=bad for waiting function
   ldx rwRegNextIOHndlr+IO1Port
   dex
   stx rwRegNextIOHndlr+IO1Port ;TR code will roll-over underflow
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'b'  ;Joystick 2 Speed Increment
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   clc
   adc #$10
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'B'  ;Joystick 2 Speed Decrement
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   sec       ;set to subtract without carry
   sbc #$10   
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'c'  ;Power-up Time Zone Increment
   bne +
   ldx rwRegTimezone+IO1Port
   inx
   ;tz rnge is -12 to +14 from UTC (x2)
   cpx #29
   bne UpdTimeZone
   ldx #-24
   jmp UpdTimeZone

+  cmp #'C'  ;Power-up Time Zone Decrement
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

+  cmp #ChrF1  ;Exit (special for IRQ remote start return)
   bne +
   rts

+  cmp #ChrF4  ;Toggle Music now
   bne +
   jsr ToggleSIDMusic
   jmp WaitForSettingsKey  

;--------------------Shift/non-shift mean the same from here on...---------------------
+  and #$7f  ;Force to lower case

+  cmp #'d'  ;12/24 hour clock
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudClock12_24hr  
   sta rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr  
   sta smc24HourClockDisp+1 ;24(non-zero) vs 12(zero) hr display 
   jsr WaitForTRWaitMsg
   jmp ShowSettings  


+  cmp #'e'  ;Power-up Synch Time toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudNetTimeMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'f'  ;Power-up Music State toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudSIDPauseMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'g'  ;Show File Extensions
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudShowExtension
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'h'  ;Choose Serial control device
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
   jmp ShowSettings  

+  cmp #'i'  ;Toggle TCP Listener
   bne +
   lda rwRegPwrUpDefaults2+IO1Port
   eor #rpud2TRTCPListen  
   sta rwRegPwrUpDefaults2+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowSettings  

+  cmp #'j'  ;Reboot TeensyROM
   bne +
   lda #$00    
   sta $d011   ;turn off the display   
   lda #rCtlRebootTeensyROM 
   sta wRegControl+IO1Port
   ;no need to wait, TR/C64 will be rebooting...
   jmp WaitForSettingsKey  

+  cmp #'k'  ;Synch Time now
   bne +
   jsr PrintBanner ;SourcesColor
   jsr SynchEthernetTime
   jsr AnyKeyMsgWait ;debug for looking at messages  <------------------------------------------
   jmp SettingsMenu ;force to reprint all 
   
+  cmp #'l'  ;Test IO
   bne +
   jsr TestIO
   jmp WaitForSettingsKey  
   
+  cmp #'m'  ;clear auto-launch
   bne +
   lda #rCtlClearAutoLaunchWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   ;moves cursor to upper right
   ;  Display "done" after "Clear Auto Launch"
   ldx #17 ;row 
   ldy #27 ;col
   clc
   jsr SetCursor   
   lda #<MsgDone
   ldy #>MsgDone
   jsr PrintString 
   jmp WaitForSettingsKey  
   
+  cmp #'n'  ;Help Menu
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

CursorToTest:
   ldx #16 ;row 
   ldy #22 ;col
   clc
   jsr SetCursor   
   rts

