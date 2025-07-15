

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   
   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2025                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of .run
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of .run
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of .run
   !byte SysAddress%10+$30         ; Calculate 4th digit of .run
   !byte $00                       ; BASIC end of line marker
BasicEnd    
   !word $0000                     ; BASIC end of program

SysAddress
   jmp Init
   
   !src "source\C64Defs.asm"
   !src "source\SSTSupport.asm"

Init:
   ;screen setup:     
   lda #PokeDrkGrey
   sta BorderColorReg
   lda #PokeBlack
   sta BackgndColorReg
   
   lda #<MsgSimpleSwiftlinkTerminal
   ldy #>MsgSimpleSwiftlinkTerminal
   jsr PrintString 
   
   jsr sw_setup ;clears RX queue, sets def (2400) baud rate, Sets up Rx NMI vectors, turns on interrupts
   lda #SW_Baud_38400
   jsr sw_setbaud
   ;jsr sw_enable
 
MainLoop:
   jsr sw_CharIn ;get a char from the swiftlink Rx queue
   beq +
   ;char received from Swiftlink buf, print it
   jsr SendChar 
   jmp MainLoop
   
+  jsr GetIn ;get a char from keyboard
   beq MainLoop
   
   ;character received from keyboard
   cmp #ChrF1 ;connect to retro BBS
   beq ConnectRetroCampus
   cmp #ChrF2  ;return to BASIC
   beq DisableExit
   
+  jsr sw_CharOut ;send it to swiftlink Tx channel
   jmp MainLoop


ConnectRetroCampus:
   lda #<MsgATDTRetroBBS
   ldy #>MsgATDTRetroBBS   
   jsr sw_StrOut
   jmp MainLoop

DisableExit:
   jsr sw_disable
   rts 

MsgATDTRetroBBS:
   !tx "atdt bbs.retrocampus.com:6510", ChrReturn, 0

MsgSimpleSwiftlinkTerminal:    
   !tx ChrToLower, ChrYellow, ChrClear, ChrReturn
   !tx "Simple Swiftlink Terminal", ChrReturn
   !tx "  F1- Connect to Retro Campus", ChrReturn
   !tx "  F2- Exit", ChrReturn
   !tx ChrReturn, 0

EOF:
   !byte 0
   

   


