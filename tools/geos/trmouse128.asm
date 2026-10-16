; SPDX-License-Identifier: MIT
;
; GEOS 128 input driver for a USB mouse on a TeensyROM (USBMouse.c has the registers).

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

regX          = 0
regY          = 1
regButtons    = 2
regID         = 3
LEFT_BUTTON   = $01

        * = $fd00               ; where GEOS 128 loads an input driver

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

; The registers are at $DF00 beside GeoRAM and $DE00 beside the REU; the ID tells which.
;  $DF03-$DF05 are REU address registers, which have no side effects when read.
FindRegs
        ldx #2
-       lda $df00+regID,x
        cmp trID,x
        bne +
        dex
        bpl -
        lda #$df
        bne ++
+       lda #$de
++      sta readX+2
        sta readY+2
        sta readButtons+2
        rts

trID    !text "TRM"

UpdateMouse
        bit mouseOn
        bmi +
        rts
+       jsr FindRegs

readButtons
        lda $df00+regButtons
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

readX
        lda $df00+regX
        sta deltaX
        ldx #0
        cmp #0
        bpl +
        dex
+       stx deltaX+1
        bit graphMode
        bpl +
        asl deltaX              ; 640 pixels across in 80 columns
        rol deltaX+1
+       clc
        lda mouseXPos
        adc deltaX
        sta mouseXPos
        lda mouseXPos+1
        adc deltaX+1
        sta mouseXPos+1         ; the kernal clamps it to the screen

readY
        lda $df00+regY
        sta deltaY
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
