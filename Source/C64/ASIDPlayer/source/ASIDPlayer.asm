

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "..\source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "..\source\CommonDefs.i" ;Common between crt loader and main code in RAM

;enum ASIDregsMatching  //synch with ASIDPlayer.asm
   ASIDAddrReg        = 0xc2;   // Data type and SID Address Register (Read only)
   ASIDDataReg        = 0xc4;   // ASID data, increment queue Tail (Read only)
   ASIDContReg        = 0xc8;   // Control Reg (Write only)
                            
   ASIDContIRQOn      = 0x01;   //enable ASID IRQ
   ASIDContIRQOff     = 0x02;   //disable ASID IRQ
   ASIDContExit       = 0x03;   //Disable IRQ, Send TR to main menu
                             
   ASIDAddrType_Skip  = 0x00;   // No data/skip
   ASIDAddrType_Char  = 0x20;   // Character data
   ASIDAddrType_Start = 0x40;   // ASID Start message
   ASIDAddrType_Stop  = 0x60;   // ASID Stop message
   ASIDAddrType_SID1  = 0x80;   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2  = 0xa0;   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3  = 0xc0;   // Lower 5 bits are SID3 reg address
   ASIDAddrType_Error = 0xe0;   // Error from parser
                            
   ASIDAddrType_Mask  = 0xe0;   // Mask for Type
   ASIDAddrAddr_Mask  = 0x1f;   // Mask for Address
;end enum synch

  ;SpinIndSID4Write   = C64ScreenRAM+40*2+1 ;spin indicator: SID1 write
   SpinIndSID3Write   = C64ScreenRAM+40*2+2 ;spin indicator: SID1 write
   SpinIndSID2Write   = C64ScreenRAM+40*2+3 ;spin indicator: SID1 write
   SpinIndSID1Write   = C64ScreenRAM+40*2+4 ;spin indicator: SID1 write
   SpinIndUnexpType   = C64ScreenRAM+40*2+5 ;spin indicator: error: Unexpected reg type or skip received
   SpinIndPacketError = C64ScreenRAM+40*2+6 ;spin indicator: error from packet parser, see AddErrorToASIDRxQueue in IOH_ASID.c
   SIDRegColorStart   = C64ColorRAM +40*2+7
   MuteColorStart     = C64ColorRAM +40*2+34 ;start of "Mute" diaply
   
   RegFirstColor  = PokeWhite
   RegSecondColor = PokeDrkGrey
                    ;then to black/off
   
	BasicStart = $0801
   code       = $080D ;2061

   *=BasicStart
   ;BASIC SYS header
   !byte $0b,$08,$01,$00,$9e  ; Line 1 SYS
   !tx "2061" ;"2061" Address for sys start in text
   !byte $00,$00,$00
   
   *=code  ; Start location for code


ASIDInit:
;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   lda #<MsgASIDPlayerMenu
   ldy #>MsgASIDPlayerMenu
   jsr PrintString 
 
   jsr UpdateAllSIDAddress
   jsr SIDinit

