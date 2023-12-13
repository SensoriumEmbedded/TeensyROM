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

MIDI2SID:
   lda #0  ;disable SID playback, but leave IRQ on
   sta smcSIDPlayEnable+1
   jsr PrintBanner 
   lda #<MsgM2SPolyMenu
   ldy #>MsgM2SPolyMenu
   jsr PrintString 
   ;clear SID regs
   lda #0
   tax
-  sta SIDLoc, x
   inx
   cpx #(EndSIDRegs-StartSIDRegs)
   bne -

   ;  set default local settings:
   lda #0x0f ; full volume
   sta SIDLoc+rRegSIDVolFltSel-StartSIDRegs
   lda #0x02 ; 12.5% duty cycle (12 bit resolution, lo reg left at 0)
   sta SIDDutyHi
   sta SIDLoc+rRegSIDDutyHi1-StartSIDRegs
   sta SIDLoc+rRegSIDDutyHi2-StartSIDRegs
   sta SIDLoc+rRegSIDDutyHi3-StartSIDRegs
   lda #0x40 ; pulse wave
   sta SIDVoicCont
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs
   lda #0x23 ; Att=16mS, Dec=72mS
   sta SIDAttDec
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   lda #0x34 ; Sus=20%, Rel=114mS
   sta SIDSusRel
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   
M2SDispUpdate:  ;upadte all M2S status display values
   lda #NameColor
   jsr SendChar
   ldx # 5 ;row  Triangle
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x10  
   jsr PrintOnOff
   
   ldx # 6 ;row  Sawtooth
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x20  
   jsr PrintOnOff
   
   ldx # 7 ;row  Pulse
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x40  
   jsr PrintOnOff
   
   ldx #8 ;row  Duty Cycle
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDutyPct
   ldy #>TblM2SDutyPct
   lda SIDDutyHi  ;duty cycle most sig nib = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'%'
   jsr SendChar
   
   ldx # 9 ;row  Noise
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x80  
   jsr PrintOnOff
 
   ldx #11 ;row  attack
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SAttack
   ldy #>TblM2SAttack
   lda SIDAttDec  ;attack = bits 7:4
   jsr Print4CharTableHiNib
   lda #'S'
   jsr SendChar

   ldx #12 ;row  decay
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDecayRelease
   ldy #>TblM2SDecayRelease
   lda SIDAttDec  ;decay = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'S'
   jsr SendChar

   ldx #13 ;row  sustain
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SSustPct
   ldy #>TblM2SSustPct
   lda SIDSusRel   ;sustain = bits 7:4
   jsr Print4CharTableHiNib
   lda #'%'
   jsr SendChar

   ldx #14 ;row  release
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDecayRelease
   ldy #>TblM2SDecayRelease
   lda SIDSusRel  ;release = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'S'
   jsr SendChar

