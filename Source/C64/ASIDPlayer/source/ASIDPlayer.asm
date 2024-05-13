

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "..\source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "..\source\CommonDefs.i" ;Common between crt loader and main code in RAM

;enum ASIDregsMatching   synch with IOH_ASID.c
   ASIDAddrReg        = 0x02; // Data type and SID Address Register
   ASIDDataReg        = 0x04; // ASID data
   ASIDCompReg        = 0x08; // Read Complete/good
                      
   ASIDAddrType_Skip  = 0x00; // No data/skip
   ASIDAddrType_Char  = 0x20; // Character data
   ASIDAddrType_Start = 0x40; // 
   ASIDAddrType_Stop  = 0x60; // 
   ASIDAddrType_SID1  = 0x80; // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2  = 0xa0; // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3  = 0xc0; // Lower 5 bits are SID3 reg address
   ASIDAddrType_SID4  = 0xe0; // Lower 5 bits are SID4 reg address
   ASIDAddrType_Mask  = 0xe0; // 
   ASIDAddrAddr_Mask  = 0x1f; // 


   SpinIndAddrMiscomp = C64ScreenRAM+40*2-1-0 ;spin top-1, right-0: addr miscompare/retry
   SpinIndDataMiscomp = C64ScreenRAM+40*2-1-1 ;spin top-1, right-1: data miscompare/retry
   SpinIndSkipType    = C64ScreenRAM+40*2-1-2 ;spin top-1, right-2: Skip message received
   SpinIndUnexpVal    = C64ScreenRAM+40*2-1-3 ;spin top-1, right-3: unexpected read value
   SpinIndNonASIDInt  = C64ScreenRAM+40*2-1-4 ;spin top-1, right-4: CIA generated IRQ received
   SpinIndSID1Write   = C64ScreenRAM+40*2-1-5 ;spin top-1, right-5: SID1 write
   
	BasicStart = $0801
   code       = $080D ;2061

   *=BasicStart
   ;BASIC SYS header
   !byte $0b,$08,$01,$00,$9e  ; Line 1 SYS
   !tx "2061" ;"2061" Address for sys start in text
   !byte $00,$00,$00
   
   *=code  ; Start location for code

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
   
   ;;Set the timer behavior
   ;lda #%10000001   ; CIA#1 ICR: B0->1 = ENABLE TIMER A INTERRUPT,
   ;sta $dc0d        ;    B7->1 = FOR B0-B6, 1 BITS GET SET, AND 0 BITS IGNORED
   ;lda $dc0e        ; CIA#1 TIMER A CONTROL REGISTER
   ;and #%10000000   ; PRESERVE KERNAL-SET TOD CLOCK NTSC OR PAL SELECTION
   ;ora #%00010001   ; START TIMER A,CONTINUOUS RUN MODE, LATCHED VALUE INTO TIMER A COUNTER
   ;sta $dc0e        ; Write it back 
   
   cli              ; RESTORE INTERRUPTS, HOOKING COMPLETE   
   
   
ASIDMainLoop: 
   jsr ScanKey ;needed since timer/raster interrupts are disabled
   jsr GetIn
   beq ASIDMainLoop
   
   cmp #'x'  ;Exit M2S
   bne +  
   
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
   rts ;return to BASIC

+  jmp ASIDMainLoop


ASIDInterrupt: 
        ;pha
        ;txa
        ;pha
        ;tya
        ;pha
        ;dec $d019
   ;lda $dc0d          ; ACK (CLEAR) CIA#1 INTERRUPT
   ;beq +
   ;inc SpinIndNonASIDInt
   ;;jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S 60HZ(?) IRQ HANDLER ROUTINE
   ;jmp ASIDIntFinished ;skip if IRQ was CIA generated

;read the addr/type and data:
+  ldx ASIDAddrReg+IO1Port
-  stx $fb
   ;read it again to make sure:
   lda ASIDAddrReg+IO1Port
   cmp $fb
   beq +
   inc SpinIndAddrMiscomp
   tax
   jmp -
+  

   ldy ASIDDataReg+IO1Port
-  sty $fb
   ;read it again to make sure:
   lda ASIDDataReg+IO1Port
   cmp $fb
   beq +
   inc SpinIndDataMiscomp
   tay
   jmp -
+  
   
   lda ASIDCompReg+IO1Port ;send read confirmed
   
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
   
+  cmp #ASIDAddrType_Skip
   bne + 
   inc SpinIndSkipType
   jmp ASIDIntFinished
   
   ;unexpected read
+  inc SpinIndUnexpVal
   ;txa ;addr
   ;jsr PrintHexByte
   ;lda #'+'
   ;jsr SendChar
   ;tya ;data
   ;jsr PrintHexByte
   ;lda #' '
   ;jsr SendChar
   
ASIDIntFinished:
   ;lda $dc0d          ; ACK (CLEAR) CIA#1 INTERRUPT
   ;lda $dd0d        ;    SAME FOR CIA2
   ;rti
        ;dec $d019
        ;pla
        ;tay
        ;pla
        ;tax
        ;pla
        ;rti
   jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S 60HZ(?) IRQ HANDLER ROUTINE

   
   !src "source\ASIDsupport.asm"






   


