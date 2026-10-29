; SPDX-License-Identifier: MIT
;
; GEOS input driver (128, or 64 assembled at its own address) for a USB mouse on a TeensyROM (USBMouse.c has the registers).

!cpu 6502

mouseOn       = $30     ; bit 7: the pointer is on
pressFlag     = $39
mouseXPos     = $3a     ; 16 bits
mouseYPos     = $3c
graphMode     = $3f     ; bit 7: 80 columns
mouseData     = $8505   ; bit 7: button up
inputData     = $8506   ; a joystick direction, or $ff

MOUSE_BIT     = $20     ; in pressFlag: mouseData changed
BUTTON_UP     = $80
NO_DIRECTION  = $ff
OFF_TOP       = 228     ; the kernal reads a mouseYPos this high as above the top

; beside GeoRAM, and beside the REU, whose unused registers read $FF across $DF0B-$DF1F
PAGE_BESIDE_GEORAM = $df
PAGE_BESIDE_REU = $de
regX          = 0
regY          = 1
regButtons    = 2
regID         = 3
regBlockID    = $10     ; the TR block device's, which shares the page and tells it from an REU's
LEFT_BUTTON   = $01

!ifndef MOUSE_BASE { MOUSE_BASE = $fd00 }
        * = MOUSE_BASE          ; where GEOS loads an input driver

        jmp MouseInit
        jmp SlowMouse
        jmp UpdateMouse
        jmp SetMouse

lastButton  !byte BUTTON_UP
deltaX      !word 0
deltaY      !byte 0

MouseInit
        lda #0
        sta mouseXPos+1
        lda #8
        sta mouseXPos
        sta mouseYPos
SlowMouse
SetMouse
        rts

; The block device's ID picks the page: beside an REU, $DF03-$DF05 are its address registers and can hold anything.
;  Carry set: what reads there is not a mouse; otherwise every register read points at the page.
FindRegs
        ldx #PAGE_BESIDE_GEORAM
        ldy #2
-       lda PAGE_BESIDE_GEORAM*256+regBlockID,y
        cmp trbID,y
        bne +
        dey
        bpl -
        bmi ++
+       ldx #PAGE_BESIDE_REU
++      stx mouseID+2
        stx readX+2
        stx readY+2
        stx readButtons+2
        ldy #2
-
mouseID lda PAGE_BESIDE_GEORAM*256+regID,y
        cmp trmID,y
        bne +
        dey
        bpl -
        clc
        rts
+       sec
        rts

trmID   !text "TRM"
trbID   !text "TRB"

UpdateMouse
        jsr FindRegs
        bcs noMouse
readX   lda PAGE_BESIDE_GEORAM*256+regX
        sta deltaX
readY   lda PAGE_BESIDE_GEORAM*256+regY
        sta deltaY
        bit mouseOn
        bmi readButtons
noMouse rts                     ; movement while the pointer is off is dropped

readButtons
readButtons
        lda PAGE_BESIDE_GEORAM*256+regButtons
        and #LEFT_BUTTON
        beq +
        lda #0                  ; pressed
        beq ++
+       lda #BUTTON_UP
++      cmp lastButton
        beq ++
        sta lastButton
        sta mouseData
        lda pressFlag
        ora #MOUSE_BIT
        sta pressFlag
++

        ldx #0
        lda deltaX
        bpl +
        dex
+       stx deltaX+1
!ifndef GEOS64 {
        bit graphMode
        bpl +
        asl deltaX              ; 640 pixels across in 80 columns
        rol deltaX+1
+
}
        clc
        lda mouseXPos
        adc deltaX
        sta mouseXPos
        lda mouseXPos+1
        adc deltaX+1
        sta mouseXPos+1         ; the kernal clamps it to the screen

        lda deltaY
        clc
        adc mouseYPos
        bit deltaY
        bmi up
        bcs bottom
        cmp #OFF_TOP
        bcc storeY
bottom  lda #OFF_TOP-1          ; below the bottom, for the kernal to clamp
        bne storeY
up      bcs storeY
        lda #$ff                ; above the top
storeY  sta mouseYPos

; inputData: 0 right, 1 up-right, 2 up ... 7 down-right, as a joystick driver gives
        ldx #1                  ; column: 0 left, 1 none, 2 right
        lda deltaX+1
        bmi +
        ora deltaX
        beq ++
        inx
        bne ++
+       dex
++      lda deltaY              ; row: 0 up, 1 none, 2 down
        beq +
        bmi up1
        inx
        inx
        inx
+       inx
        inx
        inx
up1     lda directions,x
        sta inputData
        rts

directions
        !byte 3, 2, 1           ; up
        !byte 4, NO_DIRECTION, 0
        !byte 5, 6, 7           ; down
