; SPDX-License-Identifier: MIT
;
; Minesweeper for the TeensyROM. It takes a 1351 mouse in port 1, a joystick in port 2, and a USB mouse
;  on the TR's USB host port while the Special IO is GeoRAM or REU+USB Mouse/Disk (USBMouse.c has its
;  registers), all at once.
;
; tools/c64/minesweeper-run.test.mjs runs this code from its committed header and reads the
;  "name = $hex" lines below, so the API_ entry points and the variables stay where they are written.

!cpu 6502

; ------------------------------------------------------------------ memory
SCREEN      = $0400
COLORRAM    = $d800
BOARD       = $2000     ; 16 rows of 32 cells
STACKROW    = $2200     ; flood fill stack: every cell is pushed at most once, so 512 entries are enough
STACKCOL    = $2400
SPRITES     = $3000
CHARSET     = $3800     ; extended color mode shows only the first 64 characters

; ------------------------------------------------------------------ zero page (BASIC's, unused while this runs)
ptr         = $02
scr         = $04
cptr        = $06
sptr        = $08
mregs       = $0a       ; the mouse registers; high byte 0 when there are none
rows        = $0c
cols        = $0d
mines       = $0e
level       = $0f
state       = $10
row         = $11
col         = $12
safeLeft    = $13       ; 16 bits: cells without a mine still covered
flags       = $15       ; 16 bits
posLo       = $17       ; pointer x, y
posHi       = $19
buttons     = $1b
seed        = $1c       ; 16 bits
spLo        = $1e
spHi        = $1f
lost        = $20
leftCol     = $21
rowMask     = $22
colMask     = $23
baseRow     = $24
baseCol     = $25
nbrIdx      = $26
nbrFn       = $27       ; 16 bits
nbrCount    = $29
need        = $2a
toPlace     = $2b
safeRow     = $2c
safeCol     = $2d
tmp         = $2e
seconds     = $2f       ; 16 bits
jifSub      = $31
lastJiffy   = $32
joyDelay    = $33
joyHeld     = $34
fireHeld    = $35
numLo       = $36
numHi       = $37
lastBtn     = $38
glyphIdx    = $39
dataIdx     = $3a
pressed     = $3b
joy         = $3c
cellv       = $3d
drawRow     = $3e
port1       = $3f       ; keyboard rows port 1 holds low, as $DC01 reads them
potX        = $40       ; the 1351's last POT readings
potY        = $41
potNew      = $42
keysLast    = $43
keysNow     = $44

KEYS        = 7         ; F1, F3, F5, Return, N, Space, Q

; ------------------------------------------------------------------ the TR's mouse page (USBMouse.c)
REG_X       = 0
REG_Y       = 1
REG_BUTTONS = 2
REG_ID      = 3
REG_BLOCKID = $10       ; the block device's "TRB": beside GeoRAM it is at $DF10, beside an REU at $DE10

; ------------------------------------------------------------------ cells
MINE        = $80
OPEN        = $40
FLAG        = $20
BOOM        = $10
COUNT       = $0f

READY       = 0         ; no mines yet: the first click places them around itself
PLAYING     = 1
WON         = 2
LOST        = 3

; ------------------------------------------------------------------ screen
TOP         = 5
FACE_ROW    = 2
FACE_COL    = 19
LABEL_ROW   = 24

BG1         = $40       ; extended color mode: bits 7-6 of a screen code pick $D021-$D024
BG2         = $80
BG3         = $c0

CH_TILE     = 0
CH_FLAG     = 27
CH_MINE     = 28
CH_WRONG    = 29
CH_HAPPY    = 30        ; and 31
CH_COOL     = 34        ; and 35
CH_DEAD     = 36        ; and 37
CH_SMILE    = 38        ; and 39
CH_FROWN    = 40        ; and 41

BLACK       = 0
WHITE       = 1
RED         = 2
CYAN        = 3
PURPLE      = 4
GREEN       = 5
BLUE        = 6
YELLOW      = 7
BROWN       = 9
LTRED       = 10
DKGREY      = 11
MDGREY      = 12
LTGREEN     = 13
LTGREY      = 15

HOLD_FRAMES = 20        ; holding fire this long flags instead of opening
JOY_FIRST   = 12
JOY_REPEAT  = 4

; ------------------------------------------------------------------ entry
API_START     = $0810
API_NEWGAME   = $0813
API_LEFT      = $0816
API_RIGHT     = $0819
API_MIDDLE    = $081c
API_FINDMOUSE = $081f
API_READMOUSE = $0822
API_READKEYS  = $0825
API_NMI       = $0828

        * = $0801
        !word .basicEnd, 2026
        !byte $9e
        !raw "2064"
        !byte 0
.basicEnd
        !word 0
        !fill API_START - *, 0

        jmp Start
        jmp NewGame
        jmp LeftClick
        jmp ToggleFlag
        jmp Chord
        jmp FindMouse
        jmp ReadMouse
        jmp ReadKeys
        jmp Nmi
!if * != API_NMI + 3 { !error "the API table moved" }

Start
        lda #DKGREY
        sta $d020
        sta $d021
        lda #LTGREY
        sta $d022
        lda #MDGREY
        sta $d023
        lda #YELLOW
        sta $d024
        jsr BuildCharset
        jsr SetupSprites
        lda #$ff                ; voice 3 noise, read at $D41B to seed the mines
        sta $d40e
        sta $d40f
        lda #$80
        sta $d412
        lda #$1e                ; screen $0400, characters $3800
        sta $d018
        lda #$5b                ; extended color mode
        sta $d011
        sei
        lda #<Irq
        sta $0314
        lda #>Irq
        sta $0315
        lda #<Nmi
        sta $0318
        lda #>Nmi
        sta $0319
        lda #$7f                ; SID's POT lines on port 1
        sta $dc00
        cli
        jsr FindMouse
        lda #0
        sta level
        sta fireHeld
        sta joyDelay
        sta joyHeld
        sta posHi
        sta posHi+1
        lda #160
        sta posLo
        lda #100
        sta posLo+1
        lda $d419
        sta potX
        lda $d41a
        sta potY
        jsr ReadButtons         ; a button or key held at launch is not a press
        sta lastBtn
        lda #$ff
        sta keysLast
        lda $a2
        sta lastJiffy
        jsr NewGame
MainLoop
        jsr WaitFrame
        jsr ReadMouse
        jsr ReadJoystick
        jsr ReadKeys
        jsr UpdateTimer
        jsr PlacePointer
        jmp MainLoop

; The kernal's IRQ without its key scan and cursor: a 1351's left button holds the keyboard row F1
;  and Space are on, and the scan's writes to $DC00 would switch SID's POT lines away from port 1.
;  UDTIM keeps the jiffy clock that times the game.
Irq
        jsr $ffea
        lda $dc0d
        jmp $ea81

; ------------------------------------------------------------------ input
!zone FindMouse
; The block device's ID picks the page, as the GEOS driver does: beside an REU, $DF03-$DF05 are its
;  address registers and can hold anything.
FindMouse
        ldx #$df
        ldy #2
-       lda $df00+REG_BLOCKID,y
        cmp TrbId,y
        bne .besideReu
        dey
        bpl -
        bmi .page
.besideReu
        ldx #$de
.page   stx mregs+1
        lda #0
        sta mregs
        ldy #REG_ID+2
-       lda (mregs),y
        cmp TrmId-REG_ID,y
        bne .none
        dey
        cpy #REG_ID
        bcs -
        rts
.none   lda #0
        sta mregs+1
        rts

TrbId   !raw "TRB"
TrmId   !raw "TRM"

!zone ReadMouse
; Both mice move the one pointer. The POTs are read first, while $DC00 has selected port 1 since the
;  last frame. Each read of the USB mouse's X or Y takes the movement since the last one.
ReadMouse
        lda $d419
        ldy potX
        jsr PotDelta
        sty potX
        ldx #0
        jsr MoveAxis
        lda $d41a
        ldy potY
        jsr PotDelta
        sty potY
        eor #$ff                ; a 1351's Y counts up as it moves up
        clc
        adc #1
        ldx #1
        jsr MoveAxis
        lda mregs+1
        beq .buttons
        ldy #REG_X
        lda (mregs),y
        ldx #0
        jsr MoveAxis
        ldy #REG_Y
        lda (mregs),y
        ldx #1
        jsr MoveAxis
.buttons
        jsr ReadButtons
        lda lastBtn
        eor #$ff
        and buttons
        sta pressed
        lda buttons
        sta lastBtn
        lsr pressed
        bcc +
        jsr LeftAtPointer
+       lsr pressed
        bcc +
        jsr RightAtPointer
+       lsr pressed
        bcc .out
        jmp MiddleAtPointer
.out    rts

!zone ReadButtons
; buttons and A = held: bit 0 left, 1 right, 2 middle. A 1351's left is port 1's fire line, its
;  right port 1's up line.
ReadButtons
        jsr ReadPort1
        lda #0
        sta buttons
        lda port1
        and #$10
        bne +
        lda #1
        sta buttons
+       lda port1
        lsr
        bcs +
        lda buttons
        ora #2
        sta buttons
+       lda mregs+1
        beq +
        ldy #REG_BUTTONS
        lda (mregs),y
        ora buttons
        sta buttons
+       lda buttons
        rts

; port1 = $DC01 with no keyboard column driven: only what port 1 holds low reads 0
ReadPort1
        lda #$ff
        sta $dc00
        lda $dc01
        sta port1
        lda #$7f
        sta $dc00
        rts

!zone PotDelta
; A = a 1351 POT reading, Y = the last one. Returns A = the signed movement, Y = the reading to keep:
;  the 1351 counts in bits 6-1, so the difference is taken modulo 64 and bit 0, its noise, dropped.
PotDelta
        sty tmp
        sta potNew
        sec
        sbc tmp
        and #%01111111
        cmp #%01000000
        bcs .back
        lsr
        beq .none
        ldy potNew
        rts
.back   ora #%11000000
        cmp #$ff
        beq .none
        sec
        ror
        ldy potNew
        rts
.none   lda #0
        rts

!zone MoveAxis
; A = signed movement, X = 0 for x, 1 for y; the pointer stays on the screen
MoveAxis
        ldy #0
        cmp #$80
        bcc +
        dey
+       clc
        adc posLo,x
        sta posLo,x
        tya
        adc posHi,x
        sta posHi,x
        bmi .zero
        lda posLo,x
        cmp MaxLo,x
        lda posHi,x
        sbc MaxHi,x
        bcc .out
        lda MaxLo,x
        sta posLo,x
        lda MaxHi,x
        sta posHi,x
.out    rts
.zero   lda #0
        sta posLo,x
        sta posHi,x
        rts

MaxLo   !byte <319, <199
MaxHi   !byte >319, >199

!zone ReadJoystick
ReadJoystick
        lda $dc00
        eor #$ff
        and #$1f
        sta joy
        and #$0f
        bne .dir
        sta joyDelay
        sta joyHeld
        beq .fire
.dir    lda joyDelay
        beq .move
        dec joyDelay
        jmp .fire
.move   ldx #JOY_FIRST
        lda joyHeld
        beq +
        ldx #JOY_REPEAT
+       stx joyDelay
        lda #1
        sta joyHeld
        jsr JoyMove
.fire   lda joy
        and #$10
        beq .released
        lda fireHeld
        bmi .out
        inc fireHeld
        lda fireHeld
        cmp #HOLD_FRAMES
        bcc .out
        lda #$80                ; flagged: the release does nothing
        sta fireHeld
        jmp RightAtPointer
.released
        lda fireHeld
        beq .out
        ldx #0
        stx fireHeld
        cmp #$80
        bcs .out
        lda state
        cmp #WON
        bcc +
        jmp NewGame
+       jmp LeftAtPointer
.out    rts

!zone JoyMove
; The joystick steps the pointer from cell to cell; off the board, it starts at the top left one.
JoyMove
        jsr PointerCell
        jsr OnBoard
        bcc .move
        lda #0
        sta row
        sta col
        jmp SnapPointer
.move   lda joy
        sta tmp
        lsr tmp
        bcc +
        lda row
        beq +
        dec row
+       lsr tmp
        bcc +
        lda row
        clc
        adc #1
        cmp rows
        bcs +
        sta row
+       lsr tmp
        bcc +
        lda col
        beq +
        dec col
+       lsr tmp
        bcc SnapPointer
        lda col
        clc
        adc #1
        cmp cols
        bcs SnapPointer
        sta col
SnapPointer
        lda #0
        sta posHi
        sta posHi+1
        lda col
        clc
        adc leftCol
        asl
        rol posHi
        asl
        rol posHi
        asl
        rol posHi
        ora #3
        sta posLo
        lda row
        clc
        adc #TOP
        asl
        asl
        asl
        ora #3
        sta posLo+1
        rts

!zone ReadKeys
; Scans the matrix itself (Irq has no key scan), leaving out the rows port 1 holds low: a 1351's
;  buttons there would read as keys. Acts on a key going down.
ReadKeys
        jsr ReadPort1
        lda #0
        sta keysNow
        ldx #KEYS-1
-       lda KeyCol,x
        sta $dc00
        lda $dc01
        eor #$ff
        and port1
        and KeyRow,x
        beq +
        lda KeyBit,x
        ora keysNow
        sta keysNow
+       dex
        bpl -
        lda #$7f
        sta $dc00
        lda keysLast
        eor #$ff
        and keysNow
        ldx keysNow
        stx keysLast
        ldx #0
-       lsr
        bcs .key
        inx
        cpx #KEYS
        bcc -
        rts
.key    cpx #3
        bcs +
        stx level
        jmp NewGame
+       cpx #5
        bcs +
        jmp NewGame
+       bne Quit
        jmp RightAtPointer

KeyCol  !byte $fe, $fe, $fe, $fe, $ef, $7f, $7f ; F1, F3, F5, Return, N, Space, Q
KeyRow  !byte $10, $20, $40, $02, $80, $10, $40
KeyBit  !byte 1, 2, 4, 8, 16, 32, 64

; To BASIC as from power-up: a cold start, since this game's variables sit on BASIC's zero page pointers.
;  Q and RUN/STOP are waited up first: the kernal would type a key still held, as LSTX ($C5) is stale.
Quit
        lda #0
        sta $d015
        lda #$7f                ; Q's and RUN/STOP's column
        sta $dc00
-       lda $dc01
        and #$c0
        cmp #$c0
        bne -
        sei
        jsr $ff8a               ; RESTOR: the kernal's IRQ back in $0314
        jsr $ff81               ; CINT: the VIC and the screen editor as at power-up
        cli
        jmp ($a000)

; RUN/STOP+RESTORE quits as Q does: the kernal's own NMI handler would warm-start BASIC on the game's
;  zero page. RESTORE alone returns, with $DC00 as the interrupted code left it.
Nmi
        pha
        lda $dc00
        pha
        lda #$7f
        sta $dc00
        lda $dc01
        bpl Quit                ; row 7: RUN/STOP
        pla
        sta $dc00
        pla
        rti

!zone PointerCell
; X = the text column under the pointer, Y = the text row
PointerCell
        lda posHi
        lsr
        lda posLo
        ror
        lsr
        lsr
        tax
        lda posLo+1
        lsr
        lsr
        lsr
        tay
        rts

!zone OnBoard
; X = text column, Y = text row. Carry clear, and row and col set, when that is a cell.
OnBoard
        tya
        sec
        sbc #TOP
        cmp rows
        bcs .no
        sta row
        txa
        sec
        sbc leftCol
        cmp cols
        bcs .no
        sta col
        clc
        rts
.no     sec
        rts

!zone LeftAtPointer
LeftAtPointer
        jsr PointerCell
        jsr OnBoard
        bcs .notBoard
        jmp LeftClick
.notBoard
        tya
        sec
        sbc #FACE_ROW
        cmp #2
        bcs .labels
        txa
        sec
        sbc #FACE_COL
        cmp #2
        bcs .labels
        jmp NewGame
.labels cpy #LABEL_ROW
        bne .out
        ldy #0
        cpx #12
        bcc .set
        iny
        cpx #29
        bcc .set
        iny
.set    sty level
        jmp NewGame
.out    rts

RightAtPointer
        jsr PointerCell
        jsr OnBoard
        bcs +
        jmp ToggleFlag
+       rts

MiddleAtPointer
        jsr PointerCell
        jsr OnBoard
        bcs +
        jmp Chord
+       rts

; ------------------------------------------------------------------ game
!zone NewGame
NewGame
        ldx level
        lda LvRows,x
        sta rows
        lda LvCols,x
        sta cols
        lda LvMines,x
        sta mines
        lda LvLeft,x
        sta leftCol
        lda LvColMask,x
        sta colMask
        lda #15
        sta rowMask
        lda LvSafeLo,x
        sta safeLeft
        lda LvSafeHi,x
        sta safeLeft+1
        lda #READY
        sta state
        lda #0
        sta flags
        sta flags+1
        sta seconds
        sta seconds+1
        sta jifSub
        sta lost
        sta spLo
        sta spHi
        tax
-       sta BOARD,x
        sta BOARD+$100,x
        inx
        bne -
        jmp DrawAll

LvRows    !byte 9, 16, 16
LvCols    !byte 9, 16, 30
LvMines   !byte 10, 40, 99
LvLeft    !byte (40-9)/2, (40-16)/2, (40-30)/2
LvColMask !byte 15, 15, 31
LvSafeLo  !byte <(9*9-10), <(16*16-40), <(16*30-99)
LvSafeHi  !byte >(9*9-10), >(16*16-40), >(16*30-99)

!zone LeftClick
; row, col: open a covered cell, or open around a number whose flags are all placed
LeftClick
        lda state
        cmp #WON
        bcs .out
        jsr CellPtr
        lda (ptr),y
        and #FLAG
        bne .out
        lda (ptr),y
        and #OPEN
        beq .covered
        jmp Chord
.covered
        lda state
        bne +
        jsr PlaceMines
        lda #PLAYING
        sta state
+       jsr Uncover
        jsr Flood
        jmp AfterMove
.out    rts

!zone Chord
Chord
        lda state
        cmp #PLAYING
        bne .out
        jsr CellPtr
        lda (ptr),y
        and #OPEN
        beq .out
        lda (ptr),y
        and #COUNT
        beq .out
        sta need
        lda #0
        sta nbrCount
        lda #<CountFlag
        sta nbrFn
        lda #>CountFlag
        sta nbrFn+1
        jsr ForNeighbors
        lda nbrCount
        cmp need
        bne .out
        lda #<Uncover
        sta nbrFn
        lda #>Uncover
        sta nbrFn+1
        jsr ForNeighbors
        jsr Flood
        jmp AfterMove
.out    rts

!zone ToggleFlag
ToggleFlag
        lda state
        cmp #WON
        bcs .out
        jsr CellPtr
        lda (ptr),y
        and #OPEN
        bne .out
        lda (ptr),y
        eor #FLAG
        sta (ptr),y
        and #FLAG
        beq .unflag
        inc flags
        bne .draw
        inc flags+1
        bne .draw
.unflag lda flags
        bne +
        dec flags+1
+       dec flags
.draw   jsr DrawBoard
        jmp DrawStatus
.out    rts

!zone AfterMove
AfterMove
        lda lost
        beq +
        lda #LOST
        sta state
        bne .draw
+       lda safeLeft
        ora safeLeft+1
        bne .draw
        lda #WON
        sta state
        ldx #0
-       lda BOARD,x
        bpl +
        ora #FLAG
        sta BOARD,x
+       lda BOARD+$100,x
        bpl +
        ora #FLAG
        sta BOARD+$100,x
+       inx
        bne -
        lda mines
        sta flags
        lda #0
        sta flags+1
.draw   jsr DrawBoard
        jsr DrawStatus
        lda state
        cmp #WON
        bcc +
        jmp DrawBanner
+       rts

; Across the middle of the screen, over the board, until the next new game redraws it
DrawBanner
        ldx #BannerEnd-BannerText-1
-       lda BannerText,x
        ora #BG3
        sta SCREEN+BANNER_ROW*40+BANNER_COL,x
        lda #BLACK
        sta COLORRAM+BANNER_ROW*40+BANNER_COL,x
        dex
        bpl -
        rts

!zone Uncover
; row, col: open a covered, unflagged cell. A mine loses; a cell with no mines around goes on the stack.
Uncover
        jsr CellPtr
        lda (ptr),y
        and #OPEN|FLAG
        bne .out
        lda (ptr),y
        ora #OPEN
        sta (ptr),y
        bpl .safe
        ora #BOOM
        sta (ptr),y
        lda #1
        sta lost
        rts
.safe   lda safeLeft
        bne +
        dec safeLeft+1
+       dec safeLeft
        lda (ptr),y
        and #COUNT
        bne .out
        jmp Push
.out    rts

!zone Push
Push
        lda spLo
        sta sptr
        lda spHi
        clc
        adc #>STACKROW
        sta sptr+1
        ldy #0
        lda row
        sta (sptr),y
        lda sptr+1
        clc
        adc #>(STACKCOL-STACKROW)
        sta sptr+1
        lda col
        sta (sptr),y
        inc spLo
        bne +
        inc spHi
+       rts

!zone Flood
; Opens the neighbors of every cell on the stack; Uncover pushes the ones that are empty too.
Flood
.next   lda spLo
        ora spHi
        beq .out
        lda spLo
        bne +
        dec spHi
+       dec spLo
        lda spLo
        sta sptr
        lda spHi
        clc
        adc #>STACKROW
        sta sptr+1
        ldy #0
        lda (sptr),y
        sta row
        lda sptr+1
        clc
        adc #>(STACKCOL-STACKROW)
        sta sptr+1
        lda (sptr),y
        sta col
        lda #<Uncover
        sta nbrFn
        lda #>Uncover
        sta nbrFn+1
        jsr ForNeighbors
        jmp .next
.out    rts

!zone PlaceMines
; Mines go anywhere but the 3x3 around row, col, so the first click always opens an area.
PlaceMines
        lda row
        sta safeRow
        lda col
        sta safeCol
        lda $d012
        eor seed
        sta seed
        lda $d41b
        eor seed+1
        sta seed+1
        ora seed
        bne +
        inc seed                ; xorshift stays at 0 from 0
+       lda mines
        sta toPlace
.pick   jsr Rand
        and rowMask
        cmp rows
        bcs .pick
        sta row
-       jsr Rand
        and colMask
        cmp cols
        bcs -
        sta col
        lda row
        sec
        sbc safeRow
        clc
        adc #1
        cmp #3
        bcs .far
        lda col
        sec
        sbc safeCol
        clc
        adc #1
        cmp #3
        bcc .pick
.far    jsr CellPtr
        lda (ptr),y
        bmi .pick
        ora #MINE
        sta (ptr),y
        dec toPlace
        bne .pick
        lda #<CountMine
        sta nbrFn
        lda #>CountMine
        sta nbrFn+1
        lda #0
        sta row
.row    lda #0
        sta col
.cell   lda #0
        sta nbrCount
        jsr ForNeighbors
        jsr CellPtr
        lda (ptr),y
        ora nbrCount
        sta (ptr),y
        inc col
        lda col
        cmp cols
        bcc .cell
        inc row
        lda row
        cmp rows
        bcc .row
        lda safeRow
        sta row
        lda safeCol
        sta col
        rts

; xorshift16 (7, 9, 8); A = the new high byte
Rand
        lda seed+1
        lsr
        lda seed
        ror
        eor seed+1
        sta seed+1
        ror
        eor seed
        sta seed
        eor seed+1
        sta seed+1
        rts

CountMine
        jsr CellPtr
        lda (ptr),y
        bpl +
        inc nbrCount
+       rts

CountFlag
        jsr CellPtr
        lda (ptr),y
        and #FLAG
        beq +
        inc nbrCount
+       rts

!zone ForNeighbors
; Calls (nbrFn) with row, col set to each neighbor on the board, then puts row, col back.
;  It cannot nest: nbrFn must not call it.
ForNeighbors
        lda row
        sta baseRow
        lda col
        sta baseCol
        ldx #7
.each   stx nbrIdx
        lda baseRow
        clc
        adc NbrRow,x
        cmp rows                ; -1 wraps to $FF, past the board as rows is
        bcs .skip
        sta row
        lda baseCol
        clc
        adc NbrCol,x
        cmp cols
        bcs .skip
        sta col
        jsr .call
.skip   ldx nbrIdx
        dex
        bpl .each
        lda baseRow
        sta row
        lda baseCol
        sta col
        rts
.call   jmp (nbrFn)

NbrRow  !byte $ff, $ff, $ff, 0, 0, 1, 1, 1
NbrCol  !byte $ff, 0, 1, $ff, 1, $ff, 0, 1

; ptr = the start of row's cells, Y = col
CellPtr
        lda row
RowPtr
        pha
        lsr
        lsr
        lsr
        clc
        adc #>BOARD
        sta ptr+1
        pla
        and #7
        asl
        asl
        asl
        asl
        asl
        sta ptr
        ldy col
        rts

; ------------------------------------------------------------------ drawing
!zone DrawAll
DrawAll
        ldx #0
-       lda #' '
        sta SCREEN,x
        sta SCREEN+$100,x
        sta SCREEN+$200,x
        sta SCREEN+$2e8,x
        lda #WHITE
        sta COLORRAM,x
        sta COLORRAM+$100,x
        sta COLORRAM+$200,x
        sta COLORRAM+$2e8,x
        inx
        bne -
        lda #<Texts
        ldy #>Texts
        jsr PrintList
        lda #LTGREY
        sta Labels+2
        sta Labels+Label1-Label0+2
        sta Labels+Label2-Label0+2
        ldx level
        lda LabelOffset,x
        tax
        lda #YELLOW
        sta Labels+2,x
        lda #<Labels
        ldy #>Labels
        jsr PrintList
        jsr DrawBoard
        jmp DrawStatus

LabelOffset !byte 0, Label1-Label0, Label2-Label0

!zone PrintList
; A/Y = records of row, column, color, screen codes, 0; $FF ends the list
PrintList
        sta ptr
        sty ptr+1
.record ldy #0
        lda (ptr),y
        cmp #$ff
        beq .out
        tax
        iny
        lda (ptr),y
        clc
        adc ScrRowLo,x
        sta scr
        sta cptr
        lda ScrRowHi,x
        adc #0
        sta scr+1
        clc
        adc #>(COLORRAM-SCREEN)
        sta cptr+1
        iny
        lda (ptr),y
        sta tmp
        lda ptr
        clc
        adc #3
        sta ptr
        bcc +
        inc ptr+1
+       ldy #0
-       lda (ptr),y
        beq .end
        sta (scr),y
        lda tmp
        sta (cptr),y
        iny
        bne -
.end    iny
        tya
        clc
        adc ptr
        sta ptr
        bcc .record
        inc ptr+1
        bcs .record
.out    rts

!zone DrawBoard
DrawBoard
        lda #0
        sta drawRow
.row    lda drawRow
        clc
        adc #TOP
        tax
        lda ScrRowLo,x
        clc
        adc leftCol
        sta scr
        sta cptr
        lda ScrRowHi,x
        adc #0
        sta scr+1
        clc
        adc #>(COLORRAM-SCREEN)
        sta cptr+1
        lda drawRow
        jsr RowPtr
        ldy #0
.cell   lda (ptr),y
        jsr MapCell
        sta (scr),y
        txa
        sta (cptr),y
        iny
        cpy cols
        bcc .cell
        inc drawRow
        lda drawRow
        cmp rows
        bcc .row
        rts

!zone MapCell
; A = cell; returns A = screen code, X = color
MapCell
        sta cellv
        and #OPEN
        beq .covered
        lda cellv
        bpl .number
        lda #CH_MINE|BG2
        ldx #RED
        rts
.number and #COUNT
        tax
        lda NumChar,x
        pha
        lda NumColor,x
        tax
        pla
        rts
.covered
        lda state
        cmp #LOST
        bne .playing
        lda cellv
        and #FLAG
        beq .unflagged
        lda cellv
        bmi .flag
        lda #CH_WRONG|BG2
        ldx #RED
        rts
.unflagged
        lda cellv
        bpl .tile
        lda #CH_MINE|BG2
        ldx #BLACK
        rts
.playing
        lda cellv
        and #FLAG
        beq .tile
.flag   lda #CH_FLAG|BG1
        ldx #RED
        rts
.tile   lda #CH_TILE|BG1
        ldx #MDGREY
        rts

NumChar  !byte ' '|BG2, '1'|BG2, '2'|BG2, '3'|BG2, '4'|BG2, '5'|BG2, '6'|BG2, '7'|BG2, '8'|BG2
NumColor !byte BLACK, BLUE, GREEN, RED, PURPLE, BROWN, CYAN, BLACK, WHITE

!zone DrawStatus
; mines less flags at the left, the face in the middle, the time at the right
DrawStatus
        lda mines
        sec
        sbc flags
        sta numLo
        lda #0
        sbc flags+1
        sta numHi
        ldx #' '
        lda numHi
        bpl +
        lda #0
        sec
        sbc numLo
        sta numLo
        lda #0
        sbc numHi
        sta numHi
        ldx #'-'
+       stx SCREEN+FACE_ROW*40+1
        lda #LTRED
        sta COLORRAM+FACE_ROW*40+1
        lda #<(SCREEN+FACE_ROW*40+2)
        sta scr
        lda #>(SCREEN+FACE_ROW*40+2)
        sta scr+1
        jsr PrintNum
        jsr DrawTimer
        ldx #0
        lda state
        cmp #WON
        bcc +
        sbc #1
        tax
+       lda FaceTL,x
        sta SCREEN+FACE_ROW*40+FACE_COL
        lda FaceTR,x
        sta SCREEN+FACE_ROW*40+FACE_COL+1
        lda FaceBL,x
        sta SCREEN+(FACE_ROW+1)*40+FACE_COL
        lda FaceBR,x
        sta SCREEN+(FACE_ROW+1)*40+FACE_COL+1
        lda #BLACK
        sta COLORRAM+FACE_ROW*40+FACE_COL
        sta COLORRAM+FACE_ROW*40+FACE_COL+1
        sta COLORRAM+(FACE_ROW+1)*40+FACE_COL
        sta COLORRAM+(FACE_ROW+1)*40+FACE_COL+1
        rts

FaceTL  !byte CH_HAPPY|BG3, CH_COOL|BG3, CH_DEAD|BG3
FaceTR  !byte (CH_HAPPY+1)|BG3, (CH_COOL+1)|BG3, (CH_DEAD+1)|BG3
FaceBL  !byte CH_SMILE|BG3, CH_SMILE|BG3, CH_FROWN|BG3
FaceBR  !byte (CH_SMILE+1)|BG3, (CH_SMILE+1)|BG3, (CH_FROWN+1)|BG3

DrawTimer
        lda seconds
        sta numLo
        lda seconds+1
        sta numHi
        lda #<(SCREEN+FACE_ROW*40+35)
        sta scr
        lda #>(SCREEN+FACE_ROW*40+35)
        sta scr+1
        ; falls into PrintNum

!zone PrintNum
; numHi/numLo (under 1000) as three digits at scr
PrintNum
        lda scr
        sta cptr
        lda scr+1
        clc
        adc #>(COLORRAM-SCREEN)
        sta cptr+1
        ldy #0
        ldx #0
-       lda numLo
        sec
        sbc #100
        sta tmp
        lda numHi
        sbc #0
        bcc +
        sta numHi
        lda tmp
        sta numLo
        inx
        bne -
+       jsr .digit
        ldx #0
-       lda numLo
        sec
        sbc #10
        bcc +
        sta numLo
        inx
        bne -
+       jsr .digit
        ldx numLo
.digit  txa
        ora #'0'
        sta (scr),y
        lda #LTRED
        sta (cptr),y
        iny
        rts

!zone UpdateTimer
; The kernal's jiffy clock counts 60 a second on PAL and NTSC alike.
UpdateTimer
        lda $a2
        tax
        sec
        sbc lastJiffy
        stx lastJiffy
        ldx state
        cpx #PLAYING
        bne .out
        clc
        adc jifSub
        sta jifSub
        cmp #60
        bcc .out
        sbc #60
        sta jifSub
        lda seconds+1
        cmp #>999
        bne +
        lda seconds
        cmp #<999
        beq .out
+       inc seconds
        bne +
        inc seconds+1
+       jmp DrawTimer
.out    rts

PlacePointer
        lda posLo
        clc
        adc #24
        sta $d000
        sta $d002
        lda posHi
        adc #0
        beq +
        lda #3
+       sta $d010
        lda posLo+1
        clc
        adc #50
        sta $d001
        sta $d003
        rts

; Waits for the raster to reach the lower border.
WaitFrame
-       lda $d011
        bmi -
        lda $d012
        cmp #250
        bcs -
-       lda $d011
        bmi +
        lda $d012
        cmp #250
        bcc -
+       rts

; ------------------------------------------------------------------ setup
!zone BuildCharset
BuildCharset
        sei
        lda $01
        pha
        lda #$33                ; character ROM in
        sta $01
        ldx #0
-       lda $d000,x
        sta CHARSET,x
        lda $d100,x
        sta CHARSET+$100,x
        inx
        bne -
        pla
        sta $01
        cli
        ldx #0
        stx dataIdx
.glyph  lda GlyphCodes,x
        bmi .out
        stx glyphIdx
        asl
        asl
        asl
        sta ptr
        lda GlyphCodes,x
        lsr
        lsr
        lsr
        lsr
        lsr
        clc
        adc #>CHARSET
        sta ptr+1
        ldy #0
-       ldx dataIdx
        lda GlyphData,x
        sta (ptr),y
        inc dataIdx
        iny
        cpy #8
        bne -
        ldx glyphIdx
        inx
        bne .glyph
.out    rts

SetupSprites
        ldx #63
-       lda PointerOutline,x
        sta SPRITES,x
        lda PointerFill,x
        sta SPRITES+64,x
        dex
        bpl -
        lda #SPRITES/64
        sta SCREEN+$3f8
        lda #SPRITES/64+1
        sta SCREEN+$3f9
        lda #BLACK
        sta $d027
        lda #WHITE
        sta $d028
        lda #0
        sta $d01d
        sta $d017
        sta $d01c
        sta $d01b
        lda #3
        sta $d015
        rts

; ------------------------------------------------------------------ data
ScrRowLo
!for r, 0, 24 {
        !byte <(SCREEN + r * 40)
}
ScrRowHi
!for r, 0, 24 {
        !byte >(SCREEN + r * 40)
}

Texts
        !byte 0, 1, YELLOW
        !scr "minesweeper", 0
        !byte 3, 1, LTGREY
        !scr "mines", 0
        !byte 3, 35, LTGREY
        !scr "time", 0
        !byte 22, 1, LTGREY
        !scr "left: open  right: flag  middle: chord", 0
        !byte 23, 1, LTGREY
        !scr "joystick 2: fire opens, hold fire flags", 0
        !byte 0, 22, LTGREEN
        !scr "usb or 1351 mouse", 0
        !byte $ff

BannerText
        !scr " n for new game, q to quit "
BannerEnd
BANNER_ROW = 12
BANNER_COL = (40 - (BannerEnd - BannerText)) / 2
Labels
Label0  !byte LABEL_ROW, 0, LTGREY
        !scr "f1 beginner", 0
Label1  !byte LABEL_ROW, 13, LTGREY
        !scr "f3 intermediate", 0
Label2  !byte LABEL_ROW, 30, LTGREY
        !scr "f5 expert", 0
        !byte $ff

GlyphCodes
        !byte CH_TILE, CH_FLAG, CH_MINE, CH_WRONG
        !byte CH_HAPPY, CH_HAPPY+1, CH_COOL, CH_COOL+1, CH_DEAD, CH_DEAD+1
        !byte CH_SMILE, CH_SMILE+1, CH_FROWN, CH_FROWN+1
        !byte $ff
GlyphData
        !byte %00000001, %00000001, %00000001, %00000001, %00000001, %00000001, %00000001, %11111111 ; tile
        !byte %00000000, %00111000, %01111000, %00111000, %00001000, %00001000, %00111100, %00000000 ; flag
        !byte %00010000, %01010100, %00111000, %11111110, %00111000, %01010100, %00010000, %00000000 ; mine
        !byte %10000010, %01000100, %00101000, %00010000, %00101000, %01000100, %10000010, %00000000 ; wrong flag
        !byte %00000111, %00011000, %00100000, %01000000, %01001100, %10001100, %10000000, %10000000 ; happy, left
        !byte %11100000, %00011000, %00000100, %00000010, %00110010, %00110001, %00000001, %00000001 ; happy, right
        !byte %00000111, %00011000, %00100000, %01111111, %01011110, %10001100, %10000000, %10000000 ; cool, left
        !byte %11100000, %00011000, %00000100, %11111110, %01111010, %00110001, %00000001, %00000001 ; cool, right
        !byte %00000111, %00011000, %00100000, %01001010, %01000100, %10001010, %10000000, %10000000 ; dead, left
        !byte %11100000, %00011000, %00000100, %01010010, %00100010, %01010001, %00000001, %00000001 ; dead, right
        !byte %10000000, %10010000, %10001000, %01000111, %01000000, %00100000, %00011000, %00000111 ; smile, left
        !byte %00000001, %00001001, %00010001, %11100010, %00000010, %00000100, %00011000, %11100000 ; smile, right
        !byte %10000000, %10000000, %10000111, %01001000, %01010000, %00100000, %00011000, %00000111 ; frown, left
        !byte %00000001, %00000001, %11100001, %00010010, %00001010, %00000100, %00011000, %11100000 ; frown, right

PointerOutline
        !byte $80,0,0, $c0,0,0, $a0,0,0, $90,0,0, $88,0,0, $84,0,0, $82,0,0, $81,0,0
        !byte $87,$80,0, $a4,0,0, $d2,0,0, $89,0,0, $09,0,0, $06,0,0
        !fill 64 - 14*3, 0
PointerFill
        !byte 0,0,0, 0,0,0, $40,0,0, $60,0,0, $70,0,0, $78,0,0, $7c,0,0, $7e,0,0
        !byte $78,0,0, $58,0,0, $0c,0,0, $06,0,0, $06,0,0, 0,0,0
        !fill 64 - 14*3, 0

!if * > BOARD { !error "the program runs into the board at $2000" }
