
;subroutines, tables, and strings

;SIDVoicesOff:
;   lda #0x00 ; turn 3 voices off
;   sta SIDLoc+$04 ; SIDVoicCont1
;   sta SIDLoc+$0b ; SIDVoicCont2
;   sta SIDLoc+$12 ; SIDVoicCont3 
;   rts

SIDinit:
   ;jsr SIDVoicesOff ;voices off first to prevent pops?
   ;clear all SID regs
   lda smcSID1address+1
   ldx smcSID1address+2
   jsr OneSIDinit
   lda smcSID2address+1
   ldx smcSID2address+2
   jsr OneSIDinit
   lda smcSID3address+1
   ldx smcSID3address+2
   jsr OneSIDinit
   rts
   
OneSIDinit:   
   sta smcNextSIDLoc+1
   stx smcNextSIDLoc+2
   lda #0
   tax
smcNextSIDLoc
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
   ;trashes acc, X and Y unchanged
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
   ;trashes acc, X and Y unchanged
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

PrintSIDaddress:   
   ;setup acc with high byte address and X reg with low byte
   cmp #>memNoSID ;same page as no sid, print NONE
   bne +
   lda #<MsgNone
   ldy #>MsgNone
   jsr PrintString 
   rts
+  jsr PrintHexByte
   txa
   jsr PrintHexByte
   rts
   
UpdateAllSIDAddress:
   ;set address' for all 3 SIDs from table
   lda memSID1addrNum
   asl ;to word offset
   tax
   lda memSIDaddrList,x
   sta smcSID1address+1
   lda memSIDaddrList+1,x
   sta smcSID1address+2
   
   lda memSID2addrNum
   asl ;to word offset
   tax
   lda memSIDaddrList,x
   sta smcSID2address+1
   lda memSIDaddrList+1,x
   sta smcSID2address+2
   
   lda memSID3addrNum
   asl ;to word offset
   tax
   lda memSIDaddrList,x
   sta smcSID3address+1
   lda memSIDaddrList+1,x
   sta smcSID3address+2
   rts

ClearASIDScreen:
   lda #<MsgASIDPlayerMainDisplay1
   ldy #>MsgASIDPlayerMainDisplay1
   jsr PrintString 
   
   ldx #ChrLtGreen  ;default not muted
   lda memVoiceMuteState
   and #$01
   beq +  ;not muted
   ldx #ChrRed
+  stx MsgASIDPlayerMainVoice1
   lda #<MsgASIDPlayerMainVoice1
   ldy #>MsgASIDPlayerMainVoice1
   jsr PrintString 
   
   ldx #ChrLtGreen  ;default not muted
   lda memVoiceMuteState
   and #$02
   beq +  ;not muted
   ldx #ChrRed
+  stx MsgASIDPlayerMainVoice2  
   lda #<MsgASIDPlayerMainVoice2
   ldy #>MsgASIDPlayerMainVoice2
   jsr PrintString 
   
   ldx #ChrLtGreen  ;default not muted
   lda memVoiceMuteState
   and #$04
   beq +  ;not muted
   ldx #ChrRed
+  stx MsgASIDPlayerMainVoice3
   lda #<MsgASIDPlayerMainVoice3
   ldy #>MsgASIDPlayerMainVoice3
   jsr PrintString 
   
   lda #<MsgASIDPlayerMainDisplay2
   ldy #>MsgASIDPlayerMainDisplay2
   jsr PrintString 
   
   
   lda #0
   sta smcScreenFull+1  ;clear screen full flag
   jsr SetMuteIndicator
   rts

SetMuteIndicator:
   lda memPausePlay
   beq ++
   ;stream is muted:
   lda #PokeRed
   jmp MuteColor
   ;stream is not muted:
++ lda #PokeBlack
MuteColor
   ldx #6
-  sta MuteColorStart,x
   dex
   bne -
   rts


memFrameTimer
   !byte ASIDContTimerOff   ;default to 0/off;  see ASIDregsMatching, ASIDContTimer* 
memBufferSize
   !byte ASIDContBufMedium  ;default to Medium Size;  see ASIDregsMatching, ASIDContBuf* 
memPausePlay
   !byte 0   ;0=currently streaming/not muted, 1=muted
memTextCircQueueHead:
   !byte 0
memTextCircQueueTail:
   !byte 0
   
memVoiceMuteState:  ;bit0-2 high if voice # is muted
   !byte 0
memSID1addrNum:
   !byte 0
memSID2addrNum:
   !byte 1
memSID3addrNum:
   !byte 2
memSIDaddrList:
   !word $d400    ;Num 0
   !word $d420    ;Num 1
   !word $d440    ;Num 2
   !word $d500    ;Num 3
   !word $d600    ;Num 4
   !word $df00    ;Num 5
   !word memNoSID ;Num 6 garbage location for "none" writes
memNumSIDaddresses:
   !byte 7 ;update to match list above!


MsgASIDPlayerMainDisplay1:    
   !tx NameColor, ChrClear, ChrPurple             , ChrRvsOn, "       TeensyROM ASID Player 1.2        "
   !tx ChrBlack, "                                  Mute "
   !tx ChrReturn, ChrYellow, "  @@@", ChrLtRed, "@@", ChrBlack, "****************************", ChrBrown, "  ", ChrOrange, "  " 
   !tx ChrReturn, ChrYellow, "  321", ChrLtRed, "RP", ChrLtGreen, "FrPwWAS", ChrRvsOn, "FrPwWAS", ChrRvsOff, "FrPwWAS", ChrLtBlue, "CfRV", ChrDrkGrey, "BB"
   !tx ChrReturn, "       ", 0
