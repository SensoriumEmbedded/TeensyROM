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


   ;stream PRG file from TeensyROM to RAM and set end of prg/start of variables
   ;assumes TeensyROM is set up to transfer, PRG selected and waited to complete
   ;rRegStrAvailable+IO1Port is zero when inactive/complete
   
   ;this code is relocated to PRGLoadStartReloc and run from there as loaded content 
   ;  $033c-03fb Cassette IO buffer, 192 bytes.  Current size: 116 bytes (find PrgLoaderSize in build report)
   ;could overwrite all upper RAM.  Will not execute correctly from here (string pointers are offset)

PRGLoadStart:
   ;jsr $A644 ;new   
   lda rRegStreamData+IO1Port
   sta PtrAddrLo
   lda rRegStreamData+IO1Port
   sta PtrAddrHi
   ldy #0   ;zero offset
   
-  lda rRegStrAvailable+IO1Port ;are we done?
   beq +   ;exit the loop
   lda rRegStreamData+IO1Port ;read from rRegStreamData+IO1Port increments address & checks for end
   sta (PtrAddrLo), y 
   iny
   bne -
   inc PtrAddrHi
   bne -
   ;good luck if we get to here... Trying to overflow and write to zero page
   lda #<(MsgOverflow - PRGLoadStart + PRGLoadStartReloc) ; corrected for reloc 
   ldy #>(MsgOverflow - PRGLoadStart + PRGLoadStartReloc)
   jsr $ab1e   ;PrintString
   jmp (BasicWarmStartVect)
   
   ;we're in caps/graphics mode for these messsages, use all lower case:
   ;placing these here instead of the end in case we want to erase them before running PRG
MsgRunning:
   !tx ChrReturn, "running...", ChrReturn, 0
MsgOverflow:
   !tx ChrReturn, "overflow!", ChrReturn, 0

   ;last byte of prg (+1) = y+PtrAddrLo/Hi, store this in 2D/2E
+  ldx PtrAddrHi
   tya
   clc
   adc PtrAddrLo
   bcc +
   inx
+  sta $2d  ;start of BASIC variables pointer (Lo)
   stx $2e  ; (Hi)
   sta $ae  ;End of load address (Lo)
   stx $af  ; (Hi)
   
   lda #rCtlRunningPRG    ;let TR know we're done, change IO handler
   sta wRegControl+IO1Port  ;can't wait for it because handler is changing...
   ;static delay instead of wait. 
   ; without this, race condition fails under following conditions:
   ;  PAL clock, NFC on (slowing down polling), start Cynthcart
   ; because Cynthcart starts and queries IO space, 
   ;  causing corruption when TR handler still loaded (internet time query added)
   ; 6*256*(2+2)/985250Hz(PAL) = ~6.3mS
   ldy #$06    ;<=3 fails, 4 intermittent, >=5 passes
   ldx #$ff
-  dex
   bne -
   dey
   bne -
   
   lda #<(MsgRunning - PRGLoadStart + PRGLoadStartReloc) ; corrected for reloc
   ldy #>(MsgRunning - PRGLoadStart + PRGLoadStartReloc)
   jsr $ab1e   ;PrintString
   ;as is done at $A52A    https://skoolkid.github.io/sk6502/c64rom/asm/A49C.html#A52A
   jsr $a659	;reset execution to start, clear variables and flush stack
   jsr $a533	;rebuild BASIC line chaining
   ;Also see https://codebase64.org/doku.php?id=base:runbasicprg
   
   ;determine if JiffyDOS is installed
      ;.e477  20 20 20 20 20 4a 49 46  ;      jif
      ;.e47f  46 59 44 4f 53 20 56 36  ; fydos v6
   lda $e47c
   cmp #$4a   ;j of "jiffydos"
   bne +
   ;JiffyDOS Specific:
   ;jsr $e453	; init JiffyDOS command vectors
   lda #$72	; Set up JiffyDOS function key vector
   sta $b0	; to $f672
   lda #$f6
   sta $b1

+  lda #$08  ;Set Current Device Number to 8 (floppy drive). 0=kbd, 1=tape
   sta $ba   ;makes file browsers load from floppy by default, could adversely effect games that want to be loaded from tape

;   ; erasing the tracks of this code 
;   ;   haven't seen this needed, but available just in case
;
;   lda #$00
;   sta PtrAddrLo
;   sta PtrAddrHi
;   sta Ptr2AddrLo
;   sta Ptr2AddrHi
;   
;   ldy #tohere - PRGLoadStart  ;erase from start of tape buff "tohere"
;tohere  ;code from this point to PRGLoadEnd will remain...
;-  dey
;   sta PRGLoadStartReloc,y
;   bne -

   jmp $a7ae ;BASIC warm start/interpreter inner loop/next statement (Run)
   ;jmp (BasicWarmStartVect)  
      
PRGLoadEnd = *
