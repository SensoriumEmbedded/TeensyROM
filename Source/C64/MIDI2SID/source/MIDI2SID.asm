

; ********************************   Symbols   ********************************   
   ;!set Debug = 1 ;if defined, skips HW checks/waits 
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "..\MainMenuCRT\source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "..\MainMenuCRT\source\CommonDefs.i" ;Common between crt loader and main code in RAM
   !src "..\MainMenuCRT\source\Menu_Regs.i"  ;IO space registers matching Teensy code

   SIDVoicCont      = $0338 ;midi2sid polyphonic voice/envelope controls
   SIDAttDec        = $0339
   SIDSusRel        = $033a
   SIDDutyHi        = $033b
   
   M2SDataColumn    = 14

	BasicStart = $0801
   code       = $080D ;2061



   *=BasicStart
   ;BASIC SYS header
   !byte $0b,$08,$01,$00,$9e  ; Line 1 SYS
   !tx "2061" ;"2061" Address for sys start in text
   !byte $00,$00,$00
   
   *=code  ; Start location for code
   jmp MIDI2SID
   
   !src "source\M2Ssupport.asm"

MIDI2SID:

;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
!ifndef Debug {
;check for HW:
   lda rRegPresence1+IO1Port
   cmp #$55
   bne NoHW
   lda rRegPresence2+IO1Port
   cmp #$AA
   beq +
NoHW
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
   ;jmp (BasicWarmStartVect)
   rts   ;return to BASIC
}

+  lda #<MsgM2SPolyMenu
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
   beq ++
   cmp #ChrF1
   bne +  
++ jsr SIDVoicesOff
   rts ;return to BASIC

+  jmp M2SUpdateKeyInLoop

