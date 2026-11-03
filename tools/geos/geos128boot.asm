; SPDX-License-Identifier: MIT
; A C128 function ROM that starts GEOS 128 with no drive: it pulls the boot image the firmware built through the
; TR block device's BOOT command (docs/TR_Block_Device.md) into RAM bank 1, then enters it as GEOS's own ROM
; loader does. geos128boot.mjs turns it into Source/Teensy/GEOS/Geos128Boot.h; build with acme.
; The firmware fills in bootLoad, bootPages and bootEntry, and puts the block device's page in every register address.

MMU_CR = $ff00
MMU_RAM_SHARE = $d506
SHARE_BOTTOM_8K = $06
KEEP_BANK_0 = $3f
BANK_0_IO = $3e
BANK_1_IO = $7e
VIC_CONTROL = $d011
SCREEN_OFF = $00
SCREEN_ON = $1b
VIC_CLOCK = $d030
CLOCK_1MHZ = 0
VIC_BORDER = $d020
curDevice = $ba
DEVICE_8 = 8

; in the common RAM the loader shares between the banks, where #63's loader ran
LOADER = $1300

!ifndef BLOCK_PAGE { BLOCK_PAGE = $df }
BLOCK = BLOCK_PAGE * 256
REG_CMD = BLOCK + $08
REG_ADDR0 = BLOCK + $09
REG_ADDR1 = BLOCK + $0a
REG_PAGE = BLOCK + $0b
REG_TOKEN = BLOCK + $0c
REG_STATUS = BLOCK + $0d
REG_ECHO = BLOCK + $0e
WINDOW = BLOCK + $40
WINDOW_SIZE = 64
WINDOW_PAGES = 4
CMD_BOOT = 5
STATUS_BUSY = $80
STATUS_DONE = $5a
; about ten seconds of polling at 1 MHz, for the card to answer the first page
PATIENCE_HIGH = 8

dst = $02
page = $04
token = $06
patience = $07

* = $8000
        nop                     ; the kernal's cold start call lands here
        nop
        nop
        jmp cold                ; warm start
        !byte $ff               ; start at power-up and reset
        !byte $43, $42, $4d     ; "CBM": a function ROM

; IRQs are on when the kernal calls in, and this ROM's top page holds the CPU vectors
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
bootLoad  !word 0
bootPages !word 0
bootEntry !word 0

start   sei
        lda #BANK_0_IO
        sta MMU_CR
        lda #SHARE_BOTTOM_8K
        sta MMU_RAM_SHARE
        lda #SCREEN_OFF
        sta VIC_CONTROL
        lda #CLOCK_1MHZ
        sta VIC_CLOCK
        lda #BANK_1_IO
        sta MMU_CR
        lda bootLoad
        sta dst
        lda bootLoad + 1
        sta dst + 1
        lda #0
        sta page
        sta page + 1
        sta token

nextPage
        lda page
        cmp bootPages
        bne fetch
        lda page + 1
        cmp bootPages + 1
        bne fetch
        lda #SCREEN_ON
        sta VIC_CONTROL
        lda #DEVICE_8
        sta curDevice
        jmp (bootEntry)

fetch   inc token
        bne +
        inc token
+       lda token
        sta REG_TOKEN
        lda page
        sta REG_ADDR0
        lda page + 1
        sta REG_ADDR1
        lda #0
        sta patience
        sta patience + 1
        sta patience + 2
        lda #CMD_BOOT
        sta REG_CMD
wait    inc patience
        bne +
        inc patience + 1
        bne +
        inc patience + 2
        lda patience + 2
        cmp #PATIENCE_HIGH
        beq failed
+       lda REG_STATUS
        cmp REG_STATUS
        bne wait
        cmp #STATUS_BUSY
        beq wait
        cmp #STATUS_DONE
        bne failed
        lda REG_ECHO
        cmp REG_ECHO
        bne wait
        cmp token
        bne wait

        ldx #0
quarter stx REG_PAGE
        ldy #0
-       lda WINDOW,y
        cmp WINDOW,y
        bne -
        sta (dst),y
        iny
        cpy #WINDOW_SIZE
        bne -
        clc
        lda dst
        adc #WINDOW_SIZE
        sta dst
        bcc +
        inc dst + 1
+       inx
        cpx #WINDOW_PAGES
        bne quarter
        inc page
        bne +
        inc page + 1
+       jmp nextPage

; the boot image could not be read: a flashing border says so
failed  lda #SCREEN_ON
        sta VIC_CONTROL
-       inc VIC_BORDER
        jmp -
}
loaderEnd
!if loaderEnd - loaderRom > 256 { !error "the cold start copies the loader as one page" }

; the top of ROMH, which the CPU vectors read while this ROM is mapped in: an IRQ restarts the cold start, an NMI
; returns
* = $fff9
return  rti
        !word return            ; NMI
        !word return            ; reset
        !word cold              ; IRQ
