
ScreenColorOnly:
   lda TblEscC+EscBorderColor
   sta BorderColorReg
   lda TblEscC+EscBackgndColor
   sta BackgndColorReg
   rts

;SetRTCfromEthernet:
;   ; Synchs the Teensy RTC with time acquired from Ethernet
;   lda #rCtlSetRTCfromNetWAIT
;   sta wRegControl+IO1Port
;   jsr WaitForTRDots ;use WaitForTRWaitMsg instead?
;   rts
 
SetC64TODfromRTC:
   ;Sets the C64 TOD clock from the Teensy RTC (with timezone offset)
   lda #rCtlC64TODfromRTCWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRNone 
SetC64TODfromRTC_Preloaded:
   lda rRegLastHourBCD+IO1Port
   sta TODHoursBCD  ;stop TOD regs incrementing
   lda rRegLastMinBCD+IO1Port
   sta TODMinBCD
   lda rRegLastSecBCD+IO1Port
   sta TODSecBCD
   lda #9
   sta TODTenthSecBCD ;have to write 10ths to release latch, start incrementing
   rts

;WaitForTR* uses acc, X and Y
WaitForTRDots:  ;prints a dot per second while waiting, doesn't move cursor
   ldy TODSecBCD ;reset dot second counter
   jmp WaitForTRMain
WaitForTRWaitMsg:  ;Print Waiting message in upper right and waits
   ldx #1 ;row   Show "Waiting:" over time disp
   ldy #29  ;col
   clc
   jsr SetCursor
   lda #<MsgWaiting
   ldy #>MsgWaiting
   jsr PrintString
WaitForTRNone: ;No waiting msg or dots
   ldy #$ff ;don't print dots
WaitForTRMain   ;Main wait loop
!ifndef DbgOffline {
   inc C64ScreenRAM+40*2-2 ;spinner @ top-1, right-1
   cpy #$ff
   beq +    ;skip dot printing if wait message selected
   cpy TODSecBCD  ;no latch/unlatch needed for only reading seconds
   beq +
   ldy TODSecBCD  ;print 1 dot/sec while waiting
   lda #'.'
   jsr SendChar
+  lda rwRegStatus+IO1Port
   cmp #rsC64Message
   beq ++
   cmp #rsReady
   bne WaitForTRMain
++ ldx#5 ;require 1+5 consecutive reads of same rsC64Message/rsReady to continue
-  cmp rwRegStatus+IO1Port
   bne WaitForTRMain
   dex
   bne -
   ; the ball is in the C64 court, either done or display message and cont waiting
   cmp #rsReady
   beq +
   ; display message:
   lda #rsstSerialStringBuf ;pre-populated message
   jsr PrintSerialString
   lda #rsContinue    ;tell fw we're done reading msg, continue
   sta rwRegStatus+IO1Port
   jmp WaitForTRMain
}
+  rts

MsgWaiting:
   !tx EscC,EscTimeColor, " Waiting:", 0
