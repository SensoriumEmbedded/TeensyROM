

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


   SpinIndUnexpVal    = C64ScreenRAM+40*2-1-0 ;spin top-1, right-0: unexpected read value
   SpinIndAddrMiscomp = C64ScreenRAM+40*2-1-1 ;spin top-1, right-1: addr miscompare/retry
   SpinIndDataMiscomp = C64ScreenRAM+40*2-1-2 ;spin top-1, right-2: data miscompare/retry
   SpinIndSID1Write   = C64ScreenRAM+40*2-1-3 ;spin top-1, right-3: SID1 write
   
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

ASIDPlayLoop: 
   
   ldx ASIDAddrReg+IO1Port
-  stx $fb
   ;read it again to make sure:
   lda ASIDAddrReg+IO1Port
   cmp $fb
   beq +
   inc  SpinIndAddrMiscomp
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
   jmp ASIDPlayLoop ;keep grabbing data until caught up
   
+  cmp #ASIDAddrType_Start
   bne + 
   jsr SIDinit
   lda #<MsgASIDStart
   ldy #>MsgASIDStart
   jsr PrintString 
   jmp ASIDPlayLoop ;keep grabbing data until caught up
   
+  cmp #ASIDAddrType_Stop
   bne + 
   jsr SIDinit
   lda #<MsgASIDStop
   ldy #>MsgASIDStop
   jsr PrintString 
   jmp ASIDPlayLoop ;keep grabbing data until caught up
   
+  cmp #ASIDAddrType_Char
   bne + 
   tya
   jsr SendChar
   jmp ASIDPlayLoop ;keep grabbing data until caught up
   
+  cmp #ASIDAddrType_Skip
   beq + 
   ;unexpected read
   inc SpinIndUnexpVal
   ;txa ;addr
   ;jsr PrintHexByte
   ;lda #'+'
   ;jsr SendChar
   ;tya ;data
   ;jsr PrintHexByte
   ;lda #' '
   ;jsr SendChar
   
+  jsr GetIn
   beq ASIDPlayLoop
   
   cmp #'x'  ;Exit M2S
   bne +  
   jsr SIDinit
   rts ;return to BASIC


+  jmp ASIDPlayLoop

   !src "source\ASIDsupport.asm"

