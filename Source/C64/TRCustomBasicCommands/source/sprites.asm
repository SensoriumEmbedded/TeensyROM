/*

    Change a sprite's attribute

    r0L = sprite number (0-7)
    r0H = attribute to change (SPR_VISIBLE, SPR_X_EXPAND, SPR_Y_EXPAND, SPR_HMC, SPR_PRIORITY)
    r1L = value for the attribute
    - Sprite visibility (SPR_SHOW, SPR_HIDE)
    - X/Y expansion (SPR_NORMAL, SPR_EXPAND)
    - Priority (SPR_FG, SPR_BG)
    - Hires or Multicolor (SPR_HIRES, SPR_MULTICOLOR)
*/
ChangeSpriteAttribute:
  ldy r0H
  ldx r0L
  lda BitMaskPow2, x
  ldx r1L
  cpx #ENABLE
  beq !set_flag+
  eor #$ff
  and vic.SP0X, y
  jmp !clear_flag+
!set_flag:
  ora vic.SP0X, y
!clear_flag:
  sta vic.SP0X, y

  rts

/*

    Position a sprite anywhere on the screen. This also takes care of the MSIGX register, which is used to push the sprite
    beyond an X position of 255.

    r0L = sprite number (0-7)
    r1: x position (0-319)
    r2L: y position (0-199)

    Also trashes r3
*/
PositionSprite:
  lda r0L
  mult2
  tay
  stb r2L:vic.SP0Y, y
  stb r1L:r3L
  lda r1H
  adc #$00
  sta r3H
  stb r3L:vic.SP0X, y
  ldx r0L
  lda BitMaskPow2, x
  eor #$ff
  and vic.MSIGX
  tay
  lda #$01
  and r3H
  beq !no_msb+
  tya
  ora BitMaskPow2, x
  tay

!no_msb:
  sty vic.MSIGX

!return:
  rts

/*

    Quick lookup table for sprite numbers

*/
BitMaskPow2:
  .byte %00000001
  .byte %00000010
  .byte %00000100
  .byte %00001000
  .byte %00010000
  .byte %00100000
  .byte %01000000
  .byte %10000000