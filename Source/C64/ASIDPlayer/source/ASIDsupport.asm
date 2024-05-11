
;subroutines, tables, and strings

;SIDVoicesOff:
;   lda #0x00 ; turn 3 voices off
;   sta SIDLoc+$04 ; SIDVoicCont1
;   sta SIDLoc+$0b ; SIDVoicCont2
;   sta SIDLoc+$12 ; SIDVoicCont3 
;   rts

SIDinit:
   ;clear SID regs
   lda #0
   tax
-  sta SIDLoc, x
   inx
   cpx #$19
   bne -
   rts
   
PrintString:
   ;replaces BASIC routine jsr $ab1e when unavailable
   ;usage: lda #<MsgM2SPolyMenu,  ldy #>MsgM2SPolyMenu,  jsr PrintString 
   sta smcPrintStringAddr+1
   sty smcPrintStringAddr+2
   ldy #0
smcPrintStringAddr
-  lda $fffe, y
   beq +
   jsr SendChar
   iny
   bne -
   inc smcPrintStringAddr+2
   bne -
+  rts
   
PrintHexByte:
   ;Print byte value stored in acc in hex (2 chars)
   pha
   lsr
   lsr
   lsr
   lsr
   jsr PrintHexNibble
   pla
   ;pha   ; preserve acc on return?
   jsr PrintHexNibble
   ;pla
   rts
   
PrintHexNibble:   
   ;Print value stored in lower nible acc in hex
   ;trashes acc
   and #$0f
   cmp #$0a
   bpl l 
   clc
   adc #'0'
   jmp pr
l  clc
   adc #'a'-$0a
pr jsr SendChar
   rts


MsgASIDPlayerMenu:    
   !tx NameColor, ChrClear, ChrPurple, ChrToLower, ChrRvsOn, "       TeensyROM ASID Player v0.1       "
   !tx ChrReturn, OptionColor
   !tx 0
   
MsgASIDStart:    
   !tx NameColor, "ASID Start"
   !tx ChrReturn, OptionColor
   !tx 0

MsgASIDStop:    
   !tx NameColor, "ASID Stop"
   !tx ChrReturn, OptionColor
   !tx 0

