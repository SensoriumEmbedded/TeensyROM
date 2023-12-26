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
   
   lda rRegStrAvailable+IO1Port 
   bne +   ;Make sure ready to x-fer
   jsr AnyKeyErrMsgWait  ;turns IRQ back on,    an error occurred
   rts
   
   ;load SID to C64 RAM, same as PRGLoadStart...
+  lda rRegStreamData+IO1Port
   sta PtrAddrLo
   lda rRegStreamData+IO1Port
   sta PtrAddrHi
   ldy #0   ;zero offset
   
-  lda rRegStrAvailable+IO1Port ;are we done?
   beq +   ;exit the loop
   lda rRegStreamData+IO1Port ;read from rRegStreamData+IO1Port increments address & checks for end
   sta (PtrAddrLo), y 
   iny
   bne -
   inc PtrAddrHi
   bne -
   ;good luck if we get to here... Trying to overflow and write to zero page
   jsr AnyKeyErrMsgWait  ;turns IRQ back on
   rts

   ;self-modifying init jump
+  lda rRegSIDInitLo+IO1Port
   sta smcSIDInitAddr+1
   lda rRegSIDInitHi+IO1Port
   sta smcSIDInitAddr+2
   
   ;self-modifying play jump
   lda rRegSIDPlayLo+IO1Port
   sta smcSIDPlayAddr+1
   lda rRegSIDPlayHi+IO1Port
   sta smcSIDPlayAddr+2

   sei
   lda #$35; Disable Kernal and BASIC ROMs
   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
   sta $01
   lda #$00  ;set to first song in SID
smcSIDInitAddr
   jsr $fffe ;Initialize music (self modified code)
   lda #$37 ; Reset the Kernal and BASIC ROMs
   sta $01
   cli
   jsr IRQEnable  ;start the IRQ wedge
   rts
   
ToggleSIDMusic:
   lda smcSIDPlayEnable+1
   eor #rpudMusicMask   ;toggle playing status
   sta smcSIDPlayEnable+1
   beq SIDVoicesOff ; if now off, turn voices off & return
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
   and #%10000000   ; PRESERVE KERNAL-SET TOD CLOCK NTSC OR PAL SELECTION
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
   
smcSIDPlayEnable
+  lda #0  ;default to disabled
   beq ++

smcBorderEffect
   lda #0  ;default to disabled
   beq +
   inc BorderColorReg ;tweak display border
+  lda #$35; Disable Kernal and BASIC ROMs
   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
   sta $01
smcSIDPlayAddr
   jsr $fffe          ;Play the music, self modifying
   lda #$37 ; Reset the Kernal and BASIC ROMs
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

PrintSIDVars:
   lda #NameColor
   jsr SendChar

   ;print the timer interval in hex  
   ldx #15 ;row 
   ldy #30 ;col
   clc
   jsr SetCursor
   lda LclRegSIDSpeedHi
   ;eor #$ff   ;make bigger numbers = faster
   jsr PrintHexByte
   lda LclRegSIDSpeedLo
   ;eor #$ff   ;make bigger numbers = faster
   jsr PrintHexByte
   
WaitSIDInfoKey:
   jsr DisplayTime   
   jsr CheckForIRQGetIn    
   beq WaitSIDInfoKey

   cmp #ChrF4  ;toggle music
   bne +
   jsr ToggleSIDMusic
   jmp WaitSIDInfoKey  

+  cmp #'b'  ;Toggle Border effect
   bne +
   lda smcBorderEffect+1
   eor #1
   sta smcBorderEffect+1
   jmp WaitSIDInfoKey   

+  cmp #'d'  ;Set SID speed to default
   bne +
   jsr SetSIDSpeedToDefault
   jmp PrintSIDVars  

+  cmp #ChrCRSRLeft  ;increase SID speed (small step)
   bne +
   ldx LclRegSIDSpeedLo
   dex   
   stx LclRegSIDSpeedLo
   stx CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   cpx #$ff
   bne PrintSIDVars
   jmp decSIDSpeedHi   ;underflow

+  cmp #ChrCRSRRight  ;decrease SID speed (small step)
   bne +
   ldx LclRegSIDSpeedLo
   inx
   stx LclRegSIDSpeedLo
   stx CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   bne PrintSIDVars
   jmp incSIDSpeedHi   ;overflow
   
+  cmp #ChrCRSRUp  ;increase SID speed (big step)
   bne +
decSIDSpeedHi
   ldx LclRegSIDSpeedHi
   dex   
   jmp updateSpeedHi  

+  cmp #ChrCRSRDn  ;decrease SID speed (big step)
   bne +
incSIDSpeedHi
   ldx LclRegSIDSpeedHi
   inx
updateSpeedHi
   stx LclRegSIDSpeedHi
   stx CIA1TimerA_Hi        ;Write to Set, Read gives countdown timer
   jmp PrintSIDVars  

+  cmp #ChrF1  ;Teensy mem Menu
   beq ++
   cmp #ChrSpace  ;back to Main Menu
   bne WaitSIDInfoKey   
++ rts

SetSIDSpeedToDefault:
   ;Set the timer interval to default
   lda rRegSIDDefSpeedLo+IO1Port
   sta LclRegSIDSpeedLo     ; set local reg to default
   sta CIA1TimerA_Lo        ;Write to Set, Read gives countdown timer
   lda rRegSIDDefSpeedHi+IO1Port
   sta LclRegSIDSpeedHi     ; set local reg to default
   sta CIA1TimerA_Hi        ;Write to Set, Read gives countdown timer
   rts
   
LclRegSIDSpeedHi:
   !byte 0x40
LclRegSIDSpeedLo:
   !byte 0x40
