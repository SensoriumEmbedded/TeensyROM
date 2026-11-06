; C128FastTest: 2MHz bus/memory soak for a C128 running in C64 mode, launched from the TR menu.
;
; Every region is tested in four modes per pass, each a column on screen:
;   1MHZ  write and verify at 1MHz (control)
;   2MHZ  write and verify at 2MHz ($D030 bit 0)
;   W2V1  write at 2MHz, verify at 1MHz: a FAIL here and not in W1V2 means 2MHz writes are what fail
;   W1V2  write at 1MHz, verify at 2MHz: the reverse, 2MHz reads
; ROM phases run a rotating 16-bit XOR checksum at 1MHz and 2MHz only (nothing to write); the first
; 1MHz result is the reference. RAM phases use 7 patterns (const, address, inverted address, walking one).
; Screen is only printed at 1MHz (the VIC display is off at 2MHz). A hang leaves RUN in the cell
; and the phase, mode, pattern and WRITE/VERIFY step on the NOW lines.
; Not tested: $0000-$1FFF (zero page, stack, screen, this program) and $FF00-$FFFF.
; Refuses to run on a C64 (no VIC-IIe at $D030) or when the speed check says 2MHz did not engage.
; RUN/STOP exits to BASIC. Don't press RESTORE: the NMI vector is only safe in the all-RAM phases.
; KERNAL CHROUT re-enables IRQs on its way out, so interrupts are masked again right before each bank switch.
;
; Diagnostic builds (acme -D<name>=1): SkipCharRom leaves phase 1 out; CharRomAsIO runs phase 1 with
; $01=$37 (reads the I/O block instead of the char ROM, no reference compare); NoBank never writes $01
; (skips the char ROM and the three RAM-under-ROM/IO phases, runs the rest at $37).

   !convtab pet   ;key in and text out conv to PetSCII throughout

   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2026                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of .run
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of .run
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of .run
   !byte SysAddress%10+$30         ; Calculate 4th digit of .run
   !byte $00                       ; BASIC end of line marker
BasicEnd
   !word $0000                     ; BASIC end of program

SysAddress

   !src "source/C64Defs.asm"

   ptrLo         = $fb       ;free zero page on C64
   ptrHi         = $fc
   CursorBlink   = $cc       ;0 = blink enabled
   CPUPortReg    = $01
   VICSpeedReg   = $d030     ;C128 VIC-IIe: bit 0 = 2MHz
   CIA1PortA     = $dc00
   CIA1PortB     = $dc01
   CIA2TimerALo  = $dd04
   CIA2TimerAHi  = $dd05
   CIA2CtlA      = $dd0e

   NumPhases     = 8
   NumModes      = 4
   NumPatterns   = 7
   PatCksum      = 7         ;pattern name index used for ROM checksum phases
   FirstRamPhase = 3
   FirstCrossMode = 2        ;modes 2 and 3 are the cross-speed ones, RAM phases only

   RowSpeed      = 2
   RowCounts     = 3
   RowHeader     = 5
   RowPhase0     = 6
   RowNow        = 15
   RowErr        = 17
   RowWarn       = 22
   RowFooter     = 23

   CfgAllRAM     = $30       ;$01 values
   CfgChar       = $33
   CfgDefault    = $37

!macro Plot .row, .col {
   ldx #.row
   ldy #.col
   clc
   jsr SetCursor
}

!macro PrintIdx .tbl, .idx {
   ;print the zero-terminated string whose address is word [.idx] of table .tbl
   lda .idx
   asl
   tax
   lda .tbl,x
   ldy .tbl+1,x
   jsr PrintString
}

   ;*************************************************************************************
Start:
   tsx
   stx vSP             ;exit restores the stack so RTS goes back to BASIC
   sei
   lda CIA2CtlA
   sta vSavedCtlA
   lda #1
   sta CursorBlink
   lda #PokeBlack
   sta BorderColorReg
   sta BackgndColorReg
   jsr SetupVectors
   jsr DrawScreen
   jsr CheckC128
   jsr SpeedCheck

PassLoop:
   inc vPassLo
   bne +
   inc vPassHi
