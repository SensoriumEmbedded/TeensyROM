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


; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout

   ;RAM Registers:
   PtrAddrLo   = $fb
   PtrAddrHi   = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   
   ;RAM coppies:
   MainCodeRAM = $c000    ;Main execution point, 4k available.  Only using 2.42 KB as of 2/16/23
                          ;Could move to 0801 if more space needed
   SIDCodeRAM  = $1000 

;********************************   Cartridge begin   ********************************   

;8k Cartridge/ROM   ROML $8000-$9FFF
;  Could make 16k if needed
* = $9fff                     ; set a byte to cause fill up to -$9fff (or $bfff if 16K)
   !byte 0
   
* = $8000  ;Cart Start
;  jump vectors and autostart key     
   !word Coldstart    ; Cartridge cold-start vector   ;!byte $09, $80 = !word $8009
   !word Warmstart    ; Cartridge warm-start vector
   !byte $c3, $c2, $cd, $38, $30    ; CBM8O - Autostart key
;  KERNAL RESET ROUTINE
Coldstart:
   sei
   stx $d016            ; Turn on VIC for PAL / NTSC check
   jsr $fda3            ; IOINIT - Init CIA chips
   jsr $fd50            ; RANTAM - Clear/test system RAM
   jsr $fd15            ; RESTOR - Init KERNAL RAM vectors
   jsr $ff5b            ; CINT   - Init VIC and screen editor
   cli                  ; Re-enable IRQ interrupts
;  BASIC RESET  Routine
Warmstart:
   jsr $e453            ; Init BASIC RAM vectors
   jsr $e3bf            ; Main BASIC RAM Init routine
   jsr $e422            ; Power-up message / NEW command
   ldx #$fb
   txs                  ; Reduce stack pointer for BASIC
   

;******************************* Main Code Start ************************************   
   
   jsr SIDCopyToRAM
   jsr MainCopyToRAM
   jmp MainCodeRAM
   
; ******************************* Subroutines ******************************* 
MainCopyToRAM:
   lda #>MainCode
   ldy #<MainCode   
   sta PtrAddrHi
   sty PtrAddrLo 
   ldx #>EndMainCode
   
   lda #>MainCodeRAM
   ldy #<MainCodeRAM   
   sta Ptr2AddrHi
   sty Ptr2AddrLo 
   jmp CodeCopy
   
SIDCopyToRAM:
   ;have to copy SID code to RAM because it self modifies...
   lda #>SIDCode
   ldy #<SIDCode   
   sta PtrAddrHi
   sty PtrAddrLo 
   ldx #>EndSIDCode
   
   lda #>SIDCodeRAM
   ldy #<SIDCodeRAM   
   sta Ptr2AddrHi
   sty Ptr2AddrLo 
   
CodeCopy:
   ; Copy from (PtrAddrLo/Hi) to (Ptr2AddrLo/Hi), x reg is last page to copy
   inx ;last page+1, will copy ((EndCode-StartCode) | 0xFF) bytes
   ldy #0 ;initialize
-  lda (PtrAddrLo), y 
   sta (Ptr2AddrLo),y
   iny
   bne -   
   inc PtrAddrHi
   inc Ptr2AddrHi
   cpx PtrAddrHi
   bne -
   rts
   
SIDCode = *
   !binary "source\SleepDirt_extra_ntsc_1000_6581.sid.seq",, $7c+2   ;;skip header and 2 byte load address
EndSIDCode = *

MainCode = *
   !binary "build\MainMenu_C000.bin"
EndMainCode = *

   
EndOfAllCode = *
   !byte 0
