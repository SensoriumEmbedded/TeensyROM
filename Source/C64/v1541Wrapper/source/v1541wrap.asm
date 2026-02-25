
   !convtab pet   ;key in and text out conv to PetSCII throughout
   r0   = $fb
   r0L  = $fb
   r0H  = $fc
   r1   = $fd
   r1L  = $fd
   r1H  = $fe


   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2026                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of SysAddress
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of SysAddress
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of SysAddress
   !byte SysAddress%10+$30         ; Calculate 4th digit of SysAddress
   !byte $00                       ; BASIC end of line marker
BasicEnd    
   !word $0000                     ; BASIC end of program

SysAddress
   ;Load r0 with location of PRG to copy
   lda #>StartOfCode
   ldy #<StartOfCode   
   sta r0H
   sty r0L 

   ldy #0      ;initialize index counter
   
   ;Load r1 with destination from first 2 bytes
   lda (r0),y ;read low byte
   sta r1L  ;r1 points to where to copy to
   sta smcEcecuteLocation+1  ;(Low) smcEcecuteLocation points to where to execute from
   inc r0L  ;move pointer to next byte (not the Index)
   bne +
   inc r0H  ;next page
   
+  lda (r0),y ;read high byte
   sta r1H 
   sta smcEcecuteLocation+2  ;(high)
   inc r0L  ;move pointer to next byte (not the Index)
   bne +
   inc r0H  ;next page
   
+  ldx #>EndOfCode ;load x reg with ending page
   inx ;last page+1, will copy ((EndOfCode-StartOfCode) | 0xFF) bytes
   
   ;Copy from (r0 Lo/Hi) to (r1 Lo/Hi), x reg is last page to copy
-  lda (r0),y  ;copy from 
   sta (r1),y  ;copy to
   iny         ;increment the index
   bne -       ;copy full page
   inc r0H     ;inc source page 
   inc r1H     ;inc dest page 
   cpx r0H     ;last page?
   bne -

   ; execute code where coppied to:
smcEcecuteLocation
   jmp 0000   

StartOfCode
  !binary "source/v-1541.19k.sl.prg"
  ;!binary "source/v-1541lo.19k.sl.prg"
EndOfCode
   !byte $00 ;byte to mark end address in build report