+  jsr ShowCounts
   lda #0
   sta vPhase
PhaseLoop:
!ifdef SkipCharRom {
   lda vPhase
   cmp #1
   beq NextPhase
}
!ifdef NoBank {
   ldx vPhase
   lda TblPhNoBank,x
   beq NextPhase
}
   lda #0
   sta vMode
ModeLoop:
   jsr RunPhase
   inc vMode
   lda vMode
   cmp #NumModes
   bcc ModeLoop
NextPhase:
   inc vPhase
   lda vPhase
   cmp #NumPhases
   bcc PhaseLoop
   jmp PassLoop

   ;*************************************************************************************
Exit:
   ldx vSP
   txs
   lda #CfgDefault
   sta CPUPortReg
   jsr Speed1MHz
   lda vSavedCtlA
   sta CIA2CtlA
   lda #0
   sta CursorBlink
   +Plot RowFooter, 0
   lda #<MsgStopped
   ldy #>MsgStopped
   jsr PrintString
   cli
   rts

   ;*************************************************************************************
SetupVectors:
   ;NMI/IRQ vectors in the RAM under the KERNAL, for the phases that bank the KERNAL out
   lda #CfgAllRAM
   sta CPUPortReg
   lda #<JustRTI
   sta $fffa
   sta $fffe
   lda #>JustRTI
   sta $fffb
   sta $ffff
   lda #CfgDefault
   sta CPUPortReg
   rts
JustRTI:
   rti

   ;*************************************************************************************
CheckC128:
   ;$D030 bit 1 is a real bit on the C128's VIC-IIe (reads 0); a C64's VIC-II reads $FF there
   lda VICSpeedReg
   and #%00000010
   beq @ok
   +Plot RowSpeed, 0
   lda #<MsgNotC128
   ldy #>MsgNotC128
   jsr PrintString
   jmp Exit
@ok
   rts

   ;*************************************************************************************
DrawScreen:
   lda #<MsgTitle
   ldy #>MsgTitle
   jsr PrintString
   +Plot RowHeader, 0
   lda #<MsgHeader
   ldy #>MsgHeader
   jsr PrintString
   lda #0
   sta vPhase
@phaseRow
   lda vPhase
   clc
   adc #RowPhase0
   tax
   ldy #0
   clc
   jsr SetCursor
   lda #ChrLtGrey
   jsr SendChar
   +PrintIdx TblPhaseName, vPhase
   lda #0
   sta vMode
@cell
   lda #CellIdle
   jsr SetCell
   inc vMode
   lda vMode
   cmp #NumModes
   bcc @cell
   inc vPhase
   lda vPhase
   cmp #NumPhases
   bcc @phaseRow
   +Plot RowFooter, 0
   lda #<MsgFooter
   ldy #>MsgFooter
   jsr PrintString
   rts

   ;*************************************************************************************
   CellIdle = 0
   CellOK   = 1
   CellFail = 2
   CellRun  = 3

SetCell:
   ;A = cell state; cell for vPhase / vMode
   sta vCellState
   lda vPhase
   clc
   adc #RowPhase0
   tax
   ldy vMode
   lda TblCellCol,y
   tay
   clc
   jsr SetCursor
   +PrintIdx TblCell, vCellState
   rts

   ;*************************************************************************************
ShowCounts:
   +Plot RowCounts, 0
   lda #<MsgPass
   ldy #>MsgPass
   jsr PrintString
   lda vPassHi
   jsr PrintHexByte
   lda vPassLo
   jsr PrintHexByte
   lda vErrLogged
   bne @bad
   lda #<MsgErrsOK
   ldy #>MsgErrsOK
   jmp @prErrs
@bad
   lda #<MsgErrsBad
   ldy #>MsgErrsBad
@prErrs
   jsr PrintString
   lda vErrHi
   jsr PrintHexByte
   lda vErrLo
   jsr PrintHexByte
   lda vErrLogged
   bne +
   rts
