
//  ************** Labels **************

//enum TR_BASregsMatching  //synch with IOH_TR_BASIC.c
//{
   // registers:
.label TR_BASDataReg         = $b2   // (R/W) for TPUT/TGET data  
.label TR_BASContReg         = $b4   // (Write only) Control Reg 
.label TR_BASStatReg         = $b6   // (Read only) Status Reg 
.label TR_BASFileNameReg     = $b8   // (Write only) File name transfer
.label TR_BASStreamDataReg   = $ba   // (Read Only) File transfer stream data
.label TR_BASStrAvailableReg = $bc   // (Read Only) Signals stream data available

   // Control Reg Commands/Actions:
.label TR_BASCont_None       = $00   // No Action to be taken
.label TR_BASCont_SendFN     = $02   // Prep to send Filename from BAS to TR
.label TR_BASCont_LoadPrep   = $04   // Prep to load file from TR

   // StatReg Values:
.label TR_BASStat_Processing = $00   // No update, still processing
.label TR_BASStat_Ready      = $55   // Ready to Transfer
.label TR_BASStat_FNFError   = $aa   // File not found error

//}; //end enum synch


.label IO1Port  = $de00




//  ************** Commands **************


/*

    Send String out TeensyROM Serial USB Device Port

    Example- TPUT "HELLO WORLD!" will write "HELLO WORLD!" out the USB Serial port

*/
TPutCmd:
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

    Example- TGET A$ will read a char from the USB Serial port in to A$

    Modified from copy of BASIC GET Command
      Does not accept GET#
      Does not accept numeric argument (string only)
      Does not accept multiple arguments
      Reads from TR instead of keyboard
      
*/
TGetCmd:
    jsr $b3a6    //.,ab7b 20 a6 b3   //check not Direct, back here if ok
    ldx #$01     //.,ab92 a2 01      //set pointer low byte
    ldy #$02     //.,ab94 a0 02      //set pointer high byte
    lda #$00     //.,ab96 a9 00      //clear A
    sta $0201    //.,ab98 8d 01 02   //ensure null terminator
    stx $43      //.,ac11 86 43      //save READ pointer low byte
    sty $44      //.,ac13 84 44      //save READ pointer high byte
                 //                  //READ, GET or INPUT next variable from list
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
    bne !+       //.,ac2f d0 20      //branch if not null
                 //                  //pointer was to null entry
    lda TR_BASDataReg+IO1Port        //read from TR in data reg
    sta $0200    //.,ac38 8d 00 02   //save to buffer
    ldx #$ff     //.,ac3b a2 ff      //set pointer low byte
    ldy #$01     //.,ac3d a0 01      //set pointer high byte
    stx $7a      //.,ac4d 86 7a      //save BASIC execute pointer low byte
    sty $7b      //.,ac4f 84 7b      //save BASIC execute pointer high byte
!:  jsr $0073    //.,ac51 20 73 00   //increment and scan memory, execute pointer now points to
                 //                  //start of next data or null terminator
    bit $0d      //.,ac54 24 0d      //test data type flag, $FF = string, $00 = numeric
    bmi !+       //.,ac56 10 31      //branch if string (not numeric)
    jmp $ad99                        //type mismatch error
                 //                  //type is string, do string GET
!:  inx          //.,ac5c e8         //clear X ??
    stx $7a      //.,ac5d 86 7a      //save BASIC execute pointer low byte
    lda #$00     //.,ac5f a9 00      //clear A
    sta $07      //.,ac61 85 07      //clear search character
    clc          //.,ac71 18         //clear carry for add
    sta $08      //.,ac72 85 08      //set scan quotes flag
    lda $7a      //.,ac74 a5 7a      //get BASIC execute pointer low byte
    ldy $7b      //.,ac76 a4 7b      //get BASIC execute pointer high byte
    adc #$00     //.,ac78 69 00      //add to pointer low byte. this add increments the pointer
                 //                  //if the mode is INPUT or READ and the data is a "..." string
    bcc !+       //.,ac7a 90 01      //branch if no rollover
    iny          //.,ac7c c8         //else increment pointer high byte
!:  jsr $b48d    //.,ac7d 20 8d b4   //print string to utility pointer
    jsr $b7e2    //.,ac80 20 e2 b7   //restore BASIC execute pointer from temp
    jsr $a9da    //.,ac83 20 da a9   //perform string LET
    jsr $0079    //.,ac91 20 79 00   //scan memory
    beq !+       //.,ac94 f0 07      //branch if ":" or [EOL]
    cmp #$2c     //.,ac96 c9 2c      //compare with ","
    beq !+       //.,ac98 f0 03      //branch if ","
    jmp $ab4d    //.,ac9a 4c 4d ab   //else go do bad input routine
                 //                  //string terminated with ":", "," or $00
