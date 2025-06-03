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

PrintSerialString:
   ;load Acc with RegSerialStringSelect # that will be serialized out from TR port
   sta rwRegSerialString+IO1Port   ;selects message and resets to start of string
PrintSerialStringLoaded: ;message already selected
-  lda rwRegSerialString+IO1Port
   beq +
   jsr SendChar
   jmp -
+  rts
 
PrintString:
   ;replaces BASIC routine jsr $ab1e when unavailable
   ;prints from C64 RAM location:
   ;   usage: lda #<MsgM2SPolyMenu,  ldy #>MsgM2SPolyMenu,  jsr PrintString 
   sta smcPrintStringAddr+1
   sty smcPrintStringAddr+2
   ldy #0
   
-  jsr GetNextChar
   cmp #0
   beq ++   ;zero terminates string
   cmp #EscC
   bne +
   ; process special escape char
   
   jsr GetNextChar
   tax
   lda TblEscC, x
   sta $0286  ;set text color
   jmp -
   
+  jsr SendChar
   jmp -
GetNextChar  ;load next char in to acc and and increment pointer (Y then smc)
smcPrintStringAddr
   lda $fffe, y
   iny
   bne ++
   inc smcPrintStringAddr+2
   ;bne - No roll-over protection
++ rts   



PrintBanner:
   lda #<MsgBanner
   ldy #>MsgBanner
   jsr PrintString 
   lda #rsstVersionNum
   jsr PrintSerialString
   sec
   jsr SetCursor ;read column into y reg
   lda #ChrSpace ;spaces to the end of the line
-  jsr SendChar  
   iny
   cpy #40
   bne -
   lda #ChrRvsOff
   jsr SendChar   
   rts
   
DisplayTime:
   ldx #1 ;row
   ldy #29  ;col
   clc
   jsr SetCursor
   lda TblEscC+EscTimeColor
   sta $0286  ;set text color
   lda TODHoursBCD ;latches time in regs (stops incrementing)
   tay ;save for re-use

smc24HourClockDisp   
   ldx #00 ; x reg holds 24(non-zero) vs 12(zero) hr display 
   beq Print12Hours

   ;print hours (24 hour format)
   lda #ChrSpace ;two spaces if 24 hour mode
   jsr SendChar
   jsr SendChar
   tya
   cmp #$12  ;12am special case
   bne +     
   ;print 0
   lda #$00
   beq ++ ;always jump
+  cmp #$92  ;12pm special case
   beq +     ;print as-is (without PM bit)
   and #$80  ;check am/pm bit
   beq +     ;print am hour as-is
   ; pm, but not 12pm, add 12 in decimal mode
   tya
   and #$1f
   sed
   clc
   adc #$12
   cld
   jmp ++
+  tya
   and #$1f
++ jsr PrintHexByte
   jmp Print_mm_ss
   
Print12Hours ;print hours (12 hour format)
   and #$1f
   bne nz   ;if hours is 0, make it 12...
   tya
   ora #$12
   tay ;re-save for re-use
nz tya     ;first digit is 1 or blank
   and #$10
   bne +
   lda #ChrSpace
   jmp ++
+  lda #'1'
++ jsr SendChar
   tya
   and #$0f  ;ones of hours
   jsr PrintHexNibble

Print_mm_ss   ;print :mm:ss  read 10ths
   lda #':'
   jsr SendChar
   lda TODMinBCD
   jsr PrintHexByte
   lda #':'
   jsr SendChar
   lda TODSecBCD
   jsr PrintHexByte
   ;lda #'.'
   ;jsr SendChar
   lda TODTenthSecBCD ;have to read 10ths to release latch
   ;jsr PrintHexNibble
   cpx #00 ; x reg holds 24(non-zero) vs 12(zero) hr display 
   bne +++
   ;print am/pm if in 12 hour mode
   tya ;am/pm (pre latch release)
   and #$80
   bne +
   lda #'a'
   jmp ++
+  lda #'p'
++ jsr SendChar
   lda #'m'
   jsr SendChar
+++rts

PrintIntByte: 
   ;Print acc as int, no padding
;Hundreds 
   ldx #0
   cmp #10
   bmi Ones ;skip 100s/10s if <10
-  cmp #100
   bmi +     ;loop until below 100
   sec       ;set to subtract without carry
   sbc #100   ;subtract 100
   inx      ;100s place in X
   jmp -

+  cpx #0
   beq Tens  ;skip 100s if <100
   tay  ;preserve remainder in Y
   txa
   clc
   adc #'0'
   jsr SendChar
   tya

Tens   
   ldx #0
-  cmp #10
   bmi +     ;loop until below 10
   sec       ;set to subtract without carry
   sbc #10   ;subtract 10
   inx
   jmp -

+  tay  ;preserve remainder
   txa
   clc
   adc #'0'
   jsr SendChar
   tya
   
Ones
   clc
   adc #'0'
   jsr SendChar

   rts
   
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

PrintOnOff:
   ;Print "On" or "Off" based on Zero flag
   ;uses A and Y regs
   bne +
   lda #<MsgOff
   ldy #>MsgOff
   jmp ++
+  lda #<MsgOn
   ldy #>MsgOn
++ jsr PrintString 
   rts

;Print4CharTableHiNib
;   lsr
;   lsr
;   lsr
;   lsr ; move to lower nibble
Print4CharTable:   
;prints 4 chars from a table of continuous 4 char sets (no termination)
;X=table base lo, y=table base high, acc=index to item# (63 max)
;   and #0xfc 
   stx smc4CharTableAddr+1
   sty smc4CharTableAddr+2
   asl
   asl  ;mult by 4
   tay
smc4CharTableAddr
-  lda $fffe,y
   jsr SendChar   ;type (4 chars)
   iny
   tya
   and #3
   bne -
   rts
   
