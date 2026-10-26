; SPDX-License-Identifier: MIT
; The transport of a GEOS disk driver served by the TR block device (docs/TR_Block_Device.md). The firmware
; puts this over the stock driver's turbo code and points the driver's vectors here; build with acme.

!ifndef ORG { ORG = $9000 }

r1L = $04
r1H = $05
r4  = $0a
curDevice = $ba
curDrive  = $8489
driveType = $904e

TYPE_1581   = 3
DIR_1581_TRACK = 40
HEADER_SWAP = $8c
HEADER_FIRST = 4
HEADER_END = $1d

; offsets from BASE_GEOS_RAM's page
REG_CMD = $08
REG_ADDR0 = $09
REG_ADDR1 = $0a
REG_PAGE = $0b
REG_TOKEN = $0c
REG_STATUS = $0d
REG_ECHO = $0e
WINDOW = $40
WINDOW_PAGES = 4
WINDOW_SIZE = 64

CMD_READ = 1
CMD_WRITE = 2
STATUS_BUSY = $80
STATUS_DONE = $5a
STATUS_BAD_ADDRESS = $e2

ERR_INV_TRACK = 2
ERR_DEV_NOT_FOUND = 13

VIC_IRQ_FLAGS = $d019
VIC_IRQ_ENABLE = $d01a
CIA1_ICR = $dc0d
CIA2_ICR = $dd0d
MASK_ALL = $7f

BASE_GEOS_RAM = $df

* = ORG

InitForIO
        php
        pla
        sta savedP
        sei
        lda $01
        sta saved01
        lda #$36
        sta $01
        lda VIC_IRQ_ENABLE
        sta savedIrqEnable
        lda #0
        sta VIC_IRQ_ENABLE
        lda #MASK_ALL
        sta VIC_IRQ_FLAGS
        sta CIA1_ICR
        sta CIA2_ICR
        rts

DoneWithIO
        sei
        lda CIA1_ICR
        lda CIA2_ICR
        lda savedIrqEnable
        sta VIC_IRQ_ENABLE
        lda saved01
        sta $01
        lda savedP
        pha
        plp
        rts

EnterTurbo
        ldx #0
        txa
ExitTurbo
PurgeTurbo
        rts

NewDisk
VerWriteBlock
SendTSBytes
GetDOSError
        ldx #0
        ldy #0
        rts

ChangeDiskDevice
        sta curDrive
        sta curDevice
        ldx #0
        rts

ReadBlock
        jsr readSector
        cpx #0
        bne finished
        jsr swapHeader
finished
        ldy #0
        rts

WriteBlock
        jsr swapHeader
        jsr writeSector
        txa
        pha
        jsr swapHeader
        pla
        tax
        ldy #0
        rts

ReadLink
        jsr findDevice
        bcs missing
        lda #CMD_READ
        jsr runCommand
        cpx #0
        bne finished
        lda #0
        ldx #REG_PAGE
        jsr putReg
        ldx #WINDOW
        jsr getStable
        ldy #0
        sta (r4),y
        ldx #WINDOW + 1
        jsr getStable
        iny
        sta (r4),y
        ldx #0
        ldy #0
        rts

missing
        ldx #ERR_DEV_NOT_FOUND
        ldy #0
        rts

readSector
        jsr findDevice
        bcs missing
        lda #CMD_READ
        jsr runCommand
        cpx #0
        bne readDone
        ldy #0
        lda #0
        sta page
readPage
        lda page
        ldx #REG_PAGE
        jsr putReg
        ldx #0
windowRead
        lda BASE_GEOS_RAM * 256 + WINDOW,x
        cmp BASE_GEOS_RAM * 256 + WINDOW,x
        bne windowRead
        sta (r4),y
        iny
        inx
        cpx #WINDOW_SIZE
        bne windowRead
        inc page
        lda page
        cmp #WINDOW_PAGES
        bne readPage
        ldx #0
readDone
        rts

writeSector
        jsr findDevice
        bcs missing
        ldy #0
        lda #0
        sta page
writePage
        lda page
        ldx #REG_PAGE
        jsr putReg
        ldx #0
windowWrite
        lda (r4),y
        sta BASE_GEOS_RAM * 256 + WINDOW,x
        cmp BASE_GEOS_RAM * 256 + WINDOW,x
        beq windowStored
        cmp BASE_GEOS_RAM * 256 + WINDOW,x
        bne windowFault
windowStored
        iny
        inx
        cpx #WINDOW_SIZE
        bne windowWrite
        inc page
        lda page
        cmp #WINDOW_PAGES
        bne writePage
        lda #CMD_WRITE
        jmp runCommand
windowFault
        ldx #ERR_DEV_NOT_FOUND
        rts

; A = command; returns the GEOS error in X
runCommand
        sta command
        inc token
        bne tokenReady
        inc token
tokenReady
        lda token
        ldx #REG_TOKEN
        jsr putReg
        lda r1L
        ldx #REG_ADDR0
        jsr putReg
        lda r1H
        ldx #REG_ADDR1
        jsr putReg
        lda command
        ldx #REG_CMD
        jsr putReg
        lda #0
        sta patience
        sta patience + 1
poll
        ldx #REG_STATUS
        jsr getReg
        sta seen
        jsr getReg
        cmp seen
        bne again
        cmp #STATUS_BUSY
        bne decided
again
        inc patience
        bne poll
        inc patience + 1
        bne poll
        ldx #ERR_DEV_NOT_FOUND
        rts
decided
        pha
        ldx #REG_ECHO
        jsr getReg
        cmp token
        beq ours
        pla
        jmp again
ours
        pla
        cmp #STATUS_DONE
        beq succeeded
        cmp #STATUS_BAD_ADDRESS
        beq badTrack
        ldx #ERR_DEV_NOT_FOUND
        rts
succeeded
        ldx #0
        rts
badTrack
        ldx #ERR_INV_TRACK
        rts

; the 1581 header sector keeps its fields where the 1541's are
swapHeader
        lda driveType
        cmp #TYPE_1581
        bne swapped
        lda r1L
        cmp #DIR_1581_TRACK
        bne swapped
        lda r1H
        bne swapped
        ldy #HEADER_FIRST
swapField
        lda (r4),y
        sta scratch
        tya
        clc
        adc #HEADER_SWAP
        tay
        lda (r4),y
        pha
        lda scratch
        sta (r4),y
        tya
        sec
        sbc #HEADER_SWAP
        tay
        pla
        sta (r4),y
        iny
        cpy #HEADER_END
        bne swapField
swapped
        rts

; carry clear when the TRB id answers
findDevice
        ldy #2
probe
        lda BASE_GEOS_RAM * 256 + $10,y
        cmp BASE_GEOS_RAM * 256 + $10,y
        bne probe
        cmp trbId,y
        bne absent
        dey
        bpl probe
        clc
        rts
absent
        sec
        rts

getReg
        lda BASE_GEOS_RAM * 256,x
        rts

; a read that comes back the same twice; on some machines the TR's byte arrives with the last one's bits ORed in
getStable
        jsr getReg
        sta seen
        jsr getReg
        cmp seen
        bne getStable
        rts

putReg
        sta BASE_GEOS_RAM * 256,x
        rts

trbId   !byte $54, $52, $42
savedP  !byte 0
saved01 !byte 0
scratch !byte 0
command !byte 0
seen    !byte 0
page    !byte 0
token   !byte 0
patience !word 0
savedIrqEnable !byte 0