!:  lda $7a      //.,ac9d a5 7a      //get BASIC execute pointer low byte
    ldy $7b      //.,ac9f a4 7b      //get BASIC execute pointer high byte
    sta $43      //.,aca1 85 43      //save READ pointer low byte
    sty $44      //.,aca3 84 44      //save READ pointer high byte
    lda $4b      //.,aca5 a5 4b      //get saved BASIC execute pointer low byte
    ldy $4c      //.,aca7 a4 4c      //get saved BASIC execute pointer high byte
    sta $7a      //.,aca9 85 7a      //restore BASIC execute pointer low byte
    sty $7b      //.,acab 84 7b      //restore BASIC execute pointer high byte
    jsr $0079    //.,acad 20 79 00   //scan memory
    beq !+       //.,acb0 f0 2d      //branch if ":" or [EOL]
    ldx #$0b                         //error code $0B, syntax error
    jmp basic.ERROR                  //do error #X then warm start
!:  rts          //.,acfb 60      



/*

    Load a file from the TeensyROM

    Example- TLOAD "COOL.PRG" will load this file into memory

*/
TLoadCmd:
   jsr SendFileName  //send filename to TR
   
   //check for file present & load into TR RAM
   ldx #TR_BASCont_LoadPrep
   stx TR_BASContReg+IO1Port   
   jsr WaitForTR
   cmp #TR_BASStat_Ready
   beq !+
   //cmp #TR_BASStat_FNFError
   //bne !+
   ldx #basic.ERROR_FILE_NOT_FOUND    
   jmp basic.ERROR
   
   // load file into C64 memory
!: ldy #$49   //LOADING
   jsr $f12f  //print message from table at $f0bd
   lda TR_BASStreamDataReg+IO1Port
   sta r0L
   lda TR_BASStreamDataReg+IO1Port
   sta r0H
   ldy #0   //zero offset
   
!: lda TR_BASStrAvailableReg+IO1Port //are we done?
   beq !+   //exit the loop (zero flag set)
   lda TR_BASStreamDataReg+IO1Port //read from rRegStreamData+IO1Port increments address & checks for end
   sta (r0), y 
   iny
   bne !-
   inc r0H
   bne !-
   //good luck if we get to here... Trying to overflow and write to zero page
   ldx #basic.ERROR_OVERFLOW
   jmp basic.ERROR

   //finish up
   //size of prg = y+r0L/H, store this in 2D/2E
!: ldx r0H
   tya 
   clc
   adc r0L //add low byte and offset w/ carry
   bcc !+
   inx
!: sta $2d  //start of BASIC variables pointer (Lo)
   stx $2e  // (Hi)
   sta $ae  //End of load address (Lo)
   stx $af  // (Hi)
   
   lda #$76
   ldy #$a3
   jsr basic.STROUT   //print "READY."
   
   jmp $a52a
   //done at $A52A:    https://skoolkid.github.io/sk6502/c64rom/asm/A49C.html#A52A
   //jsr $a659	;reset execution to start, clear variables and flush stack
   //jsr $a533	;rebuild BASIC line chaining
   //jmp $a480 ;go do BASIC warm start (DOESN'T RETURN)
   //Also see https://codebase64.org/doku.php?id=base:runbasicprg
   //jmp $a7ae ;BASIC warm start/interpreter inner loop/next statement (Run)
   
   //rts



/*

    Save a file to the TeensyROM

    Example- TSAVE "COOL.PRG" will save this file to the TR SD card

*/
TSaveCmd:
    jsr SendFileName  //send filename to TR

    // save from memory to file
    
    // Check for File saved
    rts




//  ************** Functions **************


/*
    Utility- Send filename to be loaded/saved  to TR
    assumes it is next, called from tload/tsave
*/
SendFileName:
    jsr basic.FRMEVL    // Evaluate the expression after the token
    jsr basic.FRESTR    // Discard Temp string, get point to string and length
    // acc=msg len
    // $22=Message L pointer
    // $23=Message H pointer

    ldx #TR_BASCont_SendFN
    stx TR_BASContReg+IO1Port  //tell TR there's a FN coming   
    sta r0                     //store Filename length
    ldy #0
!:  cpy r0
    beq !+                     //last char, exit
    lda ($22),y
    //jsr kernal.VEC_CHROUT
    sta TR_BASFileNameReg+IO1Port //write to TR filename reg
    iny
    bne !- //255 char limit
!:  lda #$00
    sta TR_BASFileNameReg+IO1Port //terminate/end filename transfer
    rts


/*
    Utility- Wait for TR to complete processing
    return result in acc
*/
WaitForTR:
   //inc $0400 //spinner @ top/left
   lda TR_BASStatReg+IO1Port
   cmp #TR_BASStat_FNFError
   beq !+
   cmp #TR_BASStat_Ready
   bne WaitForTR
!: ldx #5 //require 1+5 consecutive reads of same TR_BASStat_FNFError/TR_BASStat_Ready to continue
!: cmp TR_BASStatReg+IO1Port
   bne WaitForTR
   dex
   bne !-
   //acc holds the result
   rts
