

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "..\source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "..\source\CommonDefs.i" ;Common between crt loader and main code in RAM

;enum ASIDregsMatching  //synch with ASIDPlayer.asm
   ASIDAddrReg        = 0x02;   // Data type and SID Address Register (Read only)
   ASIDDataReg        = 0x04;   // ASID data, increment queue Tail (Read only)
   ASIDContReg        = 0x08;   // Control Reg (Write only)
                            
   ASIDContIRQOn      = 0x01;   //enable ASID IRQ
   ASIDContExit       = 0x02;   //Disable IRQ, Send TR to main menu
                             
   ASIDAddrType_Skip  = 0x00;   // No data/skip
   ASIDAddrType_Char  = 0x20;   // Character data
   ASIDAddrType_Start = 0x40;   // ASID Start message
   ASIDAddrType_Stop  = 0x60;   // ASID Stop message
   ASIDAddrType_SID1  = 0x80;   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2  = 0xa0;   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3  = 0xc0;   // Lower 5 bits are SID3 reg address
   ASIDAddrType_SID4  = 0xe0;   // Lower 5 bits are SID4 reg address
                            
   ASIDAddrType_Mask  = 0xe0;   // Mask for Type
   ASIDAddrAddr_Mask  = 0x1f;   // Mask for Address
;end enum synch

   SpinIndUnexpType   = C64ScreenRAM+40*2+5 ;spin indicator: error: Unexpected reg type or skip received
   SpinIndSID1Write   = C64ScreenRAM+40*2+6 ;spin indicator: SID1 write
   SIDRegColorStart   = C64ColorRAM +40*2+7
   
   RegFirstColor  = PokeWhite; PokeLtRed ;PokeOrange ;PokeLtBlue ;PokeWhite
   RegSecondColor = PokeDrkGrey; PokeRed ;PokeBrown ;PokeBlue ;PokeDrkGrey ; PokeLtGrey, PokeMedGrey, PokeDrkGrey
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
   jsr SIDVoicesOff
   jmp ASIDMainLoop

+  cmp #'?'  ;Help/refresh
   bne +  
   lda #<MsgASIDPlayerMenu
   ldy #>MsgASIDPlayerMenu
   jsr PrintString 
   jmp ASIDMainLoop


+  jmp ASIDMainLoop


ASIDInterrupt: 

;read the addr/type and data:
   ldx ASIDAddrReg+IO1Port
   ldy ASIDDataReg+IO1Port
   
;apply the addr/type and data:
   txa
   and #ASIDAddrType_Mask
   
   cmp #ASIDAddrType_SID1
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
   sta SIDLoc,x
   lda #RegFirstColor ;set the indicator color
   sta SIDRegColorStart,x
   inc SpinIndSID1Write
   jmp ASIDIntFinished
   
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
   tya
   jsr SendChar
   jmp ASIDIntFinished

   ;unexpected type or skip received
+  inc SpinIndUnexpType
   
ASIDIntFinished:
   jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S IRQ HANDLER ROUTINE


   !src "source\ASIDsupport.asm"






   