+  +Plot RowErr, 0
   lda #<MsgFirstErr
   ldy #>MsgFirstErr
   jsr PrintString
   +Plot RowErr+1, 0
   +PrintIdx TblPhaseName, vErrPhase
   +Plot RowErr+2, 0
   +PrintIdx TblMode, vErrMode
   lda #<MsgAddr
   ldy #>MsgAddr
   jsr PrintString
   lda vErrAddrHi
   jsr PrintHexByte
   lda vErrAddrLo
   jsr PrintHexByte
   lda #<MsgExp
   ldy #>MsgExp
   jsr PrintString
   lda vErrPhase        ;ROM errors are 16-bit checksums, RAM errors one byte
   cmp #FirstRamPhase
   bcs +
   lda vErrExpHi
   jsr PrintHexByte
+  lda vErrExp
   jsr PrintHexByte
   lda #<MsgGot
   ldy #>MsgGot
   jsr PrintString
   lda vErrPhase
   cmp #FirstRamPhase
   bcs +
   lda vErrGotHi
   jsr PrintHexByte
+  lda vErrGot
   jsr PrintHexByte
   +Plot RowErr+3, 0
   lda #<MsgPat
   ldy #>MsgPat
   jsr PrintString
   +PrintIdx TblPatName, vErrPat
   rts

   ;*************************************************************************************
PrintStatus:
   ;Two NOW lines: phase, then mode / pattern / step. Also the RUN/STOP check.
   jsr CheckStop
   +Plot RowNow, 0
   lda #<MsgNow
   ldy #>MsgNow
   jsr PrintString
   +PrintIdx TblPhaseName, vPhase
   +Plot RowNow+1, 0
   lda #<MsgNow2
   ldy #>MsgNow2
   jsr PrintString
   +PrintIdx TblMode, vMode
   lda #<MsgPat
   ldy #>MsgPat
   jsr PrintString
   +PrintIdx TblPatName, vPatIdx
   +PrintIdx TblOp, vOp
   rts

CheckStop:
   lda #$7f             ;keyboard column 7
   sta CIA1PortA
   lda CIA1PortB
   ldx #$ff
   stx CIA1PortA
   and #$80             ;row 7 = RUN/STOP
   bne @notPressed
   jmp Exit
@notPressed
   rts

   ;*************************************************************************************
Speed2MHz:
   lda VICSpeedReg
   ora #$01
   sta VICSpeedReg
   rts

Speed1MHz:
   lda VICSpeedReg
   and #$fe
   sta VICSpeedReg
   rts

SpeedSet:
   ;A = 1 for 2MHz, 0 leaves 1MHz
   beq @done
   jsr Speed2MHz
@done
   rts

   ;*************************************************************************************
TimeLoop:
   ;Fixed-length CPU loop timed with CIA2 timer A. The CIA stays on the 1MHz clock, so the
   ;same loop reads about half as many ticks at 2MHz. Result in vTLo/vTHi.
   lda #0
   sta CIA2CtlA
   lda #$ff
   sta CIA2TimerALo
   sta CIA2TimerAHi
   lda #%00010001       ;force load, start, continuous
   sta CIA2CtlA
   ldy #8
@outer
   ldx #0
@inner
   dex
   bne @inner
   dey
   bne @outer
   lda #0
   sta CIA2CtlA
   sec
   lda #$ff
   sbc CIA2TimerALo
   sta vTLo
   lda #$ff
   sbc CIA2TimerAHi
   sta vTHi
   rts

SpeedCheck:
   sei                  ;prints above re-enabled IRQs (see RunPhase); keep the jiffy IRQ out of the timing
   jsr TimeLoop
   lda vTLo
   sta vT1Lo
   lda vTHi
   sta vT1Hi
   jsr Speed2MHz
   jsr TimeLoop
   jsr Speed1MHz
   lda vTLo
   sta vT2Lo
   lda vTHi
   sta vT2Hi
   ;|2*T2 - T1| < T1/4 ? (badlines cost ~6% at 1MHz and nothing at 2MHz, so 2*T2 runs under T1)
   lda vT2Lo
   asl
   sta vTmpLo
   lda vT2Hi
   rol
   sta vTmpHi
   sec
   lda vTmpLo
   sbc vT1Lo
   sta vDLo
   lda vTmpHi
   sbc vT1Hi
   sta vDHi
   bcs @absDone
   sec
   lda #0
   sbc vDLo
   sta vDLo
   lda #0
   sbc vDHi
   sta vDHi
