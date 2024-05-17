
;subroutines, tables, and strings

SIDVoicesOff:
   lda #0x00 ; turn 3 voices off
   sta SIDLoc+$04 ; SIDVoicCont1
   sta SIDLoc+$0b ; SIDVoicCont2
   sta SIDLoc+$12 ; SIDVoicCont3 
   rts

SIDinit:
   jsr SIDVoicesOff ;voices off first to prevent pops
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
   !tx NameColor, ChrClear, ChrPurple, ChrToLower, ChrRvsOn, "         TeensyROM ASID Player          "
   !tx ChrReturn, ChrYellow, "     ", ChrLtRed, "  ", ChrBlack, "*************************", "XXXXXXX", ChrReturn
   !tx ChrYellow, "  321", ChrLtRed, "RP", ChrLtGreen, "FcPwTAS", ChrRvsOn, "FcPwTAS", ChrRvsOff, "FcPwTAS", ChrLtBlue, "CtRV" ;, "5678901"
   !tx ChrReturn, ChrLtGreen, "       ", $ed, $60, $60, "1", $60, $60, $fd,  $ed, $60, $60, "2", $60, $60, $fd,  $ed, $60, $60, "3", $60, $60, $fd  
   !tx ChrReturn, ChrPurple, $60, $60, $60, $60, $60, $60, $60, $60, $60, $60,  $60, $60, $60, $60, $60, $60, $60, $60, $60, $60
   !tx                       $60, $60, $60, $60, $60, $60, $60, $60, $60, $60,  $60, $60, $60, $60, $60, $60, $60, $60, $60, $60
   !tx OptionColor
   !tx 0
MsgASIDPlayerCommands:    
   !tx ChrWhite
   !tx "Keyboard Commands:", ChrReturn
   !tx ChrLtGrey
   !tx "   v: Set Voice regs to Off", ChrReturn
   !tx "   s: Screen toggle On/Off", ChrReturn
   !tx "   i: Show Indicator Decoder", ChrReturn
   !tx "   ?: Show this Commands List", ChrReturn
   !tx "   r: Refresh/Clear Screen", ChrReturn
   !tx "   x: Exit ASID Player", ChrReturn
   !tx "   2: Second SID address "
   !tx "   3: Third SID address "
   !tx 0
MsgASIDPlayerDecoder:    
   !tx ChrLtGrey
   !tx "Indicator Decoder:", ChrReturn
   !tx ChrMedGrey, " Spinners:", ChrReturn, ChrYellow
   ;!tx "   4: SID#4 Access", ChrReturn
   !tx "   3: SID#3 Access", ChrReturn
   !tx "   2: SID#2 Access", ChrReturn
   !tx "   1: SID#1 Access", ChrReturn, ChrLtRed
   !tx "   R: Read Error", ChrReturn
   !tx "   P: Packet Error", ChrReturn
   !tx ChrMedGrey, " Voice Reg Access: (x3)", ChrReturn
   !tx ChrLtGreen
   !tx "  Fc: Frequency Lo/Hi", ChrReturn
   !tx "  Pw: Pulse Width Lo/Hi", ChrReturn
   !tx "   T: Wave Type", ChrReturn
   !tx "   A: Attack/Decay", ChrReturn
   !tx "   S: Sustain/Release", ChrReturn
   !tx ChrMedGrey, " Filter Reg Access:", ChrReturn
   !tx ChrLtBlue
   !tx "  Ct: Cutoff Frequency Lo/Hi", ChrReturn
   !tx "   R: Resonance", ChrReturn
   !tx "   V: Volume & Select", ChrReturn
   !tx 0
   
MsgASIDStart:    
   !tx NameColor, "ASID Start"
   !tx ChrReturn, OptionColor
   !tx 0

MsgASIDStop:    
   !tx NameColor, "ASID Stop"
   !tx ChrReturn, OptionColor
   !tx 0

