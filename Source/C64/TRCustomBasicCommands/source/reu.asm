#importonce

// The REU's registers are mapped to $df00-$df0a
.label reustatus    = $df00
.label reucommand   = $df01
.label c64address   = $df02
.label reuaddress   = $df04
.label reubank      = $df06
.label transferlen  = $df07
.label reucontrol   = $df0a

/*

    Set up and execute either a STASH or a FETCH

    Requires:
    a - the value to stuff into reucommand. $90 for a stash, $91 for a fetch
    r0 - Start address on the C64 side
    r1 - Start address on the REU side
    r2 - Transfer length
    r3L - The REU bank to FETCH/STASH

    TODO:
    - Use a fast, non-destructive method of determining whether an REU is present
    - Determine whether the specified bank in r3L is valid based on the detected REU

*/
REUCommon:
    pha
    lda r0L
    sta c64address
    lda r0H
    sta c64address + $01
    lda r1L
    sta reuaddress
    lda r1H
    sta reuaddress + $01
    lda r2L
    sta transferlen
    lda r2H
    sta transferlen + $01
    lda r3L
    sta reubank
    pla
    sta reucommand

    rts

/*

    Copy data from the C64 to the REU

    r0: Starting C64 address to copy from
    r1: Starting REU address to copy to
    r2: Number of bytes to transfer
    r3L: The REU bank to use

*/
REUStash:
    lda #$90
    jsr REUCommon
    rts

/*

    Copy data from the REU to the C64

    r0: Starting C64 address to copy to
    r1: Starting REU address to copy from
    r2: Number of bytes to transfer
    r3L: The REU bank to copy from

*/
REUFetch:
    lda #$91
    jsr REUCommon
    rts

//--------------------------------------------------------------------------------------------------------------------
// REUDETECT v1.0a ( REU : 1700, 1764, 1750, 1024Kb and 2048Kb )
// ---------------------------------------------------------------------------------------------------------------------
// Overview : Detection of REC and RAM-Type : 0 = 1700 (64Kbx1) or 16 = 1764/50 or bigger(256Kbx1).
//            Write to Registers 2-5 and compare.
//            Write 33 banks with "messy datas".
//            Fetch bank, inc bank-counter (banks $1500) if own dummy-bytes not found.
//            Stash bank, write own dummy bytes for later comparing and fetch next bank.
//            Skip bankcheck if dummy byte-chain found. Detection is finished.
//            Read available banks, evaluate and drop some text on the screen.
// ---------------------------------------------------------------------------------------------------------------------
// 21. Januar 2005 M. Sachse (cbmhardware/People of Liberty)
//
// E-Mail : info(at)cbmhardware.de
//
// 22. Januar 2005 : Bugfix, add ram-type detection 1764, Vice and C64 compatibility, messy code reworked;
//
// ------------------------------------------------------------------------------------------------------------------
// GPL
//
// This program is free software; you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
//---------------------------------------------------------------------------------------------------------------------
// Source Code for ACME Cross-Assembler :
//---------------------------------------------------------------------------------------------------------------------

.byte $00,$0c,$08,$0a,$00,$9e,$32,$30,$36,$32,$00,$00,$00,$00

banks:
    .word $0000
config:
    .byte 252

//--------------------------------------------------
// Detect REU 1700,1764/50 or 1/2MB
//--------------------------------------------------
reudetect:
    lda #$00
    sta banks
    sta reustatus
    cmp reustatus
    beq noreu
unsafe:
    lda reustatus
    and #$10         // check bit 4 for REU mem
    cmp #$10         // 16 = 256Kbx1
    beq regcheck     // yes, touch registers
    bne l1           // no, 1700 ?
l1:
    lda reustatus
    and #$10         // check bit 4 for REU mem
    cmp #$00
    beq r1700        // reu 1700 found
    bne noreu        // no ram-type, no reu, no fun ...
regcheck:
    lda reustatus
    ldx #$02
