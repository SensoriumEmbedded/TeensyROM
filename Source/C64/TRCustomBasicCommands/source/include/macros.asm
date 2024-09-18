#importonce

/*
Push everything onto the stack. This lets us do whatever we want with the
registers and put it back the way it was before returning. This is mostly
used by the raster interrupt routine, but can be used anywhere.
*/
.macro PushStack() {
  php
  pha
  txa
  pha
  tya
  pha
}

/*
Sets the registers and processor status back to the way they were
*/
.macro PopStack() {
  pla
  tay
  pla
  tax
  pla
  plp
}

/*
Toggle a flag
*/
.macro Toggle(address) {
  lda address
  eor #ENABLE
  sta address
}

/*
Disable a flag
*/
.macro Disable(address) {
  stb #DISABLE:address
}

/*
Enable a flag
*/
.macro Enable(address) {
  stb #ENABLE:address
}

/*
Copy the contents of the source address to the target address
*/
.macro CpyW(source, target) {
  stb source:target
  stb source + $01:target + $01
}

/*
Load a word into a target address
*/
.macro CpyWI(word, target) {
  stb #<word:target
  stb #>word:target + $01
}

#if MEMORY

/*
Copy a block of memory
*/
.macro CpyM(source, target, length) {
  CpyWI(source, r0)
  CpyWI(target, r1)
  CpyWI(length, r2)

  jsr CopyMemory
}

/*
Fill a chunk of memory with an immediate value
*/
.macro FillMI(value, target, length) {
  stb #value:r0L
  CpyWI(target, r1)
  CpyWI(length, r2)

  jsr FillMemory
}

/*
Fill a chunk of memory with a value from memory
*/
.macro FillM(value, target, length) {
  stb value:r0L
  CpyWI(target, r1)
  CpyWI(length, r2)

  jsr FillMemory
}

#endif

/*
Do a 16-bit increment of a memory location
*/
.macro Inc16(word) {
  inc word
  bne !return+
  inc word + $01
!return:
}

/*
Do a 16-bit decrement of a memory location
*/
.macro Dec16(word) {
  lda word
  bne !return+
  dec word + $01
!return:
  dec word
}

/*
Compare 2 bytes
*/
.macro CmpB(byte1, byte2) {
  lda byte1
  cmp byte2
}

/*
Compare a byte in memory with an immediate value
*/
.macro CmpBI(byte1, byte2) {
  lda byte1
  cmp #byte2
}

/*
Compare a word in memory to an immediate word value
*/
.macro CmpWI(word1, word2) {
  CmpBI(word1 + $01, >word2)
  bne !return+
  CmpBI(word1 + $00, <word2)
!return:
}

/*
Compare 2 memory words
*/
.macro CmpW(word1, word2) {
  CmpB(word1 + $01, word2 + $01)
  bne !return+
  CmpB(word1 + $00, word2 + $00)
!return:
}

/*
Load a byte and push it onto the stack
*/
.macro PushB(address) {
  lda address
  pha
}

/*
Load a word and push it onto the stack
*/
.macro PushW(address) {
  PushB(address + $01)
  PushB(address + $00)
}

/*
Pop a byte from the stack and store it
*/
.macro PopB(address) {
  pla
  sta address
}

/*
Pop a word from the stack and store it
*/
.macro PopW(address) {
  PopB(address + $00)
  PopB(address + $01)
}

#if SPRITES

#if REGISTER_MODE

/*
Set the VIC bank, the screen location and the character set location

bank: 0-3
char_address: 0-7
screen_address: 0-15
*/
.macro SetVICBank(bank, char_address, screen_address) {
  stx #bank
  sta #char_address
  sty #screen_address

  jsr SetVICBank
}

/*
Enable or disable a sprite's visibility
*/
.macro SpriteEnable(number, enabled) {
  ldx #number
  ldy #SPR_VISIBLE
  lda #enabled

  jsr ChangeSpriteAttribute
}

/*
Enable or disable a sprite's X expansion
*/
.macro SpriteXExpand(number, endis) {
  ldx #number
  ldy #SPR_X_EXPAND
  lda #endis

  jsr ChangeSpriteAttribute
}

/*
Enable or disable a sprite's Y expansion
*/
.macro SpriteYExpand(number, endis) {
  ldx #number
  ldy #SPR_Y_EXPAND
  lda #endis

  jsr ChangeSpriteAttribute
}

/*
Set a sprite's priority
*/
.macro SpritePriority(number, priority) {
  ldx #number
  ldy #SPR_PRIORITY
  lda #priority

  jsr ChangeSpriteAttribute
}

/*
Set a sprite's color mode
*/
.macro SpriteColorMode(number, mode) {
  ldx #number
  ldy #SPR_HMC
  lda #mode

  jsr ChangeSpriteAttribute
}

/*
Position a sprite
*/
.macro PositionSprite(number, xpos, ypos) {
  stb #ypos:$02
  ldx #number
  lda xpos
  ldy xpos + $01

  jsr PositionSprite
}

#else

/*
Set the VIC bank, the screen location and the character set location

bank: 0-3
char_address: 0-7
screen_address: 0-15
*/
.macro SetVICBank(bank, char_address, screen_address) {
  stb #bank:r0L
  stb #char_address:r0H
  stb #screen_address:r1L

  jsr SetVICBank
}

/*
Enable or disable a sprite's visibility
*/
.macro SpriteEnable(number, enabled) {
  stb #number:r3L
  stb #SPR_VISIBLE:r3H
  stb #enabled:r4L

  jsr ChangeSpriteAttribute
}

/*
Enable or disable a sprite's X expansion
*/
.macro SpriteXExpand(number, endis) {
  stb #number:r3L
  stb #SPR_X_EXPAND:r3H
  stb #endis:r4L

  jsr ChangeSpriteAttribute
}

/*
Enable or disable a sprite's Y expansion
*/
.macro SpriteYExpand(number, endis) {
  stb #number:r3L
  stb #SPR_Y_EXPAND:r3H
  stb #endis:r4L

  jsr ChangeSpriteAttribute
}

/*
Set a sprite's priority
*/
.macro SpritePriority(number, priority) {
  stb #number:r3L
  stb #SPR_PRIORITY:r3H
  stb #priority:r4L

  jsr ChangeSpriteAttribute
}

/*
Set a sprite's color mode
*/
.macro SpriteColorMode(number, mode) {
  stb #number:r3L
  stb #SPR_HMC:r3H
  stb #mode:r4L

  jsr ChangeSpriteAttribute
}

/*
Position a sprite
*/
.macro PositionSprite(number, xpos, ypos) {
  CpyW(xpos, r4)
  stb ypos:r5L
  stb #number:r3L

  jsr PositionSprite
}

#endif

#endif

#if TIMERS

/*
Create a timer
*/
.macro CreateTimer(number, enabled, type, frequency, location) {
  CpyWI(frequency, r0)
  CpyWI(location, r1)
  stb #type:r2L
  stb #number:r2H
  stb #enabled:r3L

  jsr CreateTimer
}

/*
Disable a timer
*/
.macro DisableTimer(number) {
  stb #number:r2H
  stb #DISABLE:r3L

  jsr EnDisTimer
}

/*
Enable a timer
*/
.macro EnableTimer(number) {
  stb #number:r2H
  stb #ENABLE:r3L

  jsr EnDisTimer
}

#endif