;continue into the main loop...
M2SUpdateKeyInLoop:
;refresh dynamic SID regs from MIDI:   todo: move this to an interrupt?
   lda SIDVoicCont  ;waveform in upper nibble
   ora rRegSIDVoicCont1+IO1Port ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs 
   lda rRegSIDFreqHi1+IO1Port 
   sta SIDLoc+rRegSIDFreqHi1-StartSIDRegs 
   lda rRegSIDFreqLo1+IO1Port 
   sta SIDLoc+rRegSIDFreqLo1-StartSIDRegs 

   lda SIDVoicCont  ;waveform in upper nibble
   ora rRegSIDVoicCont2+IO1Port ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs 
   lda rRegSIDFreqHi2+IO1Port 
   sta SIDLoc+rRegSIDFreqHi2-StartSIDRegs 
   lda rRegSIDFreqLo2+IO1Port 
   sta SIDLoc+rRegSIDFreqLo2-StartSIDRegs 

   lda SIDVoicCont  ;waveform in upper nibble
   ora rRegSIDVoicCont3+IO1Port ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs 
   lda rRegSIDFreqHi3+IO1Port 
   sta SIDLoc+rRegSIDFreqHi3-StartSIDRegs 
   lda rRegSIDFreqLo3+IO1Port 
   sta SIDLoc+rRegSIDFreqLo3-StartSIDRegs 

   jsr DisplayTime
   ldx #20 ;row   ;print note vals
   ldy #3  ;col
   clc
   jsr SetCursor
   lda #NameColor
   jsr SendChar
   lda #<rRegSIDStrStart+IO1Port
   ldy #>rRegSIDStrStart+IO1Port
   jsr PrintString 
   
   jsr CheckForIRQGetIn
   beq M2SUpdateKeyInLoop
   
   cmp #'t'  ;Triangle
   bne +
   lda #0x10
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'w'  ;saWtooth
   bne +
   lda #0x20 
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'p'  ;Pulse
   bne +
   lda #0x40 
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'u'  ;dUty cycle
   bne +
   ldx SIDDutyHi  ;duty cycle most sig nib = bits 3:0, upper unused
   inx
   stx SIDDutyHi ;apply change at time of update
   stx SIDLoc+rRegSIDDutyHi1-StartSIDRegs
   stx SIDLoc+rRegSIDDutyHi2-StartSIDRegs
   stx SIDLoc+rRegSIDDutyHi3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'n'  ;Noise
   bne +
   lda #0x80 
   ;eor SIDVoicCont  ;doesnt play nice with others
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'a'  ;Attack
   bne +
   lda SIDAttDec  ;attack = bits 7:4
   clc
   adc #$10
   sta SIDAttDec ;apply change at time of update
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'d'  ;Decay
   bne +
   lda SIDAttDec  ;decay = bits 3:0
   tax
   and #$0f
   cmp #$0f
   bne dok
   txa
   and #$f0 ;Wrap Around without overflow
   jmp dcnt
dok   
   inx
   txa
dcnt
   sta SIDAttDec ;apply change at time of update
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'s'  ;Sustain
   bne +
   lda SIDSusRel  ;sustain = bits 7:4
   clc
   adc #$10
   sta SIDSusRel ;apply change at time of update
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'r'  ;Release
   bne +
   lda SIDSusRel  ;release = bits 3:0
   tax
   and #$0f
   cmp #$0f
   bne rok
   txa
   and #$f0 ;Wrap Around without overflow
   jmp rcnt
rok   
   inx
   txa
rcnt
   sta SIDSusRel ;apply change at time of update
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'x'  ;Exit M2S
   beq ++
   cmp #ChrF1
   bne +  
++ jsr SIDVoicesOff
   rts 

+  jmp M2SUpdateKeyInLoop

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

   ;Set the timer interval
   lda rwRegSIDSpeedLo+IO1Port
   sta $dc04        ;    CIA#1 TIMER A LO
   lda rwRegSIDSpeedHi+IO1Port
   sta $dc05        ;    CIA#1 TIMER A HI

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
   beq +
   !ifdef SidDisp {
   inc BorderColorReg ;tweak display border
   }
   lda #$35; Disable Kernal and BASIC ROMs
   ;lda #$34; Disable IO, Kernal and BASIC ROMs (RAM only)
   sta $01
smcSIDPlayAddr
   jsr $fffe          ;Play the music, self modifying
   lda #$37 ; Reset the Kernal and BASIC ROMs
   sta $01
   !ifdef SidDisp {
   dec BorderColorReg ;tweak display border
   }
+  jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S 60HZ(?) IRQ HANDLER ROUTINE

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
   lda #<MsgSIDInfo
   ldy #>MsgSIDInfo
   jsr PrintString 

   lda #<MsgSettingsMenu2SpaceRet
   ldy #>MsgSettingsMenu2SpaceRet
   jsr PrintString 

WaitSIDInfoKey:
   jsr DisplayTime   
   jsr CheckForIRQGetIn    
   beq WaitSIDInfoKey

   cmp #ChrF4  ;toggle music
   bne +
   jsr ToggleSIDMusic
   jmp WaitSIDInfoKey  

;+  cmp #ChrF6  ;Settings Menu
;   bne +
;   jmp SettingsMenu  ;return from there
;
;+  cmp #ChrF7  ;Help
;   bne +
;   jmp HelpMenu ;refresh (could ignore)
;
;+  cmp #ChrF8  ;MIDI to SID
;   bne +
;   jmp MIDI2SID  ;return from there

+  cmp #ChrF1  ;Teensy mem Menu
   beq ++
   cmp #ChrSpace  ;back to Main Menu
   bne WaitSIDInfoKey   
++ rts
   