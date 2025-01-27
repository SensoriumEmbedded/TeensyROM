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

; ******************************* SID Related ******************************* 


SIDLoadInit:
   ;SID is Prepared to transfer from TR RAM before calling
   
   jsr FastLoadFile ;load SID to C64 RAM
   beq +  ;check for error, include not ready to send due to parse error
   lda #$ff      ;rpudSIDPauseMask  ;disable SID playback on error, all bits don't allow un-pause until reload
   sta smcSIDPauseStop+1
   rts
   
   ;set self-modifying init jump
+  lda rRegSIDInitLo+IO1Port
   sta smcSIDInitAddr+1
   lda rRegSIDInitHi+IO1Port
   sta smcSIDInitAddr+2
   
   ;set self-modifying play jump
   lda rRegSIDPlayLo+IO1Port
   sta smcSIDPlayAddr+1
   lda rRegSIDPlayHi+IO1Port
   sta smcSIDPlayAddr+2

   lda #00  
   sta smcVoicesMuted+1  ;un-mute all voices on new SID load

   jsr SIDSongInit
   
   ;check play address
   lda rRegSIDPlayLo+IO1Port
   bne +
   lda rRegSIDPlayHi+IO1Port
   bne +
   ;play address is 0000: 
   ;    Init will set up interrupt, don't enable it here
   ;int won't catch IRQs, next SID will force reset
   ;todo: Disable pause capability? Wedge our interrupt?
   rts
   
+  jsr IRQEnable  ;start the IRQ wedge
   rts

SIDSongInit:
   ;run SID Init routine
   sei
   lda #$35; Disable Kernal and BASIC ROMs
   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
   sta $01
   lda rwRegSIDSongNumZ+IO1Port ;load acc with song number
smcSIDInitAddr
   jsr $fffe ;Initialize music (self modified code)
   lda #$37 ; Reset the Kernal and BASIC ROMs
   sta $01
   cli
   rts
   
ToggleSIDMusic:
   lda smcSIDPauseStop+1
   eor #rpudSIDPauseMask   ;toggle playing status
   sta smcSIDPauseStop+1
   bne SIDVoicesOff ; if now off, turn voices off & return
   rts
   
