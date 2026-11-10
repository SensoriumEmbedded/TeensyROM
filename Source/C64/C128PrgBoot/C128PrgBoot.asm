; SPDX-License-Identifier: MIT
; The firmware (C128Prg.ino) serves the first $400 bytes as ROML, mirrored through $8000-$BFFF, and the last page as
; ROMH, mirrored through $C000-$FFFF, and patches the PRG's end address into prgEnd.
;
; The ID byte asks to start after power-up and reset, so the kernal calls the cold start from PHOENIX, which BASIC's
; cold start runs last: BASIC has already done its NEW, which would otherwise clear the program at $1C01.

   !src "../MainMenuCRT/source/Menu_Regs.i"

IO1Port = $de00
MMU_CR = $ff00
SYSTEM_BANK = $00               ; BASIC, kernal and I/O over bank 0
KEEP_BANK_0 = $3f
BANK_0_IO = $3e
BANK_0_ALL_RAM = $3f
BASIC_START = $1c01
TEXT_TOP = $1210
KEYBOARD_BUFFER = $034a
KEYBOARD_COUNT = $d0
BASIC_RESTART = $0a00           ; BASIC's cold start points it at its warm start before calling PHOENIX
ChrReturn = 13

; in the common RAM below BASIC, which a $1C01 program leaves alone
LOADER = $1300
ROML_SIZE = $400

dst = $02

* = $8000
        nop                     ; the kernal's cold start call lands here
        nop
        nop
        jmp cold                ; warm start
        !byte $ff               ; start at power-up and reset
        !byte $43, $42, $4d     ; "CBM": a function ROM

cold    sei
        lda MMU_CR
        and #KEEP_BANK_0
        sta MMU_CR
        ldx #0
-       lda loaderRom,x
        sta LOADER,x
        inx
        bne -
        jmp start

loaderRom
!pseudopc LOADER {
prgEnd  !word 0

; I/O is mapped in to read the stream and mapped out to store each byte, so a program reaching past $CFFF lands in
; RAM and not in the VIC, SID and CIAs
start   lda #BANK_0_IO
        sta MMU_CR
        lda rRegStreamData+IO1Port      ; the load address, which the firmware has checked
        lda rRegStreamData+IO1Port
        lda #<BASIC_START
        sta dst
        lda #>BASIC_START
        sta dst + 1
        ldy #0
next    lda dst
        cmp prgEnd
        bne +
        lda dst + 1
        cmp prgEnd + 1
        beq loaded
+       lda rRegStreamData+IO1Port
        ldx #BANK_0_ALL_RAM
        stx MMU_CR
        sta (dst),y
        ldx #BANK_0_IO
        stx MMU_CR
        inc dst
        bne next
        inc dst + 1
        jmp next

loaded  lda prgEnd
        sta TEXT_TOP
        lda prgEnd + 1
        sta TEXT_TOP + 1
        lda #rCtlVanishROM
        sta wRegControl+IO1Port
        lda #rCtlRunningPRG
        sta wRegControl+IO1Port
-       lda rRegIOHSwapPoll+IO1Port
        cmp #rihsReady
        bne -

        lda #SYSTEM_BANK
        sta MMU_CR
        ldx #runKeysEnd - runKeys - 1
-       lda runKeys,x
        sta KEYBOARD_BUFFER,x
        dex
        bpl -
        lda #runKeysEnd - runKeys
        sta KEYBOARD_COUNT
        jmp (BASIC_RESTART)

runKeys !text "RUN", ChrReturn
runKeysEnd
}
loaderEnd
!if loaderEnd - loaderRom > 256 { !error "the cold start copies the loader as one page" }
!if loaderRom != $8021 { !error "prgEnd moved: C128PRG_END_PARAM in C128Prg.ino must follow it" }
!if loaderEnd > $8000 + ROML_SIZE { !error "the firmware serves only ROML_SIZE bytes of ROML" }

!fill $8000 + ROML_SIZE - *, $ff

; ROMH: one page, the CPU vectors, which are read from here while the kernal has this ROM mapped
!pseudopc $ff00 {
        !fill $f9, $ff
return  rti
        !word return            ; NMI
        !word return            ; reset
        !word return            ; IRQ
}