loop1:
    txa
    sta $df00, x      // write to registers 2-5
    inx
    cpx #$05
    bne loop1
    ldx #$02
loop2:
    txa
    cmp $df00, x
    bne noreu
    inx
    cpx #$05
    bne loop2
    jmp rinit
r1700:
    lda #<reutext
    ldy #>reutext
    jmp $ab1e
//-------------------------------------------------
rinit:
    ldx #00          // 1764 wake up
rinit2:
    lda #$80         // stash
    sta config
    lda #$12         // write some crap in ...
    sta c64hi+1
    stx bank+1
    jsr main
    inx
    cpx #$21         //  ... 33 banks into somewhere
    bne rinit2
    jmp action
noreu:
    lda #<notext
    ldy #>notext
    jmp $ab1e

//--------------------------------------------------
// Count banks
//--------------------------------------------------
action:
    lda reustatus
    ldx #$00
    stx bank+1       // reset bank counter
check:
    lda #$81         // fetch : transfer to C64 : $1300
    sta config
    lda #$13         // C64 : $1300
    sta c64hi+1
    jsr main
    lda #$80         // stash
    sta config
    lda #$0a         // write dummy bytes from $0900
    sta c64hi+1
    jsr main
    jsr bankcheck    // check for existing ram banks
    inx
    cpx #$21         // try 33
    stx bank+1
    bne check
//--------------------------------------------------

    lda banks        // banks found ?
    cmp #4
    bne j1
r1764:
    lda #4
    sta banks
    lda #<text1764
    ldy #>text1764
    jmp $ab1e
j1:
    cmp #8
    bne j2
r512:
    lda #<reut512
    ldy #>reut512
    jmp $ab1e
j2:
    cmp #16
    bne j3
r1024:
    lda #<reut1024
    ldy #>reut1024
    jmp $ab1e
j3:
    cmp #20
    beq r1764
j4:
    cmp #32
    bne j6
r2048:
    lda #<reut2048
    ldy #>reut2048
    jmp $ab1e
j6:
    lda #<reuunk
    ldy #>reuunk
    jmp $ab1e

//--------------------------------------------------
// Bank Check
//--------------------------------------------------
bankcheck:
    ldy #$00
l2:
    lda $0A00,y
l6:
    cmp $1300,y
    bne l5            // bank found ?
l3:
    iny
    cpy #16
    bne l2            // loop
end:
    ldy #00
    lda #00
delete:
    sta $1300,y       // delete buffer
    iny
    cpy #16
    bne delete
    rts
l5:
    inc banks         // bank found (inc), border color change and exit
    rts

//--------------------------------------------------
// Bytes and text
//--------------------------------------------------

reutext:
    .text "REU 1700 : 128KB DETECTED"
    .byte $00
text1764:
    .text "REU 1764 : 256KB DETECTED"
    .byte $00
reut512:
    .text "REU 1750 : 512KB DETECTED"
    .byte $00
reut1024:
    .text "REU 1024KB DETECTED"
    .byte $00
reut2048:
    .text "REU 2048KB DETECTED"
    .byte $00
reuunk:
    .text "REU PORT DETECTED - BANK ERROR"
    .byte $00
notext:
    .text "NO REU"
    .byte $00

//--------------------------------------------------
// REU TRANSFER ROUTINE
//--------------------------------------------------

main:
    lda config
    sta reustatus+1
    lda #$00
    sta reustatus+2
c64hi:
    lda #$09
    sta reustatus+3
    lda #$00
    sta reustatus+4
    sta reustatus+5
bank:
    lda #0
    sta reustatus+6      // Bank
rbytes:
    lda #16
    sta reustatus+7      // 16 Bytes
    lda #$00
    sta reustatus+8
irq:
    lda #$00
    sta reustatus+9
    lda #$00
    sta reustatus+10
    lda $1
    pha
    lda #$30           // All RAM
    sei
    sta $1
    sta $ff00
    pla
    sta $1
    cli
    rts