@absDone
   lda vT1Hi
   lsr
   sta vLimHi
   lda vT1Lo
   ror
   sta vLimLo
   lsr vLimHi
   ror vLimLo
   lda vDLo
   cmp vLimLo
   lda vDHi
   sbc vLimHi
   lda #0
   rol                  ;A = carry: set when D >= limit
   eor #1               ;1 = ratio OK (D < limit)
   sta vSpeedOK
   +Plot RowSpeed, 0
   lda #<MsgSpeedChk
   ldy #>MsgSpeedChk
   jsr PrintString
   lda vT1Hi
   jsr PrintHexByte
   lda vT1Lo
   jsr PrintHexByte
   lda #<MsgSpeedChk2
   ldy #>MsgSpeedChk2
   jsr PrintString
   lda vT2Hi
   jsr PrintHexByte
   lda vT2Lo
   jsr PrintHexByte
   lda vSpeedOK
   beq @bad
   lda #<MsgRatioOK
   ldy #>MsgRatioOK
   jmp PrintString
@bad
   lda #<MsgRatioBad
   ldy #>MsgRatioBad
   jsr PrintString
   +Plot RowWarn, 0
   lda #<MsgWarn
   ldy #>MsgWarn
   jsr PrintString
   jmp Exit             ;a 2MHz column that ran at 1MHz would read as a pass

   ;*************************************************************************************
RunPhase:
   ;one phase in the mode in vMode
   ldx vMode
   lda TblWriteFast,x
   sta vWriteFast
   lda TblVerifyFast,x
   sta vVerifyFast
   lda #0
   sta vPhaseFail
   ldx vPhase
   lda TblPhStart,x
   sta vPhStart
   lda TblPhCount,x
   sta vPhCount
   lda TblPhCfg,x
   sta vPhCfg
   lda vPhase
   cmp #FirstRamPhase
   bcs @ram
   lda vMode
   cmp #FirstCrossMode
   bcc +
   rts                  ;ROM cross-speed cells stay ----
+  lda #CellRun
   jsr SetCell
   lda #PatCksum
   sta vPatIdx
   lda #OpRead
   sta vOp
   jsr PrintStatus
   lda vVerifyFast
   jsr SpeedSet
   sei                  ;CHROUT's screen path ends with CLI, so every print re-enables IRQs; with I/O
   lda vPhCfg           ;  banked out CIA1's jiffy IRQ could never be acknowledged and the CPU would crawl
   sta CPUPortReg
   jsr RomChecksum
   lda #CfgDefault
   sta CPUPortReg
   jsr Speed1MHz
   jsr RomCompare
   jmp @finish

@ram
   lda #CellRun
   jsr SetCell
   lda #0
   sta vPatIdx
@pattern
   lda #OpWrite
   sta vOp
   jsr PrintStatus
   lda vWriteFast
   jsr SpeedSet
   sei                  ;CHROUT's screen path ends with CLI, so every print re-enables IRQs; with I/O
   lda vPhCfg           ;  banked out CIA1's jiffy IRQ could never be acknowledged and the CPU would crawl
   sta CPUPortReg
   jsr FillRegion
   lda #CfgDefault
   sta CPUPortReg
   jsr Speed1MHz
   lda #OpVerify
   sta vOp
   jsr PrintStatus
   lda vVerifyFast
   jsr SpeedSet
   sei                  ;CHROUT's screen path ends with CLI, so every print re-enables IRQs; with I/O
   lda vPhCfg           ;  banked out CIA1's jiffy IRQ could never be acknowledged and the CPU would crawl
   sta CPUPortReg
   jsr VerifyRegion
   lda #CfgDefault
   sta CPUPortReg
   jsr Speed1MHz
   inc vPatIdx
   lda vPatIdx
   cmp #NumPatterns
   bcc @pattern

@finish
   lda vPhaseFail
   beq @ok
   lda #CellFail
   jmp @setCell
