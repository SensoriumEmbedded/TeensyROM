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


   ;this code is relocated to PRGLoadStartReloc and run from there as it 
   ;could overwrite all upper RAM.  Will not execute correctly from here (string pointers are offset)
   
   ;stream PRG file from TeensyROM to RAM and set end of prg/start of variables
   ;assumes TeensyROM is set up to transfer, PRG selected and waited to complete
   ;rRegStrAvailable+IO1Port is zero when inactive/complete

PRGLoadStart:
   ;jsr $A644 ;new   
   lda rRegStrAddrHi+IO1Port
   sta PtrAddrHi
   lda rRegStrAddrLo+IO1Port   
   sta PtrAddrLo
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
   sta wRegControl+IO1Port 
   lda #<(MsgRunning - PRGLoadStart + PRGLoadStartReloc) ; corrected for reloc
   ldy #>(MsgRunning - PRGLoadStart + PRGLoadStartReloc)
   jsr $ab1e   ;PrintString
   ;as is done at $A52A    https://skoolkid.github.io/sk6502/c64rom/asm/A49C.html#A52A
   jsr $a659	;reset execution to start, clear variables and flush stack
   jsr $a533	;rebuild BASIC line chaining
   ;Also see https://codebase64.org/doku.php?id=base:runbasicprg
   jmp $a7ae ;BASIC warm start/interpreter inner loop/next statement (Run)
   ;jmp (BasicWarmStartVect)  
   
MsgRunning:
   !tx ChrReturn, ChrReturn, "Running...", ChrReturn, 0
MsgOverflow:
   !tx ChrReturn, "Overflow!", ChrReturn, 0
   

PRGLoadEnd = *
