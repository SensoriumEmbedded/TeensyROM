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
   jsr SIDMusicOff
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
   
   jsr GetIn
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
   bne +
   jsr SIDVoicesOff
   lda MusicPlaying ;turn music back on if it was before...
   beq ++
   jsr SIDMusicOn
++ rts 

+  jmp M2SUpdateKeyInLoop

ToggleSIDMusic:
   lda MusicPlaying
   eor #rpudMusicMask   ;toggle playing status
   sta MusicPlaying
   beq +
   jsr SIDMusicOn ;sid is off, turn it on
   rts
+  jsr SIDMusicOff ;sid is on, turn it off
   rts
   
SIDMusicOn:  ;Start SID interrupt
   lda #$7f    ;disable all ints
   sta $dc0d   ;CIA1 int ctl
   lda $dc0d   ;CIA1 int ctl    reading clears
   sei
   lda #$01    ;raster compare enable
   sta $d01a   ;irq mask reg
   sta $d019   ;ACK any raster IRQs
   lda #100    ;mid screen
   sta $d012   ;raster scan line compare reg
   lda $d011   ;VIC ctl reg fine scrolling/control
   and #$7f    ;bit 7 is bit 8 of scan line compare
   sta $d011   ;VIC ctl reg fine scrolling/control
   lda #<irqRastSID
   ldx #>irqRastSID
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   cli
   rts

SIDMusicOff:  ;stop SID interrupt
   sei
   lda #<IRQDefault
   ldx #>IRQDefault
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi
   lda #$81
   sta $dc0d  ;CIA int ctl
   lda #0
   sta $d01a  ;irq enable
   inc $d019
   lda $dc0d  ;CIA int ctl
   cli 
   ;jsr SIDCodeRAM  ;turns voices off, but resets song to start
   jsr SIDVoicesOff
   lda #BorderColor
   sta BorderColorReg   ;restore border in case we ended in mid region
   rts

SIDVoicesOff:
   lda #0x00 ; turn 3 voices off
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs 
   rts
   
irqRastSID:
   inc $d019   ;ACK raster IRQs
   inc BorderColorReg ;tweak display border
   jsr SIDCodeRAM+3 ;Play the music
   lda #<irqRast2
   ldx #>irqRast2
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   lda #200    ;loweer part of screen
   sta $d012   ;raster scan line compare reg
   jmp IRQDefault

irqRast2:
   inc $d019   ;ACK raster IRQs
   dec BorderColorReg ;tweak it back
   lda #<irqRastSID
   ldx #>irqRastSID
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   lda #100    ;upper part of screen
   sta $d012   ;raster scan line compare reg
   
   jmp IRQDefault
