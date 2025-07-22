

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
   
   jsr sw_setup ;clears RX queue, sets def (2400) baud rate, Sets up Rx NMI vectors, disables interrupt
   
   lda #<MsgSimpleSwiftlinkTerminal
   ldy #>MsgSimpleSwiftlinkTerminal
   jsr PrintString 
   
-  jsr GetIn ;get a char from keyboard
   cmp #'p'
   beq +
   cmp #'i'
   bne -
+  pha ;save it for below
   jsr SendChar ;print it   
   
   lda #<MsgSimpleSwiftlinkTerminalBaud
   ldy #>MsgSimpleSwiftlinkTerminalBaud
   jsr PrintString 
-  jsr GetIn ;get a char from keyboard
   beq -
   pha 
   jsr SendChar ;print it   
   pla
   cmp #'1'
   bne +
   lda #SW_Baud_2400
   jmp ++
+  cmp #'2'
   bne +
   lda #SW_Baud_38400
   jmp ++
+  cmp #'3'
   bne -
   lda #SW_Baud_230400
++ jsr sw_setbaud
   
   lda #<MsgSimpleSwiftlinkTerminalReady
   ldy #>MsgSimpleSwiftlinkTerminalReady
   jsr PrintString 
   
   pla
   cmp #'p'
   beq MainLoop_Poll
 
MainLoop_RxQ:
   jsr sw_enable  ;enable Rx interrupt 

-  jsr sw_CharIn ;get a char from the swiftlink Rx queue
   beq +
   
   ;char received from Rx buf, print it
   jsr SendChar 
   jmp - ;keep processing Rx buff

+  jsr CheckKbdTx
   jmp -

MainLoop_Poll:
   jsr sw_disable  ;disable interrupt, we're polling

-  lda sw_stat
   and #%00001000 ; mask out all but "receive register full/ready" bit
   beq +    ; skip if no data ready
   
   ;char available direct from Swiftlink buf, read and print it
   lda sw_data ;read a byte from Rx
   jsr SendChar 
   jmp - ;keep processing Rx buff

+  jsr CheckKbdTx
   jmp -


CheckKbdTx:   
   jsr GetIn ;get a char from keyboard
   beq ++
   
   ;character received from keyboard
   cmp #ChrF1 ;connect to retro BBS
   bne +
   lda #<MsgATDTRetroBBS
   ldy #>MsgATDTRetroBBS   
   jsr sw_StrOut
   rts
   
+  cmp #ChrF2 ;Exit
   bne +
   jsr sw_disable  ;disable interrupt
   pla ;pull this jsr address from stack to return to BASIC
   pla
   rts
   
+  cmp #ChrF4 ;Restart
   bne +
   jsr sw_disable  ;disable interrupt
   pla ;pull this jsr address from stack
   pla
   jmp Init
   
+  jsr sw_CharOut ;send it to swiftlink Tx channel
++ rts

MsgATDTRetroBBS:
   !tx "atdt bbs.retrocampus.com:6510", ChrReturn, 0

MsgSimpleSwiftlinkTerminal:    
   !tx ChrToLower, ChrYellow, ChrClear, ChrReturn
   !tx "Simple Swiftlink Terminal", ChrReturn
   !tx " Rx (p)olling or (i)nterrupt? ", 0

MsgSimpleSwiftlinkTerminalBaud:    
   !tx ChrReturn, ChrReturn
   !tx " Baud rate", ChrReturn
   !tx "  1- 2400, 2-38k, 3-230k? ", 0

MsgSimpleSwiftlinkTerminalReady:    
   !tx ChrReturn, ChrReturn
   !tx "Terminal Ready.", ChrReturn
   !tx "  F1- Connect to Retro Campus", ChrReturn
   !tx "  F2- Exit", ChrReturn
   !tx "  F4- Restart", ChrReturn, ChrReturn
   !tx 0

EOF:
   !byte 0
   