; interpreted from cryptoboy code at https://www.lemon64.com/forum/viewtopic.php?t=71980&start=30
IRQEnable:  ;insert IRQ wedge to catch CIA Timer for SID or TR generated IRQ

   ; DISABLE MASKABLE INTERRUPTS, AND THEN TURN THEM OFF
   sei              
   lda #%01111111   ; BIT 7 (OFF) MEANS THAT ANY 1S WRITTEN TO CIA ICRS TURN THOSE BITS OFF
   sta $dc0d        ;    CIA#1 INTERRUPT CONTROL REGISTER (IRC): DISABLE ALL INTERRUPTS
   sta $dd0d        ;    CIA#2 ICR: DISABLE ALL INTERRUPTS
   lda $dc0d        ; ACK (CLEAR) ANY PENDING CIA1 INTERRUPTS (READING CLEARS 'EM)
   lda $dd0d        ;    SAME FOR CIA2
   asl $d019        ; TOSS ANY PENDING VIC INTERRUPTS (WRITING CLEARS 'EM, VIA RMW MAGIC)

   jsr SetSIDSpeedToDefault

   ; HOOK INTERRUPT ROUTINE (NORMALLY POINTS TO $EA31)
   lda #<IRQwedge 
   sta $0314
   lda #>IRQwedge
   sta $0315
   
   ;Set the timer behavior
   lda #%10000001   ; CIA#1 ICR: B0->1 = ENABLE TIMER A INTERRUPT,
   sta $dc0d        ;    B7->1 = FOR B0-B6, 1 BITS GET SET, AND 0 BITS IGNORED
   lda $dc0e        ; CIA#1 TIMER A CONTROL REGISTER
   and #%10000000   ; PRESERVE KERNAL-SET(?) TOD CLOCK 50/60Hz SELECTION
   ora #%00010001   ; START TIMER A,CONTINUOUS RUN MODE, LATCHED VALUE INTO TIMER A COUNTER
   sta $dc0e        ; Write it back 
   
   cli              ; RESTORE INTERRUPTS, HOOKING COMPLETE
   rts

IRQDisable:
   sei
   lda #<IRQDefault
   ldx #>IRQDefault
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi

   lda #%10000001 
   sta $dc0d  ;CIA int ctl
   
   lda #0
   sta $d01a  ;irq enable
   inc $d019
   lda $dc0d  ;CIA int ctl
   cli 
   ;jsr SIDVoicesOff ;in case we stopped playback, turn voices off too
   ;rts
   ;continue...

SIDVoicesOff:
   lda #BorderColor
   sta BorderColorReg   ;restore border in case we ended in mid region
   lda #0x00 ; turn 3 voices off
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs 
   rts
   
IRQwedge:
   lda $dc0d          ; ACK (CLEAR) CIA#1 INTERRUPT
   bne +
   
   ;interrupt from TR
   ;inc BorderColorReg ;tweak display border
   lda #ricmdAck1   
   sta smcIRQFlagged+1  ;local flag for action in main code
   sta wRegIRQ_ACK+IO1Port  ;send ack 1 to TR
   jmp IRQDefault
   
smcSIDPauseStop
+  lda #rpudSIDPauseMask  ;default to disabled
   bne ++                 ;any bits set skips playback

smcBorderEffect
   lda #0  ;default to disabled
   beq +
   inc BorderColorReg ;tweak display border
+  lda #$35; Disable Kernal and BASIC ROMs
   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
   sta $01
smcSIDPlayAddr
   jsr $fffe          ;Play the music, self modifying
   
   ;voice muting checks
   ldx#$00
smcVoicesMuted
   lda#$00   ;direct modified reg, bits 2:0    default to none muted
   
   lsr
   bcc +
   stx $d400  ;mute voice 1
   stx $d401
   ;stx $d404
   stx $d405
   stx $d406
   
+  lsr
   bcc +
   stx $d400+7  ;mute voice 2
   stx $d401+7
   ;stx $d404+7
   stx $d405+7
   stx $d406+7
   
+  lsr
   bcc +
   stx $d400+14  ;mute voice 3
   stx $d401+14
   ;stx $d404+14
   stx $d405+14
   stx $d406+14
   
+  lda #$37 ; Reset the Kernal and BASIC ROMs
   sta $01
   lda #BorderColor
   sta BorderColorReg
++ jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S 60HZ(?) IRQ HANDLER ROUTINE

;SIDMusicOn:  ;Start SID player Raster based interrupt
;   lda #$7f    ;disable all ints
;   sta $dc0d   ;CIA1 int ctl
;   lda $dc0d   ;CIA1 int ctl    reading clears
;   sei
;   lda #$01    ;raster compare enable
;   sta $d01a   ;irq mask reg
;   sta $d019   ;ACK any raster IRQs
;   lda #100    ;mid screen
;   sta $d012   ;raster scan line compare reg
;   lda $d011   ;VIC ctl reg fine scrolling/control
;   and #$7f    ;bit 7 is bit 8 of scan line compare
;   sta $d011   ;VIC ctl reg fine scrolling/control
;   lda #<irqRastSID
;   ldx #>irqRastSID
;   sta $314    ;CINV, HW IRQ Int Lo
;   stx $315    ;CINV, HW IRQ Int Hi
;   cli
;   rts

;Raster based interrupts which also allowed synchronized border tweaking
;irqRastSID:
;   inc $d019   ;ACK raster IRQs
;   inc BorderColorReg ;tweak display border
;   lda #$35; Disable Kernal and BASIC ROMs
;   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
;   sta $01
;smcSIDPlayAddr
;   jsr $fffe ;Play the music, self modifying
;   lda #$37 ; Reset the Kernal and BASIC ROMs
;   sta $01
;   lda #<irqRast2
;   ldx #>irqRast2
;   sta $314    ;CINV, HW IRQ Int Lo
;   stx $315    ;CINV, HW IRQ Int Hi
;   lda #234    ;loweer part of screen
;   sta $d012   ;raster scan line compare reg
;   jmp IRQDefault
;
;irqRast2:
;   inc $d019   ;ACK raster IRQs
;   dec BorderColorReg ;tweak it back
;   lda #<irqRastSID
;   ldx #>irqRastSID
;   sta $314    ;CINV, HW IRQ Int Lo
;   stx $315    ;CINV, HW IRQ Int Hi
;   lda #74    ;upper part of screen
;   sta $d012   ;raster scan line compare reg
;   jmp IRQDefault

ShowSIDInfoPage:
   jsr PrintBanner
   lda #<MsgSIDInfo1
   ldy #>MsgSIDInfo1
   jsr PrintString 

   lda #rsstSIDInfo
   jsr PrintSerialString

   lda #<MsgSIDInfo2
   ldy #>MsgSIDInfo2
   jsr PrintString 

   lda #rsstMachineInfo
   jsr PrintSerialString

   lda #<MsgSIDInfo3
   ldy #>MsgSIDInfo3
   jsr PrintString 

   lda #<MsgSettingsMenu2SpaceRet
   ldy #>MsgSettingsMenu2SpaceRet
   jsr PrintString 

   ;page identifier for check from interrupt
   lda #BackgndColor
   sta PageIdentifyColor
   lda #PILSIDScreen
   sta PageIdentifyLoc  

   jsr PrintSongNum
   jsr PrintVoiceMutes

PrintSIDVars:
   jsr PrintSIDSpeed
   
WaitSIDInfoKey:
   jsr DisplayTime   
   jsr CheckForIRQGetIn    
   beq WaitSIDInfoKey

+  cmp #ChrCRSRLeft  ;increase SID speed (small step)
   bne +
   ldx rwRegSIDCurSpeedLo+IO1Port
   dex   
   stx rwRegSIDCurSpeedLo+IO1Port
   stx CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   cpx #$ff
   bne PrintSIDVars
   jmp decSIDSpeedHi   ;underflow

+  cmp #ChrCRSRRight  ;decrease SID speed (small step)
   bne +
   ldx rwRegSIDCurSpeedLo+IO1Port
   inx
   stx rwRegSIDCurSpeedLo+IO1Port
   stx CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   bne PrintSIDVars
   jmp incSIDSpeedHi   ;overflow
   
+  cmp #ChrCRSRUp  ;increase SID speed (big step)
   bne +
decSIDSpeedHi
   ldx rwRegSIDCurSpeedHi+IO1Port
   dex   
   jmp updateSpeedHi  

+  cmp #ChrCRSRDn  ;decrease SID speed (big step)
   bne +
incSIDSpeedHi
   ldx rwRegSIDCurSpeedHi+IO1Port
   inx
updateSpeedHi
   stx rwRegSIDCurSpeedHi+IO1Port
   stx CIA1TimerA_Hi        ;Write to Set, Read gives countdown timer
   jmp PrintSIDVars  

+  cmp #'d'  ;Set SID speed to default
   bne +
   jsr SetSIDSpeedToDefault
   jmp PrintSIDVars  

+  cmp #'1'  ;Voice #1 mute toggle
   bne +
   lda #%00000001
VoiceMuteTogle
   eor smcVoicesMuted+1
   sta smcVoicesMuted+1
   jsr PrintVoiceMutes 
   jmp WaitSIDInfoKey  

+  cmp #'2'  ;Voice #2 mute toggle
   bne +
   lda #%00000010
   jmp VoiceMuteTogle

+  cmp #'3'  ;Voice #3 mute toggle
   bne +
   lda #%00000100
   jmp VoiceMuteTogle

+  cmp #'+'  ;next song in SID
   bne +
   ldx rwRegSIDSongNumZ+IO1Port 
   cpx rRegSIDNumSongsZ+IO1Port
   bne ++
   ldx #$ff  ;roll over
++ inx
   stx rwRegSIDSongNumZ+IO1Port
   jsr SIDSongInit
   jsr PrintSongNum ;Reprint song num/num songs 
   jmp WaitSIDInfoKey  

+  cmp #'-'  ;prev song in SID
   bne +
   ldx rwRegSIDSongNumZ+IO1Port 
   bne ++
   ldx rRegSIDNumSongsZ+IO1Port ;roll under
   inx
++ dex
   stx rwRegSIDSongNumZ+IO1Port
   jsr SIDSongInit
   jsr PrintSongNum ;Reprint song num/num songs 
   jmp WaitSIDInfoKey  

+  cmp #ChrF4  ;toggle music
   bne +
-  jsr ToggleSIDMusic
   jmp WaitSIDInfoKey  
+  cmp #'p'  ;toggle music
   beq -

+  cmp #'b'  ;Toggle Border effect
   bne +
   lda smcBorderEffect+1
   eor #1
   sta smcBorderEffect+1
   jmp WaitSIDInfoKey   

+  cmp #'s'  ;Set current SID as default/background
   bne +
   lda #rCtlSetBackgroundSIDWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg
   ldx #21 ;row 
   ldy #33 ;col
   clc
   jsr SetCursor      
   lda #<MsgDone
   ldy #>MsgDone
   jsr PrintString 
   jmp WaitSIDInfoKey  

+  cmp #ChrF1  ;Teensy mem Menu
   beq ++
   cmp #ChrSpace  ;back to Main Menu
   beq ++
   jmp WaitSIDInfoKey   
++ rts

PrintSongNum:
   lda #NameColor
   jsr SendChar
   ;print the timer song num/num songs in decimal
   ldx #16 ;row 
   ldy #26 ;col
   clc
   jsr SetCursor  
   ldx rwRegSIDSongNumZ+IO1Port 
   inx ;from zero to one based
   txa
   jsr PrintIntByte
   lda #'/'
   jsr SendChar
   ldx rRegSIDNumSongsZ+IO1Port
   inx ;from zero to one based
   txa
   jsr PrintIntByte
   lda #ChrSpace
   jsr SendChar
   jsr SendChar
   rts
   
PrintSIDSpeed:
   lda #NameColor
   jsr SendChar
   ;print the timer interval in hex  
   ldx #17 ;row 
   ldy #30 ;col
   clc
   jsr SetCursor
   lda rwRegSIDCurSpeedHi+IO1Port
   ;eor #$ff   ;make bigger numbers = faster
   jsr PrintHexByte
   lda rwRegSIDCurSpeedLo+IO1Port
   ;eor #$ff   ;make bigger numbers = faster
   jsr PrintHexByte
   rts

PrintVoiceMutes:
   ldx #0   ;voice #
   lda smcVoicesMuted+1 
   
-  ldy #PokeGreen ;non-mute/default
   lsr
   bcc +
   ldy #PokeRed ;muted
+  sta smcCurVoicesMuted+1
   tya
   sta C64ColorRAM+40*20+30,x
smcCurVoicesMuted
   lda#00 ;recover voice status
   inx
   cpx#3
   bne -
   rts
   
FastLoadFile:   
   ;load file from Teensy to C64 RAM, same as PRGLoadStart...
   ;on return, zero flag clear if an error occured, set if OK
   
   lda rRegStrAvailable+IO1Port 
   beq +   ;Make sure ready to x-fer
   
   lda rRegStreamData+IO1Port
   sta PtrAddrLo
   lda rRegStreamData+IO1Port
   sta PtrAddrHi
   ldy #0   ;zero offset
   
-  lda rRegStrAvailable+IO1Port ;are we done?
   beq ++   ;exit the loop (zero flag set)
   lda rRegStreamData+IO1Port ;read from rRegStreamData+IO1Port increments address & checks for end
   sta (PtrAddrLo), y 
   iny
   bne -
   inc PtrAddrHi
   bne -
   ;good luck if we get to here... Trying to overflow and write to zero page
   
+  jsr TextScreenMemColor ;in case called while pic is displayed
   jsr AnyKeyErrMsgWait  ;turns IRQ back on,    an error occurred
   lda #1  ;clear zero flag
++ rts


SetSIDSpeedToDefault:
   ;Set the timer interval to default
   lda rRegSIDDefSpeedLo+IO1Port
   sta rwRegSIDCurSpeedLo+IO1Port
   sta CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   lda rRegSIDDefSpeedHi+IO1Port
   sta rwRegSIDCurSpeedHi+IO1Port
   sta CIA1TimerA_Hi        ;Write to Set, Read gives countdown timer
   rts
   