@ok
   lda #CellOK
@setCell
   jsr SetCell
   jmp ShowCounts

   OpWrite  = 0
   OpVerify = 1
   OpRead   = 2

   ;*************************************************************************************
   ;Everything from here to the end of the region routines runs with $01 = vPhCfg, which can
   ;bank out I/O and both ROMs: RAM only, no KERNAL/BASIC calls.

RegionInit:
   lda #0
   sta ptrLo
   lda vPhStart
   sta ptrHi
   lda vPhCount
   sta vPagesLeft
   rts

GetPat:
   ;expected byte for pattern vPatIdx at ptrHi:Y -> A. X trashed, Y kept.
   ldx vPatIdx
   cpx #4
   bcc @const
   cpx #6
   bcs @walk
   tya
   eor ptrHi
   cpx #5
   bne @done
   eor #$ff
@done
   rts
@const
   lda TblPatConst,x
   rts
@walk
   tya
   and #7
   tax
   lda TblWalk,x
   rts

FillRegion:
   jsr RegionInit
@page
   ldy #0
@nextbyte
   jsr GetPat
   sta (ptrLo),y
   iny
   bne @nextbyte
   inc ptrHi
   dec vPagesLeft
   bne @page
   rts

VerifyRegion:
   jsr RegionInit
@page
   ldy #0
@nextbyte
   jsr GetPat
   sta vExp
   lda (ptrLo),y
   cmp vExp
   beq @ok
   jsr RecordError
@ok
   iny
   bne @nextbyte
   inc ptrHi
   dec vPagesLeft
   bne @page
   rts

RomChecksum:
   jsr RegionInit
   lda #0
   sta vCkLo
   sta vCkHi
@page
   ldy #0
@nextbyte
   asl vCkLo
   rol vCkHi
   bcc @noCarry
   inc vCkLo            ;rotate: bit 15 goes to bit 0
@noCarry
   lda vCkLo
   eor (ptrLo),y
   sta vCkLo
   iny
   bne @nextbyte
   inc ptrHi
   dec vPagesLeft
   bne @page
   rts

RomCompare:
   ;first 1MHz result per ROM becomes the reference; every later result must match it
!ifdef CharRomAsIO {
   lda vPhase
   cmp #1
   bne +
   rts                  ;the I/O block is not stable, nothing to compare
+
}
   ldx vPhase
   lda vRefSet,x
   bne @compare
   lda vMode
   bne @done
   lda #1
   sta vRefSet,x
   txa
   asl
   tax
   lda vCkLo
   sta vRefCk,x
   lda vCkHi
   sta vRefCk+1,x
@done
   rts
@compare
   txa
   asl
   tax
   lda vCkLo
   cmp vRefCk,x
   bne @bad
   lda vCkHi
   cmp vRefCk+1,x
   beq @done
@bad
   lda vRefCk,x
   sta vExp
   lda vRefCk+1,x
   sta vExpHi
   lda vCkHi
   sta vGotHi
   lda vPhStart         ;address shown = start of the ROM
   sta ptrHi
   ldy #0
   lda vCkLo
   jsr RecordError
   rts

RecordError:
   ;A = got (low byte), vExp/vExpHi = expected, vGotHi = got high byte (ROM only),
   ;ptrHi:Y = address. Keeps the first one for the screen.
   sta vGot
   lda #1
   sta vPhaseFail
   inc vErrLo
   bne @counted
   inc vErrHi
   bne @counted
   lda #$ff             ;saturate
   sta vErrLo
   sta vErrHi
@counted
   lda vErrLogged
   bne @done
   lda #1
   sta vErrLogged
   sty vErrAddrLo
   lda ptrHi
   sta vErrAddrHi
   lda vExp
   sta vErrExp
   lda vExpHi
   sta vErrExpHi
   lda vGot
   sta vErrGot
   lda vGotHi
   sta vErrGotHi
   lda vPhase
   sta vErrPhase
   lda vMode
   sta vErrMode
   lda vPatIdx
   sta vErrPat
@done
   rts

   ;*************************************************************************************
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
   jmp PrintHexNibble

