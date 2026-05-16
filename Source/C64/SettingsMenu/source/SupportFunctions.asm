
ScreenColorOnly:
   lda TblEscC+EscBorderColor
   sta BorderColorReg
   lda TblEscC+EscBackgndColor
   sta BackgndColorReg
   rts

SetC64TODfromRTC:
   ;Sets the C64 TOD clock from the Teensy RTC (with timezone offset)
   lda #rCtlC64TODfromRTCWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots 
   lda rRegLastHourBCD+IO1Port
   sta TODHoursBCD  ;stop TOD regs incrementing
   lda rRegLastMinBCD+IO1Port
   sta TODMinBCD
   lda rRegLastSecBCD+IO1Port
   sta TODSecBCD
   lda #9
   sta TODTenthSecBCD ;have to write 10ths to release latch, start incrementing
   rts

StartSelItem_WaitForTRDots:
   lda #rCtlStartSelItemWAIT ;kick off the selection
   sta wRegControl+IO1Port   
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
   ldy #$ff ;don't print dots
WaitForTRMain   ;Main wait loop
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
+  rts

CheckCommonKeys:

   cmp #ChrCRSRRight  ;Next Page
   bne +
   ;inc...
   jmp PageUpdate

+  cmp #ChrCRSRLeft  ;Previous Page
   bne +
   ;dec...
   jmp PageUpdate

+  cmp #ChrSpace  ;Reboot TeensyROM
   bne +
   lda #$00    
   sta $d011   ;turn off the display   
   lda #rCtlRebootTeensyROM 
   sta wRegControl+IO1Port
   ;no need to wait, TR/C64 will be rebooting...
+  rts 


PageUpdate:
   pla
   pla ; pop the jsr return address
   ;jump to indexed settings page
   jmp ColorConfigMenu