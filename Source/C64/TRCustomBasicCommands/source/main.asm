
// https://github.com/barryw/CustomBasicCommands

#import "include/all.asm"

.label DATA     = $83
.label REM      = $8f
.label PRINT    = $99
.label QUOTE    = $22
.label BUFFER   = $200

// Set these to the start/end tokens for commands and functions.
// You can find the table of tokens below at NewTab
.label CMDSTART = $cc
.label CMDEND   = $dd
.label FUNSTART = CMDEND + $01
.label FUNEND   = $e0

.label MainMemLoc  = $c000

.label IO1Port  = $de00


//enum TR_BASregsMatching  //synch with IOH_TR_BASIC.c
//{
   // registers:
.label TR_BASDataReg         = $b4   // (Write only)  
.label TR_BASContReg         = $ba   // (Write only) Control Reg 

   // Control Reg Commands


//}; //end enum synch

*=$0801
// BASIC SYS header

.byte $0b,$08,$01,$00,$9e      // Line 1 SYS
.text "2070"                   // Address for sys start in text
.byte $00,$11,$08,$02,$00,$a2  //  Line 2 NEW
.byte $00,$00,$00              // End of Program


*=$0816  //2070

   // transfer code to upper memory (MainMemLoc)
   lda #>StartOfCode
   ldy #<StartOfCode   
   sta r0H   //r0 points to code to copy
   sty r0L 
   ldx #>EndOfCode

   lda #>MainMemLoc
   ldy #<MainMemLoc   
   sta r1H   //r1 points to where to copy to
   sty r1L 

   // Copy from (r0 Lo/Hi) to (r1 Lo/Hi), x reg is last page to copy
   inx //last page+1, will copy ((EndOfCode-StartOfCode) | 0xFF) bytes
   ldy #0      //initialize
!loop:
   lda (r0),y  //copy from 
   sta (r1),y  //copy to
   iny
   bne !loop-  //copy full page
   inc r0H     //inc source page 
   inc r1H     //inc dest page 
   cpx r0H     //last page?
   bne !loop-

   // execute it to hook in new commands:
   jsr MainMemLoc   
   //print TR commands message:
   lda #<StartupText 
   ldy #>StartupText
   jsr basic.STROUT

   rts  //return to BASIC (next line is "NEW")

StartupText:
.byte 145  //cursor up to overwrite "RUNNING..."
.text "TR CUSTOM COMMANDS LOADED"
.byte 0 