PrintHexNibble:
   ;Print value stored in lower nible acc in hex
   and #$0f
   cmp #$0a
   bpl +
   clc
   adc #'0'
   jmp SendChar
+  clc
   adc #'a'-$0a
   jmp SendChar

   ;*************************************************************************************
   ;Tables

   ;phase:     0 BASIC ROM, 1 CHAR ROM, 2 KERNAL ROM, 3-7 RAM
TblPhStart: !byte $a0, $d0, $e0, $20, $a0, $c0, $d0, $e0
TblPhCount: !byte 32,  16,  32,  128, 32,  16,  16,  31    ;pages
!ifdef NoBank {
TblPhCfg:   !byte CfgDefault, CfgDefault, CfgDefault, CfgDefault, CfgDefault, CfgDefault, CfgDefault, CfgDefault
TblPhNoBank: !byte 1, 0, 1, 1, 0, 1, 0, 0   ;1 = runs without touching $01
} else { !ifdef CharRomAsIO {
TblPhCfg:   !byte CfgDefault, CfgDefault, CfgDefault, CfgAllRAM, CfgAllRAM, CfgAllRAM, CfgAllRAM, CfgAllRAM
} else {
TblPhCfg:   !byte CfgDefault, CfgChar, CfgDefault, CfgAllRAM, CfgAllRAM, CfgAllRAM, CfgAllRAM, CfgAllRAM
} }

   ;mode:        1MHZ 2MHZ W2V1 W1V2
TblWriteFast:  !byte 0, 1, 1, 0
TblVerifyFast: !byte 0, 1, 0, 1
TblCellCol:    !byte 20, 25, 30, 35

TblPatConst: !byte $00, $ff, $55, $aa
TblWalk:     !byte $01, $02, $04, $08, $10, $20, $40, $80

TblPhaseName: !word MsgPh0, MsgPh1, MsgPh2, MsgPh3, MsgPh4, MsgPh5, MsgPh6, MsgPh7
TblPatName:   !word MsgPa0, MsgPa1, MsgPa2, MsgPa3, MsgPa4, MsgPa5, MsgPa6, MsgPa7
TblMode:      !word MsgMd0, MsgMd1, MsgMd2, MsgMd3
TblOp:        !word MsgOp0, MsgOp1, MsgOp2
TblCell:      !word MsgCell0, MsgCell1, MsgCell2, MsgCell3

   ;19 chars each so a redraw covers the previous text; cells start at column 20
MsgPh0: !tx "BASIC ROM A000+    ", 0
!ifdef CharRomAsIO {
MsgPh1: !tx "I/O D000+ (DIAG)   ", 0
} else {
MsgPh1: !tx "CHAR ROM D000+     ", 0
}
MsgPh2: !tx "KERNAL ROM E000+   ", 0
MsgPh3: !tx "RAM 2000-9FFF      ", 0
MsgPh4: !tx "RAM A000 U/BASIC   ", 0
MsgPh5: !tx "RAM C000-CFFF      ", 0
MsgPh6: !tx "RAM D000 U/IO      ", 0
MsgPh7: !tx "RAM E000 U/KERNAL  ", 0

   ;8 chars each
MsgPa0: !tx "$00     ", 0
MsgPa1: !tx "$FF     ", 0
MsgPa2: !tx "$55     ", 0
MsgPa3: !tx "$AA     ", 0
MsgPa4: !tx "ADDR    ", 0
MsgPa5: !tx "INV ADDR", 0
MsgPa6: !tx "WALK 1  ", 0
MsgPa7: !tx "CKSUM   ", 0

MsgMd0: !tx "1MHZ  ", 0
MsgMd1: !tx "2MHZ  ", 0
MsgMd2: !tx "W2V1  ", 0
MsgMd3: !tx "W1V2  ", 0

MsgOp0: !tx "WRITE ", 0
MsgOp1: !tx "VERIFY", 0
MsgOp2: !tx "READ  ", 0

MsgCell0: !tx ChrDrkGrey, "----", 0
MsgCell1: !tx ChrLtGreen, " OK ", 0
MsgCell2: !tx ChrLtRed, "FAIL", 0
MsgCell3: !tx ChrYellow, "RUN ", 0