MsgASIDPlayerMainVoice1: ; first byte overwritten with correct color
   !tx ChrYellow, $ed, $60, $60, "1", $60, $60, $fd, 0
MsgASIDPlayerMainVoice2: ; first byte overwritten with correct color
   !tx ChrYellow, $ed, $60, $60, "2", $60, $60, $fd, 0
MsgASIDPlayerMainVoice3: ; first byte overwritten with correct color
   !tx ChrYellow, $ed, $60, $60, "3", $60, $60, $fd, 0
MsgASIDPlayerMainDisplay2:
   !tx ChrMedGrey, "     ?-Help"
   !tx ChrReturn, ChrPurple, $60, $60, $60, $60, $60, $60, $60, $60, $60, $60,  $60, $60, $60, $60, $60, $60, $60, $60, $60, $60
   !tx                       $60, $60, $60, $60, $60, $60, $60, $60, $60, $60,  $60, $60, $60, $60, $60, $60, $60, $60, $60, $60
   !tx OptionColor
   !tx 0
   
MsgASIDPlayerCommands1:    
   !tx ChrWhite
   !tx "Keyboard Commands:", ChrReturn
   !tx ChrLtGrey
   !tx "     v: Clear Voices", ChrReturn
   !tx "     s: Screen Toggle", ChrReturn
   !tx "     m: Mute All Toggle", ChrReturn
   !tx " 1/2/3: Mute Voice # Toggle", ChrReturn
   !tx "     ?: This Help List", ChrReturn
   !tx "     d: Register/Indicator Decoder", ChrReturn
   !tx "     c: Clear Screen", ChrReturn
   !tx "     x: Exit", ChrReturn
   !tx " F1/F2: First  SID address ", ChrRvsOn, "$"
   !tx 0
MsgASIDPlayerCommands2:    
   !tx ChrReturn, " F3/F4: Second SID address ", ChrRvsOn, "$"
   !tx 0
MsgASIDPlayerCommands3:    
   !tx ChrReturn, " F5/F6: Third  SID address ", ChrRvsOn, "$"
   !tx 0
MsgASIDPlayerCommands4:    
   !tx ChrReturn, "   T/t: Frame Timer ", ChrRvsOn
   !tx 0
MsgASIDPlayerCommands5:    
   !tx ChrReturn, "   B/b: Buffer Size ", ChrRvsOn
   !tx 0
;MsgASIDPlayerCommands6:    
;   !tx ChrReturn, ChrDrkGrey
;   !tx ChrReturn, "  Recommend muting all playback"
;   !tx ChrReturn, "    when changing SID adresses"
;   !tx 0
   
MsgASIDPlayerDecoder:    
   !tx ChrLtGrey
   !tx "Register/Indicator Decoder:", ChrReturn
   !tx ChrMedGrey, " Spinners:", ChrReturn, ChrYellow
   ;!tx "   4: SID#4 Access", ChrReturn
   !tx "    3: SID#3 Access", ChrReturn
   !tx "    2: SID#2 Access", ChrReturn
   !tx "    1: SID#1 Access", ChrReturn, ChrLtRed
   !tx "    R: Read Error", ChrReturn
   !tx "    P: Packet Error", ChrReturn
   !tx ChrMedGrey, " SID1 Voice Reg Access: (x3)", ChrReturn
   !tx ChrLtGreen
   !tx "   Fr: Frequency Lo/Hi", ChrReturn
   !tx "   Pw: Pulse Width Lo/Hi", ChrReturn
   !tx "    W: Wave Type", ChrReturn
   !tx "    A: Attack/Decay", ChrReturn
   !tx "    S: Sustain/Release", ChrReturn
   !tx ChrMedGrey, " SID1 Filter Reg Access:", ChrReturn
   !tx ChrLtBlue
   !tx "   Cf: Cutoff Frequency Lo/Hi", ChrReturn
   !tx "    R: Resonance", ChrReturn
   !tx "    V: Volume/Select", ChrReturn
   !tx ChrDrkGrey
   !tx "    B: Bit Bucket", ChrReturn
   !tx 0
   
MsgASIDStart:    
   !tx NameColor, "ASID Start"
   !tx ChrReturn, OptionColor
   !tx 0

MsgASIDStop:    
   !tx NameColor, "ASID Stop"
   !tx ChrReturn, OptionColor
   !tx 0

MsgNone:
   !tx "None"
   !tx 0

TblMsgTimerState: ;must match ASIDregsMatching, ASIDContTimer* order/qty
   !word MsgOff
   !word MsgOnAuto
   !word MsgOn50Hz
MsgOff:
   !tx "Off", ChrDrkGrey, 0  ;so that buffer select is grey
MsgOnAuto:
   !tx "On-Auto", 0
MsgOn50Hz:
   !tx "On-50Hz", 0

TblMsgBufferSize: ;must match ASIDregsMatching, ASIDContBuf* order/qty
   !word MsgTiny
   !word MsgSmall
   !word MsgMedium
   !word MsgLarge
   !word MsgXLarge
   !word MsgXXLarge
MsgTiny:
   !tx "Tiny", 0
MsgSmall:
   !tx "Small", 0
MsgMedium:
   !tx "Med", 0
MsgLarge:
   !tx "Lrg", 0
MsgXLarge:
   !tx "XLrg", 0
MsgXXLarge:
   !tx "XXLrg", 0
