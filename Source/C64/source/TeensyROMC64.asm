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
   !src "source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "source\CommonDefs.i" ;Common between crt loader and main code in RAM


;********************************   Cartridge begin   ********************************   

; 8k Cartridge/ROM   ROML $8000-$9FFF
;16k Cartridge/ROM   ROML $8000-$BFFF (Replaces BASIC ROM)
;* = $9fff      ; set a byte to cause fill up to -$9fff (or $bfff if 16K)
;   !byte 0     ; only have to fill it when converting to CRT for emulation
   
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
   ;jsr $e422            ; Power-up message / NEW command (requires BASIC ROM)
   ldx #$fb
   txs                  ; Reduce stack pointer for BASIC
   
;******************************* Main Code Start ************************************   
   ;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg

   lda #<MsgCartBanner
   ldy #>MsgCartBanner
   jsr $ab1e   ;PrintString; can use basic since this is an 8k cart
;   ;jsr PrintString  
;   sta PtrAddrLo
;   sty PtrAddrHi
;   ldy #0
;-  lda (PtrAddrLo), y
;   beq MainCopyToRAM
;   jsr SendChar
;   iny
;   bne -
      
MainCopyToRAM
   lda #>MainCode
   ldy #<MainCode   
   sta PtrAddrHi
   sty PtrAddrLo 
   ldx #>EndMainCode
   
   lda #>MainCodeRAMStart
   ldy #<MainCodeRAMStart   
   sta Ptr2AddrHi
   sty Ptr2AddrLo 

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

   jmp MainCodeRAMStart

MsgCartBanner:    
   !tx ChrClear, ChrToLower, ChrPurple, ChrRvsOn
   !tx "     *** Sensorium Embedded 2024 ***    "  ;*VERSION*
   !tx ChrRvsOff, ChrBlack, 0 ;hide sid load msg
      
MainCode = *
   !binary "build\MainMenu.bin"
EndMainCode = *
   
EndOfAllCartCode = *
   !byte 0