MsgTitle:
   !tx ChrToLower, ChrClear, ChrYellow
   !tx "C128 2MHZ BUS/MEMORY TEST"
!ifdef SkipCharRom {
   !tx " (NO CHAR)"
}
!ifdef CharRomAsIO {
   !tx " (CHAR=IO)"
}
!ifdef NoBank {
   !tx " (NO BANK)"
}
   !tx ChrReturn
   !tx ChrLtGrey, "C64 MODE. W2V1 = WRITE 2MHZ, VERIFY 1MHZ", 0

MsgHeader:
   !tx ChrCyan, "PHASE               1MHZ 2MHZ W2V1 W1V2", 0

MsgSpeedChk:  !tx ChrLtGrey, "SPEED CHECK 1MHZ:$", 0
MsgSpeedChk2: !tx " 2MHZ:$", 0
MsgRatioOK:   !tx ChrLtGreen, " OK", 0
MsgRatioBad:  !tx ChrLtRed, " BAD!", 0
MsgWarn:      !tx ChrLtRed, "RATIO NOT 2:1: 2MHZ NOT ON, NOT TESTING", 0
MsgNotC128:   !tx ChrLtRed, "NOT A C128: NO VIC-IIE AT $D030", 0

MsgPass:    !tx ChrWhite, "PASS $", 0
MsgErrsOK:  !tx ChrLtGreen, "   ERRORS $", 0
MsgErrsBad: !tx ChrLtRed, "   ERRORS $", 0

MsgNow:  !tx ChrWhite, "NOW: ", 0
MsgNow2: !tx ChrWhite, "     ", 0
MsgPat:  !tx "PAT: ", 0

MsgFirstErr: !tx ChrLtRed, "FIRST ERROR:", 0
MsgAddr:     !tx "ADDR $", 0
MsgExp:      !tx " EXP $", 0
MsgGot:      !tx " GOT $", 0

MsgFooter:  !tx ChrLtGrey, "RUN/STOP=EXIT  DON'T PRESS RESTORE", 0
MsgStopped: !tx ChrLtGrey, "STOPPED.                          ", ChrReturn, ChrToUpper, 0

   ;*************************************************************************************
   ;Variables (after the code, below $2000 which is where the RAM test starts)
vSP:         !byte 0
vSavedCtlA:  !byte 0
vPassLo:     !byte 0
vPassHi:     !byte 0
vErrLo:      !byte 0
vErrHi:      !byte 0
vErrLogged:  !byte 0
vErrAddrLo:  !byte 0
vErrAddrHi:  !byte 0
vErrExp:     !byte 0
vErrExpHi:   !byte 0
vErrGot:     !byte 0
vErrGotHi:   !byte 0
vErrPhase:   !byte 0
vErrMode:    !byte 0
vErrPat:     !byte 0
vPhase:      !byte 0
vMode:       !byte 0
vWriteFast:  !byte 0
vVerifyFast: !byte 0
vPatIdx:     !byte 0
vOp:         !byte 0
vPhStart:    !byte 0
vPhCount:    !byte 0
vPhCfg:      !byte 0
vPagesLeft:  !byte 0
vPhaseFail:  !byte 0
vCellState:  !byte 0
vExp:        !byte 0
vExpHi:      !byte 0
vGot:        !byte 0
vGotHi:      !byte 0
vCkLo:       !byte 0
vCkHi:       !byte 0
vRefSet:     !byte 0, 0, 0
vRefCk:      !word 0, 0, 0
vTLo:        !byte 0
vTHi:        !byte 0
vT1Lo:       !byte 0
vT1Hi:       !byte 0
vT2Lo:       !byte 0
vT2Hi:       !byte 0
vTmpLo:      !byte 0
vTmpHi:      !byte 0
vDLo:        !byte 0
vDHi:        !byte 0
vLimLo:      !byte 0
vLimHi:      !byte 0
vSpeedOK:    !byte 0

ProgramEnd:
!if ProgramEnd > $2000 {
   !error "program overlaps the RAM test region at $2000"
}