StartOfCode:
.pseudopc MainMemLoc {  //compile for $c000

/*
    Set up our vectors
*/
Init:
    ldx #<ConvertToTokens
    ldy #>ConvertToTokens
    stx vectors.ICRNCH
    sty vectors.ICRNCH + $01

    ldx #<ConvertFromTokens
    ldy #>ConvertFromTokens
    stx vectors.IQPLOP
    sty vectors.IQPLOP + $01

    ldx #<ExecuteCommand
    ldy #>ExecuteCommand
    stx vectors.IGONE
    sty vectors.IGONE + $01

    ldx #<ExecuteFunction
    ldy #>ExecuteFunction
    stx vectors.IEVAL
    sty vectors.IEVAL + $01

    rts

#import "memory.asm"        // Memory commands
#import "reu.asm"           // REU functions/commands
#import "sprites.asm"       // Sprite commands and functions
#import "include/timer.asm" // Timer functions

/*

    Add your commands and functions here. The last byte of each command/function name
    must have $80 added to it. Your commands should come first starting from $cc. You can
    let the execution routine know how to identify commands and functions above in CMDSTART,
    CMDEND, FUNSTART, FUNEND. These are the token numbers for each block of commands/functions.
    Our tokens start at $cc and can go up to $fe ($ff is pi). Both the tokenization and
    detokenization routines use this table, so adding them here will ensure that BASIC
    will recognize them as you enter them and will detokenize them when LISTed.

*/
NewTab:
    // Commands start here
    .text "BORDE"       // $cc
    .byte 'R' + $80
    .text "BACKGROUN"   // $cd
    .byte 'D' + $80
    .text "WOK"         // $ce
    .byte 'E' + $80
    .text "CL"          // $cf
    .byte 'S' + $80
    .text "MEMCOP"      // $d0
    .byte 'Y' + $80
    .text "MEMFIL"      // $d1
    .byte 'L' + $80
    .text "SCREE"       // $d2
    .byte 'N' + $80
    .text "BAN"         // $d3
    .byte 'K' + $80
    .text "STAS"        // $d4
    .byte 'H' + $80
    .text "FETC"        // $d5
    .byte 'H' + $80
    .text "SPRSE"       // $d6
    .byte 'T' + $80
    .text "SPRPO"       // $d7
    .byte 'S' + $80
    .text "SPRCOLO"     // $d8
    .byte 'R' + $80
    .text "MEMLOA"      // $d9
    .byte 'D' + $80
    .text "MEMSAV"      // $da
    .byte 'E' + $80
    .text "DI"          // $db
    .byte 'R' + $80
    .text "TPRIN"       // $dc
    .byte 'T' + $80
    .text "TGE"         // $dd
    .byte 'T' + $80
    // Functions start here
    .text "WEE"         // $de
    .byte 'K' + $80
    .text "SCRLO"       // $df
    .byte 'C' + $80
    .text "RE"          // $e0
    .byte 'U' + $80
    .byte 0

CmdTab:                         // A table of vectors pointing at your commands' execution addresses
    .word BorderCmd - 1         // Address - 1 of first command. Token = CMDSTART
    .word BackgroundCmd - 1
    .word WokeCmd - 1
    .word ClsCmd - 1
    .word MemCopyCmd - 1
    .word MemFillCmd - 1
    .word ScreenCmd - 1
    .word BankCmd - 1
    .word StashCmd - 1
    .word FetchCmd - 1
    .word SpriteSetCmd - 1
    .word SpritePosCmd - 1
    .word SpriteColorCmd - 1
    .word MemLoadCmd - 1
    .word MemSaveCmd - 1
    .word DirectoryCmd - 1
    .word TPrintCmd - 1
    .word TGetCmd - 1
    
FunTab:                         // A table of vectors pointing at your functions' execution addresses
    .word WeekFun               // Address of first function. Token = FUNSTART
    .word ScrLocFun
    .word ReuFun

/*

    Command Execution. This is the meat of it. This is where the magic happens.
    We first have to figure out whether we're executing one of our custom functions. We
    do this by looking at the current token that's returned by CHRGET. If this token
    is in the range of CMDSTART to CMDEND, then it's one of ours and we should find its
    routine in a lookup table. If it's not, then just perform the normal BASIC command
    handler.

*/
ExecuteCommand:
    jsr zp.CHRGET
    jsr TestCmd
    jmp basic.NEWSTT
TestCmd:
    cmp #CMDSTART
    bcc OldCmd
    cmp #CMDEND + 1
    bcc OkNew
OldCmd:
    jsr zp.CHRGOT
    jmp basic.EXECOLD
OkNew:
    sec
    sbc #CMDSTART
    asl
    tax
    lda CmdTab+1, x
    pha
    lda CmdTab, x
    pha
    jmp zp.CHRGET

/*

    Function Execution. This is the meat of it. This is where the magic happens.
    We first have to figure out whether we're executing one of our custom functions. We
    do this by looking at the current token that's returned by CHRGET. If this token
    is in the range of FUNSTART to FUNEND, then it's one of ours and we should find its
    routine in a lookup table. If it's not, then just perform the normal BASIC function
    handler.

*/
ExecuteFunction:
    lda #0
    sta zp.VALTYP
    jsr zp.CHRGET
    cmp #'$'                    // Is this a HEX number?
    beq ProcessHex
    cmp #'%'                    // Is this a binary number?
    beq ProcessBinary
    cmp #FUNSTART               // Is this one of ours?
    bcc OldFun
    cmp #FUNEND + 1
    bcc Ok1New
OldFun:
    jsr zp.CHRGOT               // It's a built-in Commodore function, so re-fetch the token
    jmp basic.FUNCTOLD          // and call the normal BASIC function handler.
Ok1New:
    sec
    sbc #FUNSTART               // We need to get an index to the function in the vector table
    asl                         // Start by subtracting to get a 0 based index, and then mult by 2.
    pha
    jsr zp.CHRGET
    jsr basic.PARCHK            // Grab whatever is in parens and evaluate it. This is passed to our function.
    pla
    tay
    lda FunTab, y               // Get the function's vector address...
    sta zp.JMPER + 1
    lda FunTab + 1, y
    sta zp.JMPER + 2
    jsr zp.JMPER                // ... and then call it.
    rts

/*

    Allow us to represent numbers as HEX using $ABCD syntax

*/
ProcessHex:
    jsr ClearFAC
!:
    jsr zp.CHRGET
    bcc !+
    cmp #'A'
    bcc !+++
    cmp #'F' + 1
    bcs !+++
    sec
    sbc #$07
!:
    sec
    sbc #'0'
    pha
    lda zp.EXP
    beq !+
    clc
    adc #$04
    bcs !+++
    sta zp.EXP
!:
    pla
    beq !---
    jsr basic.FINLOG
    jmp !---
!:
    jmp zp.CHRGOT
!:
    jmp basic.OVERR

/*

    Allow us to represent numbers as binary using %1010101 syntax

*/
ProcessBinary:
    jsr ClearFAC
!:
    jsr zp.CHRGET
    cmp #'2'
    bcs !---
    cmp #'0'
    bcc !---
    sbc #'0'
    pha
    lda zp.EXP
    beq !+
    inc zp.EXP
    beq !+
!:
    pla
    beq !--
    jsr basic.FINLOG
    jmp !--

ClearFAC:
    lda #$00
    ldx #$0a
!:
    sta zp.FLOAT, x
    dex
    bpl !-
    rts

/*

    Clear the screen using PETSCII $93. Easy peasey.

    Example: CLS

*/
ClsCmd:
    lda #$93
    jmp kernal.VEC_CHROUT

/*

    Set the border color

    Example: BORDER 0. Sets the border color to black

*/
BorderCmd:
    jsr GetColor
    sta vic.EXTCOL
    rts

/*

    Set the background color

    Example: BACKGROUND 0. Sets the background color to black

*/
BackgroundCmd:
    jsr GetColor
    sta vic.BGCOL0
    rts

/*

    Execute a POKE but allow passing in a word for the address and the value.

    Example: WOKE 250, 65535. Puts 255 in location 250 and 251.

*/
WokeCmd:
    jsr Get16Bit        // Get the address to WOKE to
    lda $14
    sta $57
    lda $15
    sta $58

    jsr basic.CHKCOM    // Make sure we have a comma

    jsr Get16Bit        // Get the 16-bit word to WOKE

    ldy #$00            // WOKE IT!
    lda $14
    sta ($57), y
    lda $15
    iny
    sta ($57), y

    rts

/*

    Select the VIC bank (0-3)

    Example: BANK 0 would specify 0-16384 as the range of locations that the VIC sees

*/
BankCmd:
    jsr Get8Bit         // Get the bank #
    tya
    pha
    and #$fc            // Strip the bottom 2 bits.
    cmp #$00            // Is the value > 0? That means we specified a number > 3
    bne !+              // Illegal quantity
    pla
    sta r0L
    lda #$03            // The bit patterns are backwards. 11 means bank 0 and 00 means bank 3
    sec
    sbc r0L
    sta r0H
    lda $dd00           // Read the existing value from $dd00
    and #$fc            // Set just the bank selection bits (0 & 1)
    ora r0H
    sta $dd00
    rts
!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Get the current VIC bank 0-3

*/
CurrentBank:
    lda $dd00
    and #$03
    sta r0L
    lda #$03
    sec
    sbc r0L

    rts

/*

    Set the screen location within the current VIC bank

    Example: SCREEN 1 would set the screen location to offset $0400 of the current VIC bank.
    If in bank 0, this would be $0400 since bank 0 starts at $0000. This is also the default
    at startup.

*/
ScreenCmd:
    jsr CurrentBank     // Get the current VIC bank
    sta r0L
    jsr Get8Bit         // Get the screen number (0-15)
    tya
    pha
    and #$f0            // Strip the bottom 4 bits
    cmp #$00            // Is the value > 0? Means we've specified a number > 15
    bne !+              // Illegal quantity
    pla
    tay
    sty r0H             // Tuck away the screen number (0-15)
    lda $d018           // Set the screen number in $d018
    and #$0f
    ora ScreenLoc, y
    sta $d018
    lda r0H             // Now let BASIC know where that screen is
    asl                 // Multiply by 4
    asl
    sta r0H
    lda r0L
    asl
    asl
    asl
    asl
    asl
    asl
    ora r0H
    sta $288
    rts

!:
    jmp basic.ILLEGAL_QUANTITY

SpriteDiskCommandCommon:
    pha
    jsr basic.FRMEVL    // Evaluate the expression after the token
    jsr basic.FRESTR    // Discard Temp string, get point to string and length
    sta r0L             // Filename length
    lda $22
    sta r1L             // Filename
    lda $23
    sta r1H
    jsr basic.CHKCOM
    jsr Get8Bit
    sty r0H             // Device number
    jsr basic.CHKCOM
    jsr Get16Bit
    lda $14
    sta r2L             // Load/Save address
    lda $15
    sta r2H
    pla
    beq !+              // If this is a save, we need to fetch the end address
    jsr basic.CHKCOM
    jsr Get16Bit
    lda $14
    sta r3L             // Number of bytes to save
    lda $15
    sta r3H
!:
    lda r0L
    ldx r1L
    ldy r1H
    jsr kernal.VEC_SETNAM

    lda #$02
    ldx r0H
    ldy #$02
    jsr kernal.VEC_SETLFS
    jsr kernal.VEC_OPEN
    bcs !+
    ldx #$02
    ldy #$00
    rts
!:
    lda #<DiskError
    ldy #>DiskError
    jmp CustomError

DiskClose:
    lda #$02
    jsr kernal.VEC_CLOSE
    jsr kernal.VEC_CLRCHN
    rts

/*

    Load bytes from disk directly into memory.

    Example: MEMLOAD "SPRITES.SPR", 8, $2000 would load the file SPRITES.SPR from device 8 into memory at $2000

*/
MemLoadCmd:
    lda #$00
    jsr SpriteDiskCommandCommon
    jsr kernal.VEC_CHKIN
!: // LOOP
    jsr kernal.VEC_READST
    bne !++ // EOF
    jsr kernal.VEC_CHRIN
    bcs !--
    sta (r2), y
    inc r2L
    bne !+ // SKIP2
    inc r2H
!: // SKIP2
    jmp !-- // LOOP
!: // EOF
    ora #$40
    cmp #$40
    bne !----
!: // CLOSE
    jmp DiskClose

/*

    Save memory to disk.

    Example: MEMSAVE "@:SPRITES.SPR,P,W", 8, $2000, $2040 would save memory from $2000 - $2040 to the file SPRITES.SPR on device 8.

*/
MemSaveCmd:
    lda #$80
    jsr SpriteDiskCommandCommon
    jsr kernal.VEC_CHKOUT
!: // LOOP
    jsr kernal.VEC_READST
    bne !------ // WERROR
    lda (r2), y
    jsr kernal.VEC_CHROUT
    bcs !------
    inc r2L
    bne !+ // SKIP
    inc r2H
!: // SKIP
    lda r2L
    cmp r3L
    lda r2H
    sbc r3H
    bcc !-- // LOOP
!: // CLOSE
    jmp DiskClose

/*

    Send String out TeensyROM Serial USB Device Port

    Example: TPRINT "HELLO WORLD!" will write "HELLO WORLD!" out the USB Serial port

*/
TPrintCmd:
    jsr basic.FRMEVL    // Evaluate the expression after the token
    jsr basic.FRESTR    // Discard Temp string, get point to string and length
    // acc=msg len
    // $22=Message L pointer
    // $23=Message H pointer
    
    sta r0
    ldy #0
!:  cpy r0
    beq !+ //last char, exit
    lda ($22),y
    //jsr kernal.VEC_CHROUT
    sta TR_BASDataReg+IO1Port //write to TR out data reg
    iny
    bne !- //255 char limit
!:  rts
    


/*

    Get a char from TeensyROM Serial USB Device Port, return "" if none

    Example: TGET A$ will read a char from the USB Serial port in to A$

*/
TGetCmd:

                 //                *** perform GET
    jsr $b3a6    //.,ab7b 20 a6 b3     //check not Direct, back here if ok
    //cmp #$23   //.,ab7e c9 23        //compare with "#"
    //bne $ab92  //.,ab80 d0 10        //branch if not GET#
    //jsr $0073  //.,ab82 20 73 00     //increment and scan memory
    //jsr $b79e  //.,ab85 20 9e b7     //get byte parameter
    //lda #$2c   //.,ab88 a9 2c        //set ","
    //jsr $aeff  //.,ab8a 20 ff ae     //scan for CHR$(A), else do syntax error then warm start
    //stx $13    //.,ab8d 86 13        //set current I/O channel
    //jsr $e11e  //.,ab8f 20 1e e1     //open channel for input with error check
    ldx #$01     //.,ab92 a2 01        //set pointer low byte
    ldy #$02     //.,ab94 a0 02        //set pointer high byte
    lda #$00     //.,ab96 a9 00        //clear A
    sta $0201    //.,ab98 8d 01 02     //ensure null terminator
    lda #$40     //.,ab9b a9 40        //input mode = GET
    //jsr PerfGET    //.,ab9d 20 0f ac     //perform the GET part of READ
    //ldx $13    //.,aba0 a6 13        //get current I/O channel
    //bne $abb7  //.,aba2 d0 13        //if not default channel go do channel close and return
    //rts          //.,aba4 60         

//    lda TR_BASDataReg+IO1Port //read from TR in data reg

                 //               *** perform GET
//PerfGET:
    sta $11      //.,ac0f 85 11      //set input mode flag, $00 = INPUT, $40 = GET, $98 = READ
    stx $43      //.,ac11 86 43      //save READ pointer low byte
    sty $44      //.,ac13 84 44      //save READ pointer high byte
                 //                  //READ, GET or INPUT next variable from list
NextVar:                 
    jsr $b08b    //.,ac15 20 8b b0   //get variable address
    sta $49      //.,ac18 85 49      //save address low byte
    sty $4a      //.,ac1a 84 4a      //save address high byte
    lda $7a      //.,ac1c a5 7a      //get BASIC execute pointer low byte
    ldy $7b      //.,ac1e a4 7b      //get BASIC execute pointer high byte
    sta $4b      //.,ac20 85 4b      //save BASIC execute pointer low byte
    sty $4c      //.,ac22 84 4c      //save BASIC execute pointer high byte
    ldx $43      //.,ac24 a6 43      //get READ pointer low byte
    ldy $44      //.,ac26 a4 44      //get READ pointer high byte
    stx $7a      //.,ac28 86 7a      //save as BASIC execute pointer low byte
    sty $7b      //.,ac2a 84 7b      //save as BASIC execute pointer high byte
    jsr $0079    //.,ac2c 20 79 00   //scan memory
    bne NotNULL  //.,ac2f d0 20      //branch if not null
                 //                  //pointer was to null entry
    bit $11      //.,ac31 24 11      //test input mode flag, $00 = INPUT, $40 = GET, $98 = READ
    bvc NotGET   //.,ac33 50 0c      //branch if not GET
                 //                  //else was GET
    jsr $e124    //.,ac35 20 24 e1   //get character from input device with error check
    sta $0200    //.,ac38 8d 00 02   //save to buffer
    ldx #$ff     //.,ac3b a2 ff      //set pointer low byte
    ldy #$01     //.,ac3d a0 01      //set pointer high byte
    bne SingleChar //.,ac3f d0 0c      //go interpret single character
NotGET:
    bmi ReadCmd  //.,ac41 30 75      //branch if READ
                 //                  //else was INPUT
    lda $13      //.,ac43 a5 13      //get current I/O channel
    bne SkipQM   //.,ac45 d0 03      //skip "?" prompt if not default channel
    jsr $ab45    //.,ac47 20 45 ab   //print "?"
SkipQM:
    jsr $abf9    //.,ac4a 20 f9 ab   //print "? " and get BASIC input
SingleChar:
    stx $7a      //.,ac4d 86 7a      //save BASIC execute pointer low byte
    sty $7b      //.,ac4f 84 7b      //save BASIC execute pointer high byte
NotNULL:
    jsr $0073    //.,ac51 20 73 00   //increment and scan memory, execute pointer now points to
                 //                  //start of next data or null terminator
    bit $0d      //.,ac54 24 0d      //test data type flag, $FF = string, $00 = numeric
    bpl IsNumber //.,ac56 10 31      //branch if numeric
                 //                  //type is string
    bit $11      //.,ac58 24 11      //test INPUT mode flag, $00 = INPUT, $40 = GET, $98 = READ
    bvc InputOrRead //.,ac5a 50 09      //branch if not GET
                 //                  //else do string GET
    inx          //.,ac5c e8         //clear X ??
    stx $7a      //.,ac5d 86 7a      //save BASIC execute pointer low byte
    lda #$00     //.,ac5f a9 00      //clear A
    sta $07      //.,ac61 85 07      //clear search character
    beq Always   //.,ac63 f0 0c      //branch always
                 //                  //is string INPUT or string READ
InputOrRead:
    sta $07      //.,ac65 85 07      //save search character
    cmp #$22     //.,ac67 c9 22      //compare with "
    beq isQuote  //.,ac69 f0 07      //branch if quote
                 //                  //string is not in quotes so ":", "," or $00 are the
                 //                  //termination characters
    lda #$3a     //.,ac6b a9 3a      //set ":"
    sta $07      //.,ac6d 85 07      //set search character
    lda #$2c     //.,ac6f a9 2c      //set ","
Always:
    clc          //.,ac71 18         //clear carry for add
isQuote:
    sta $08      //.,ac72 85 08      //set scan quotes flag
    lda $7a      //.,ac74 a5 7a      //get BASIC execute pointer low byte
    ldy $7b      //.,ac76 a4 7b      //get BASIC execute pointer high byte
    adc #$00     //.,ac78 69 00      //add to pointer low byte. this add increments the pointer
                 //                  //if the mode is INPUT or READ and the data is a "..."
                 //                  //string
    bcc NoRoll   //.,ac7a 90 01      //branch if no rollover
    iny          //.,ac7c c8         //else increment pointer high byte
NoRoll:
    jsr $b48d    //.,ac7d 20 8d b4   //print string to utility pointer
    jsr $b7e2    //.,ac80 20 e2 b7   //restore BASIC execute pointer from temp
    jsr $a9da    //.,ac83 20 da a9   //perform string LET
    jmp ContProc //.,ac86 4c 91 ac   //continue processing command
                 //                  //GET, INPUT or READ is numeric
IsNumber:
    jsr $bcf3    //.,ac89 20 f3 bc   //get FAC1 from string
    lda $0e      //.,ac8c a5 0e      //get data type flag, $80 = integer, $00 = float
    jsr $a9c2    //.,ac8e 20 c2 a9   //assign value to numeric variable
ContProc:
    jsr $0079    //.,ac91 20 79 00   //scan memory
    beq ColOrEOL //.,ac94 f0 07      //branch if ":" or [EOL]
    cmp #$2c     //.,ac96 c9 2c      //comparte with ","
    beq ColOrEOL //.,ac98 f0 03      //branch if ","
    jmp $ab4d    //.,ac9a 4c 4d ab   //else go do bad input routine
                 //                  //string terminated with ":", "," or $00
ColOrEOL:
    lda $7a      //.,ac9d a5 7a      //get BASIC execute pointer low byte
    ldy $7b      //.,ac9f a4 7b      //get BASIC execute pointer high byte
    sta $43      //.,aca1 85 43      //save READ pointer low byte
    sty $44      //.,aca3 84 44      //save READ pointer high byte
    lda $4b      //.,aca5 a5 4b      //get saved BASIC execute pointer low byte
    ldy $4c      //.,aca7 a4 4c      //get saved BASIC execute pointer high byte
    sta $7a      //.,aca9 85 7a      //restore BASIC execute pointer low byte
    sty $7b      //.,acab 84 7b      //restore BASIC execute pointer high byte
    jsr $0079    //.,acad 20 79 00   //scan memory
    beq ColOrEOL2 //.,acb0 f0 2d      //branch if ":" or [EOL]
    jsr $aefd    //.,acb2 20 fd ae   //scan for ",", else do syntax error then warm start
    jmp NextVar  //.,acb5 4c 15 ac   //go READ or INPUT next variable from list
                 //                  //was READ
ReadCmd:
    jsr $a906    //.,acb8 20 06 a9   //scan for next BASIC statement ([:] or [EOL])
    iny          //.,acbb c8         //increment index to next byte
    tax          //.,acbc aa         //copy byte to X
    bne isColon  //.,acbd d0 12      //branch if ":"
    ldx #$0d     //.,acbf a2 0d      //else set error $0D, out of data error
    iny          //.,acc1 c8         //increment index to next line pointer high byte
    lda ($7a),y  //.,acc2 b1 7a      //get next line pointer high byte
    //beq $ad32    //.,acc4 f0 6c      //branch if program end, eventually does error X
    iny          //.,acc6 c8         //increment index
    lda ($7a),y  //.,acc7 b1 7a      //get next line # low byte
    sta $3f      //.,acc9 85 3f      //save current DATA line low byte
    iny          //.,accb c8         //increment index
    lda ($7a),y  //.,accc b1 7a      //get next line # high byte
    iny          //.,acce c8         //increment index
    sta $40      //.,accf 85 40      //save current DATA line high byte
isColon:
    jsr $a8fb    //.,acd1 20 fb a8   //add Y to the BASIC execute pointer
    jsr $0079    //.,acd4 20 79 00   //scan memory
    tax          //.,acd7 aa         //copy the byte
    cpx #$83     //.,acd8 e0 83      //compare it with token for DATA
    bne ReadCmd  //.,acda d0 dc      //loop if not DATA
    jmp NotNULL  //.,acdc 4c 51 ac   //continue evaluating READ
ColOrEOL2:
    lda $43      //.,acdf a5 43      //get READ pointer low byte
    ldy $44      //.,ace1 a4 44      //get READ pointer high byte
    ldx $11      //.,ace3 a6 11      //get INPUT mode flag, $00 = INPUT, $40 = GET, $98 = READ
    bpl InputOrGet //.,ace5 10 03      //branch if INPUT or GET
    jmp $a827    //.,ace7 4c 27 a8   //else set data pointer and exit
InputOrGet:    
    ldy #$00     //.,acea a0 00      //clear index
    lda ($43),y  //.,acec b1 43      //get READ byte
    beq EndOfGet //.,acee f0 0b      //exit if [EOL]
    lda $13      //.,acf0 a5 13      //get current I/O channel
    bne EndOfGet //.,acf2 d0 07      //exit if not default channel
    lda #$fc     //.,acf4 a9 fc      //set "?EXTRA IGNORED" pointer low byte
    ldy #$ac     //.,acf6 a0 ac      //set "?EXTRA IGNORED" pointer high byte
    jmp $ab1e    //.,acf8 4c 1e ab   //print null terminated string
EndOfGet:
    rts          //.,acfb 60      

/*

    Perform a non-destructive directory listing.
    Pilfered from https://csdb.dk/forums/?roomid=11&topicid=17487

    Example: DIR 8 would list the directory of device 8

*/
DirectoryCmd:
    jsr Get8Bit
    tya
    pha
    lda #$01
    tax
    ldy #$e8
    jsr kernal.VEC_SETNAM
    pla
    sta $ba
    lda #$60
    sta $b9
    jsr $f3d5
    jsr $f219
    ldy #$04
!:
    jsr $ee13
    dey
    bne !-
    lda $c6
    ora $90
    bne !++
    jsr $ee13
    tax
    jsr $ee13
    jsr $bdcd
!:
    jsr $ee13
    jsr $e716
    bne !-
    jsr $aad7
    ldy #$02
    bne !--
!:
    jsr $f642
    jsr $f6f3
!:
    rts

/*

    Grab the sprite number as the first argument for the sprite commands. Stores in r0L

*/
SpriteCommon:
    jsr Get8Bit         // Get the sprite number (0-7)
    cpy #$08
    bcs !+
    sty r0L             // Sprite #
    rts
!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Set a sprite's color

*/
SpriteColorCmd:
    jsr SpriteCommon
    jsr basic.CHKCOM
    jsr GetColor
    ldy r0L
    sta vic.SP0COL, y
    rts
!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Set a sprite's X and Y position

    Example: SPRPOS 0, 100, 100 would set sprite 0's X position to 100 and its Y position to 100
    The x position can be 0-511.

*/
SpritePosCmd:
    jsr SpriteCommon
    jsr basic.CHKCOM
    jsr Get16Bit
    lda $14
    sta r1L             // X
    lda $15
    sta r1H
    jsr basic.CHKCOM
    jsr Get8Bit
    sty r2L             // Y
    jsr PositionSprite
    rts

!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Turn a sprite on or off and set its shape data location

    Example: SPRSET 0, 1, $0d would turn sprite 0 on and set its shape data pointer to $0d in the current bank,
    which by default would be location $340 (bank 0 starts at $0 + $40 * $0d)

*/
SpriteSetCmd:
    jsr SpriteCommon
    jsr basic.CHKCOM
    jsr Get8Bit
    cpy #$02
    bcs !+
    tya
    ror
    ror
    sta r1L             // On/Off
    jsr basic.CHKCOM
    jsr Get8Bit
    sty r3L             // Pointer to shape data in the current bank
    lda #SPR_VISIBLE
    sta r0H
    jsr ChangeSpriteAttribute
    lda $288            // Get the page number of the current screen
    clc
    adc #$03            // Add 1016 to the start of screen RAM to get the sprite pointers
    sta r0H
    lda #$00
    clc
    adc #$f8
    sta r0L
    ldy #$00
    lda r3L
    sta (r0), y         // Write the pointer to the sprite data

    rts
!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Execute a PEEK function but return a 16-bit word instead of an 8-bit byte.

    Example: PRINT WEEK(250). Would return the 16-bit value in 250 & 251.

*/
WeekFun:
    jsr basic.GETADR    // Get the WEEK address

    ldy #$00
    lda ($14), y
    sta $63
    iny
    lda ($14), y
    sta $62

    // Thanks to Gregory Naçu for this trick. It allows writing a uint16 to the FAC
    // https://c64os.com/post/floatingpointmath
    ldx #$90
    sec
    jmp $bc49

/*

    Return the start address of the current screen.

    Example: PRINT SCRLOC(0). In the default C64 configuration, this would return 1024.

*/
ScrLocFun:
    lda $288
    sta $62
    lda #$00
    sta $63
    ldx #$90
    sec
    jmp $bc49

/*

    Check to see if there's a REU attached

    TODO: The reudetect routine is completely broken and needs a lot of work

*/
ReuFun:
    jsr reudetect
    lda #$00
    sta $62
    sta $63
    ldx #$90
    sec
    jmp $bc49

/*

    Store data from the C64's memory into an REU

    Example: STASH $400, $0, 1000, 0 would store the default screen to the REU at address $0, bank $0

*/
StashCmd:
    jsr ReuCommandCommon
    jsr REUStash

    rts

/*

    Retrieve data from the REU and store it in the C64's memory

    Example: FETCH $400, $0, 1000, 0 would retrieve 1000 bytes from the REU starting at address $0, bank $0 and store it to the default screen

*/
FetchCmd:
    jsr ReuCommandCommon
    jsr REUFetch

    rts

/*

    Copy a block of memory.

    Example: MEMCOPY $a000, $a000, $2000 would copy BASIC from ROM to RAM

*/
MemCopyCmd:
    jsr MemCommon
    jsr basic.CHKCOM
    jsr Get16Bit
    lda $14
    sta r2L
    lda $15
    sta r2H

    jsr MemCopy
    rts

/*

    Fill a block of memory with a character

    Example: MEMFILL $0400, $03e8, $20 would fill the screen with space characters.

*/
MemFillCmd:
    jsr MemCommon
    jsr basic.CHKCOM
    jsr Get8Bit
    sty r2L

    jsr MemFill

    rts

/*

    Set up parameters for STASH and FETCH since they're pretty much identical

*/
ReuCommandCommon:
    jsr MemCommon
    jsr basic.CHKCOM
    jsr Get16Bit        // Transfer length
    lda $14
    sta r2L
    lda $15
    sta r2H
    jsr basic.CHKCOM
    jsr Get8Bit         // REU bank number
    sty r3L

    rts

/*

    The existing memory routines start with 2 16-bit values and they're written to
    the same registers.

*/
MemCommon:
    jsr Get16Bit
    lda $14
    sta r0L
    lda $15
    sta r0H

    jsr basic.CHKCOM

    jsr Get16Bit
    lda $14
    sta r1L
    lda $15
    sta r1H

    rts

/*

    Fetch a 16 bit value from the current pointer. Value is returned in $14/$15

*/
Get16Bit:
    lda #$00
    sta zp.VALTYP
    jsr basic.FRMNUM
    jsr basic.GETADR
    rts

/*

    Fetch an 8 bit value which is returned in Y

*/
Get8Bit:
    jsr basic.FRMEVL    // Evaluate the expression after the token
    jsr $ad8d
    jsr basic.FACINX    // Convert the value in FAC1 to A(H)&Y(L)
    cmp #$00            // Is the high byte 0? (>255)
    bne !+              // Yup. Illegal quantity
    rts
!:
    jmp basic.ILLEGAL_QUANTITY

/*

    Common routine to grab some text, ensure it's a number and make sure
    it's < 16. This is used for the Background and Border commands which
    set the colors of each. Returns the value in A

*/
GetColor:
    jsr Get8Bit
    cpy #$10            // Is the color > 15?
    bcs !+
    tya
    rts
!:
    lda #<InvalidColorError
    ldy #>InvalidColorError
    jmp CustomError

/*

    Display a custom error. Pass in the LB of the error message in A and the HB in Y

*/
CustomError:
    sta $22
    tya
    jmp basic.CUSTERROR

/*

    Detokenize. Converts tokens back into PETSCII. This is called when you list a program
    with custom commands. It ensures that those commands expand correctly to their PETSCII
    form.

*/
ConvertFromTokens:
    bpl Out
    bit zp.GARBFL
    bmi Out
    cmp #$ff
    beq Out
    cmp #CMDSTART
    bcs NewList
    jmp $a724
Out:
    jmp $a6f3
NewList:
    sec
    sbc #$cb
    tax
    sty zp.FORPNT
    ldy #-1
Next:
    dex
    beq Found
Loop:
    iny
    lda NewTab, y
    bpl Loop
    bmi Next
Found:
    iny
    lda NewTab, y
    bmi OldEnd
    jsr basic.CHAROUT
    bne Found
OldEnd:
    jmp $a6ef

/*

    Tokenize. Converts PETSCII commands into tokens. This routine is called
    as you enter commands in either immediate mode, or as you enter lines of
    BASIC code.

*/
ConvertToTokens:
    ldx zp.TXTPTR
    ldy #4
    sty zp.GARBFL
NextChar:
    lda BUFFER, x
    bpl Normal
    cmp #$ff
    beq TakChar
    inx
    bne NextChar
Normal:
    cmp #' '
    beq TakChar
    sta zp.ENDCHAR
    cmp #QUOTE
    beq GetChar
    bit zp.GARBFL
    bvs TakChar
    cmp #'?'
    bne Skip
    lda #PRINT
    bne TakChar
Skip:
    cmp #'0'
    bcc Skip1
    cmp #'<'
    bcc TakChar
Skip1:
    sty zp.FBUFPT
    ldy #0
    sty zp.COUNT
    dey
    stx zp.TXTPTR
    dex
CmpLoop:
    iny
    inx
TestNext:
    lda BUFFER, x
    sec
    sbc basic.RESLST, y
    beq CmpLoop
    cmp #$80
    bne NextCmd
    ora zp.COUNT
TakChar1:
    ldy zp.FBUFPT
TakChar:
    inx
    iny
    sta BUFFER-5, y
    cmp #0
    beq End
    sec
    sbc #':'
    beq Skip2
    cmp #DATA-':'
    bne Skip3
Skip2:
    sta zp.GARBFL
Skip3:
    sec
    sbc #REM-':'
    bne NextChar
    sta zp.ENDCHAR
RemLoop:
    lda BUFFER, x
    beq TakChar
    cmp zp.ENDCHAR
    beq TakChar
GetChar:
    iny
    sta BUFFER-5, y
    inx
    bne RemLoop
NextCmd:
    ldx zp.TXTPTR
    inc zp.COUNT
Continue:
    iny
    lda basic.RESLST-1, y
    bpl Continue
    lda basic.RESLST, y
    bne TestNext
    beq NewTok
NotFound:
    lda BUFFER, x
    bpl TakChar1
End:
    sta BUFFER-3, y
    dec zp.TXTPTR+1
    lda #$ff
    sta zp.TXTPTR
    rts
NewTok:
    ldy #0
    lda NewTab, y
    bne NewTest
NewCmp:
    iny
    inx
NewTest:
    lda BUFFER, x
    sec
    sbc NewTab, y
    beq NewCmp
    cmp #$80
    bne NextNew
    ora zp.COUNT
    bne TakChar1
NextNew:
    ldx zp.TXTPTR
    inc zp.COUNT
Cont1:
    iny
    lda NewTab-1,y
    bpl Cont1
    lda NewTab, y
    bne NewTest
    beq NotFound

/*

    You can create your own custom error messages as well. Set $22 to the LB of the error message
    and A with the HB of the error message and then call basic.CUSTERROR

*/
InvalidColorError:
    .text "INVALID COLO"
    .byte 'R' + $80

NoReuError:
    .text "NO ATTACHED RE"
    .byte 'U' + $80

InvalidReuBankError:
    .text "INVALID REU BAN"
    .byte 'K' + $80

DiskError:
    .text "DISK I/"
    .byte 'O' + $80

// These are bit patterns that we poke into $d018 to set the screen location. We only care about
// bits 4-7 since bit 0 is unused and bits 1-3 select the char rom location within the VIC bank.
// There's probably a way to do this in code, but this is easier.
ScreenLoc:
    .byte %00000000, %00010000, %00100000, %00110000, %01000000, %01010000, %01100000, %01110000
    .byte %10000000, %10010000, %10100000, %10110000, %11000000, %11010000, %11100000, %11110000

} //.pseudopc
EndOfCode:   