;set up ASID interrupt:
   ; DISABLE MASKABLE INTERRUPTS, AND THEN TURN THEM OFF
   sei              
   lda #%01111111   ; BIT 7 (OFF) MEANS THAT ANY 1S WRITTEN TO CIA ICRS TURN THOSE BITS OFF
   sta $dc0d        ;    CIA#1 INTERRUPT CONTROL REGISTER (IRC): DISABLE ALL INTERRUPTS
   sta $dd0d        ;    CIA#2 ICR: DISABLE ALL INTERRUPTS
   lda $dc0d        ; ACK (CLEAR) ANY PENDING CIA1 INTERRUPTS (READING CLEARS 'EM)
   lda $dd0d        ;    SAME FOR CIA2
   asl $d019        ; TOSS ANY PENDING VIC INTERRUPTS (WRITING CLEARS 'EM, VIA RMW MAGIC)
   ; HOOK INTERRUPT ROUTINE (NORMALLY POINTS TO $EA31)
   lda #<ASIDInterrupt 
   sta $0314
   lda #>ASIDInterrupt
   sta $0315 
   cli              ; RESTORE INTERRUPTS, HOOKING COMPLETE   
   
   lda #ASIDContIRQOn  ;enable the interrupt from TR
   sta ASIDContReg+IO1Port

ShowKeyboardCommands: ;including SID2/3 address from smcSIDxaddress
   jsr ClearLowerScreen
   jsr SetCursorPosCol
   lda #<MsgASIDPlayerCommands1
   ldy #>MsgASIDPlayerCommands1
   jsr PrintString 
   
   ;sid2 addr:
   lda smcSID1address+2 ;high byte
   ldx smcSID1address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands2
   ldy #>MsgASIDPlayerCommands2
   jsr PrintString 
   
   ;sid2 addr:
   lda smcSID2address+2 ;high byte
   ldx smcSID2address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands3
   ldy #>MsgASIDPlayerCommands3
   jsr PrintString 
   
   ;sid3 addr:
   lda smcSID3address+2 ;high byte
   ldx smcSID3address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands4
   ldy #>MsgASIDPlayerCommands4
   jsr PrintString 
   jsr SetCursorPosCol
   ;end of ShowKeyboardCommands, continue to main loop...
   
ASIDMainLoop: 
  
;SID reg color update: 25 SID registers updated once per 10th/sec
   lda TODTenthSecBCD ;read 10ths releases latch
smcLastUpd
   cmp #0
   beq GetKeypress
   sta smcLastUpd+1
   ;inc SpinIndUnexpType  ;temp to check update cycle

   ldx #0
regcolorupdate
   lda SIDRegColorStart,x
   and #$0f   ;only 4 bits are valid
   beq nextsidreg   ;skip reg if 0 (black) already
   
   cmp #RegFirstColor ;color set by interrupt
   bne +
   lda #RegSecondColor ;RegFirstColor -> RegSecondColor
   jmp setcolor   
+
   lda #PokeBlack ;RegSecondColor (or anything else) -> black
 
setcolor 
   sta SIDRegColorStart,x
nextsidreg
   inx  ;next register
   cpx #25 ;25 registers 0-24
   bne regcolorupdate

 
;get/check key press
GetKeypress:
   jsr ScanKey ;needed since timer/raster interrupts are disabled
   jsr GetIn
   beq ASIDMainLoop
   
   cmp #'x'  ;Exit TR ASID player
   bne +  
   lda #ASIDContExit  ;turn off the interrupt and go back to main menu
   sta ASIDContReg+IO1Port
   ;C64 is resetting now, probably won't get far...
   ; IRQ back to default
   sei
   lda #<IRQDefault
   ldx #>IRQDefault
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi
   lda #%10000001 
   sta $dc0d  ;CIA int ctl
   lda #0
   sta $d01a  ;irq enable
   inc $d019
   lda $dc0d  ;CIA int ctl
   cli 
   jsr SIDinit
   rts ;return to BASIC (if not TR main menu)

+  cmp #'s'  ;screen on/off
   bne +  
smcScreenOnOff
   lda #0
   bne TurnOff
   ;Turn screen back on
   dec smcScreenOnOff+1
   lda #$00   ;turn off the display 
   ldx #PokeBlack  ;set to black
   jmp SetScreen
TurnOff   
   inc smcScreenOnOff+1
   lda #$1b   ;turn the display back on
   ldx #BorderColor  ;set to regular/default
SetScreen
   sta $d011 
   stx BorderColorReg
   jmp ASIDMainLoop

+  cmp #'v'  ;voices off
   bne +  
   jsr SIDinit
   jmp ASIDMainLoop

+  cmp #'m'  ;mute toggle
   bne +  
smcPausePlay
   lda #0   ;0=currently streaming/not muted, 1=muted
   beq ++
   ;stream is off, unmute:
   dec smcPausePlay+1
   lda #ASIDContIRQOn  ;enable the interrupt from TR
   sta ASIDContReg+IO1Port
   lda #PokeBlack
   jmp MuteColor
   ;stream is on, mute it and kill voices:
++ inc smcPausePlay+1
   lda #ASIDContIRQOff  ;disable the interrupt from TR
   sta ASIDContReg+IO1Port
   jsr SIDinit
   lda #PokeRed
MuteColor
   ldx #4
-  sta MuteColorStart,x
   dex
   bne -
   jmp ASIDMainLoop

+  cmp #'c'  ;Refresh/Clear Screen
   bne +  
   lda #<MsgASIDPlayerMenu
   ldy #>MsgASIDPlayerMenu
   jsr PrintString 
   lda #0
   sta smcScreenFull+1 
   jmp ASIDMainLoop

+  cmp #'?'  ;Show Keyboard Commands
   bne + 
   jmp ShowKeyboardCommands   

+  cmp #'d'  ;Show Register Decoder
   bne +  
   jsr ClearLowerScreen
   jsr SetCursorPosCol
   lda #<MsgASIDPlayerDecoder
   ldy #>MsgASIDPlayerDecoder
   jsr PrintString 
   jsr SetCursorPosCol
   jmp ASIDMainLoop

+  cmp #'1'  ;1st SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID1addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID1addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  cmp #'2'  ;2nd SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID2addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID2addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  cmp #'3'  ;3rd SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID3addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID3addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  jmp ASIDMainLoop

ClearLowerScreen:
   ;uses X and Acc 
   lda #ChrSpace
   ldx #$00
-  sta C64ScreenRAM+40*6     ,x  ; 240 first block
   sta C64ScreenRAM+40*6 +256,x  ; 496 second block
                                 ; 752 8 byte overlap
   sta C64ScreenRAM+40*25-256,x  ; 744 last block
   dex
   bne -
   stx smcScreenFull+1 ;clear screen full flag
   rts
   
SetCursorPosCol:
   ;set cursor position & default color
   ldx #6 ;row
   ldy #0 ;col
   clc
   jsr SetCursor   
   lda #OptionColor
   jsr SendChar
   inc smcScreenFull+1 ;set screen full flag
   rts
   
   
!align $ff, 0 , 0  ;page align interrupt to reduce some branch times.
ASIDInterrupt: 
;read the addr/type and data:
   ldx ASIDAddrReg+IO1Port
   ldy ASIDDataReg+IO1Port
   
;apply the addr/type and data:
   txa
   and #ASIDAddrType_Mask
   ;Acc holds msg type
   cmp #ASIDAddrType_SID1
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID1address
   sta $faca,x
   lda #RegFirstColor ;set the indicator color
   sta SIDRegColorStart,x
   inc SpinIndSID1Write
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_SID2
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID2address
   sta $facb,x
   ;lda #RegFirstColor ;set the indicator color
   ;sta SIDRegColorStart,x
   inc SpinIndSID2Write
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_SID3
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID3address
   sta $facc,x
   ;lda #RegFirstColor ;set the indicator color
   ;sta SIDRegColorStart,x
   inc SpinIndSID3Write
   jmp ASIDIntFinished

;+  cmp #ASIDAddrType_SID4
;   bne + 
;   txa
;   and #ASIDAddrAddr_Mask
;   tax ;x now holds SID offset address
;   tya ;acc now holds data to write
;smcSID4address
;   sta $d460,x
;   ;lda #RegFirstColor ;set the indicator color
;   ;sta SIDRegColorStart,x
;   inc SpinIndSID4Write
;   jmp ASIDIntFinished
   
+  cmp #ASIDAddrType_Start
   bne + 
   jsr SIDinit
   lda #<MsgASIDStart
   ldy #>MsgASIDStart
   jsr PrintString 
   jmp ASIDIntFinished
   
+  cmp #ASIDAddrType_Stop
   bne + 
   jsr SIDinit
   lda #<MsgASIDStop
   ldy #>MsgASIDStop
   jsr PrintString 
   jmp ASIDIntFinished
   
+  cmp #ASIDAddrType_Char 
   bne + 
   ;print a character after checking if screen is full.
smcScreenFull
   ldx #$01 ;non-zero means screen is full
   beq ++
   jsr ClearLowerScreen ;leaves y reg in tact
++ tya
   jsr SendChar
   ;check cursor position if return char
   cmp #ChrReturn
   bne ASIDIntFinished
   sec
   jsr SetCursor ;read current to load x (row)
   cpx #23
   bmi ASIDIntFinished ;skip if row is <23
   inc smcScreenFull+1 ;otherwise set screen full flag
   jsr SetCursorPosCol ; and set cursor position
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_Error 
   bne + 
   inc SpinIndPacketError
   jmp ASIDIntFinished

   ;unexpected type or skip received
+  inc SpinIndUnexpType
   
ASIDIntFinished:
   jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S IRQ HANDLER ROUTINE


   !src "source\ASIDsupport.asm"


!align 32, 0 , 0  ;32 byte align to inxex within page.
memNoSID:
;set "none" sid 2/3 writes to here, don't add anything after this!




   


