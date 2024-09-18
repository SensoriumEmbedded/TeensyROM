#importonce

/*

    Copy a block of memory from one location to another

    r0: source
    r1: target
    r2: number of bytes

    Pilfered from https://github.com/mist64/geos/blob/2090bca64fc2627beef5c8232aafaec61f1f5a53/kernal/memory/memory2.s#L123

*/
MemCopy:
    lda r2L
    ora r2H
    beq !l7+
    PushW(r0)
    PushB(r1H)
    PushB(r2H)
    PushB(r3L)
!l1:
    CmpW(r0, r1)
!l2:
    bcs !l3+
    bcc !l8+
!l3:
    ldy #0
    lda r2H
    beq !l5+
!l4:
    lda (r0), y
    sta (r1), y
    iny
    bne !l4-
    inc r0H
    inc r1H
    dec r2H
    bne !l4-
!l5:
    cpy r2L
    beq !l6+
    lda (r0), y
    sta (r1), y
    iny
    bra !l5-
!l6:
    PopB(r3L)
    PopB(r2H)
    PopB(r1H)
    PopW(r0)
!l7:
    rts

!l8:
    clc
    lda r2H
    adc r0H
    sta r0H
    clc
    lda r2H
    adc r1H
    sta r1H
    ldy r2L
    beq !lA+
!l9:
    dey
    lda (r0), y
    sta (r1), y
    tya
    bne !l9-
!lA:
    dec r0H
    dec r1H
    lda r2H
    beq !l6-
!lB:
    dey
    lda (r0), y
    sta (r1), y
    tya
    bne !lB-
    dec r2H
    bra !lA-

/*

    Fill a block of memory with a byte

    r0: start address
    r1: number of bytes
    r2L: byte to write

*/
MemFill:
    ldy #$00
!l1:
    lda r2L
    sta (r0), y
    Dec16(r1)
    CmpWI(r1, $0000)
    beq !end+
    iny
    bne !l1-
    inc r0H
    jmp !l1-

!end:
    rts
