

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   
   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2025                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of .run
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of .run
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of .run
   !byte SysAddress%10+$30         ; Calculate 4th digit of .run
   !byte $00                       ; BASIC end of line marker
BasicEnd    
   !word $0000                     ; BASIC end of program

SysAddress
   ;jmp Init
   
   !src "source\C64Defs.asm"

Init:
   ;screen setup:     
   lda #PokeDrkGrey
   sta BorderColorReg
   lda #PokeBlack
   sta BackgndColorReg
   
   lda #<MsgTODClockChecker
   ldy #>MsgTODClockChecker
   jsr PrintString 

Start:   
   sei             
   lda #$00
   tax
   tay
   sta $dd08       ; TO2TEN start TOD - in case it wasn't running
-  cmp $dd08       ; poll TO2TEN for change
   bne changed
   inx
   bne -
   iny
   bne -
   ;fail, no change detected
   lda #<MsgFailSlow
   ldy #>MsgFailSlow
   jsr PrintString 
   jmp finish
changed:
   txa
   pha
   tya
   pha
   and #$f0
   bne pass
   lda #<MsgFailFast
   ldy #>MsgFailFast
   jmp printtimer
pass:
   lda #<MsgPass
   ldy #>MsgPass
printtimer:
   jsr PrintString 
   pla
   jsr PrintHexByte
   pla
   jsr PrintHexByte
finish:
   cli
   jmp Start
   
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
   bpl + 
   clc
   adc #'0'
   jmp ++
+  clc
   adc #'a'-$0a
++ jsr SendChar
   rts   
   
MsgTODClockChecker:    
   !tx ChrReturn, ChrReturn, ChrToLower
   !tx "CIA TOD Clock Checker", ChrReturn
   !tx 0

MsgPass:
   !tx ChrReturn
   !tx "Pass: $"
   !tx 0
   
MsgFailSlow:
   !tx ChrReturn
   !tx "Fail too slow: $ffff"
   !tx 0

MsgFailFast:
   !tx ChrReturn
   !tx "Fail too fast: $"
   !tx 0

EOF:
   !byte 0
   

