
ScreenColorOnly:
   lda TblEscC+EscBorderColor
   sta BorderColorReg
   lda TblEscC+EscBackgndColor
   sta BackgndColorReg
   rts

SetRTCfromEthernet:
   ; Synchs the Teensy RTC with time acquired from Ethernet
   lda #rCtlSetRTCfromNetWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots ;use WaitForTRWaitMsg instead?
   rts
 
SetC64TODfromRTC:
   ;Sets the C64 TOD clock from the Teensy RTC (with timezone offset)
   lda #rCtlC64TODfromRTCWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots 
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

CheckCommonKeys:

   cmp #ChrCRSRRight  ;Next Page
   bne +
   ;inc...
   inc bPageNum
   lda bPageNum
   cmp bTotalPages
   bne PopPageUpdate
   lda #0
   sta bPageNum
   jmp PopPageUpdate

+  cmp #ChrCRSRLeft  ;Previous Page
   bne +
   ;dec...
   lda bPageNum
   bne ++
   lda bTotalPages
   sta bPageNum
++ dec bPageNum
   jmp PopPageUpdate

+  cmp #ChrF1  ;Back to TeensyROM menu
   beq ++
   cmp #ChrSpace  ;Back to TeensyROM menu
   bne +
++ lda #rCtlReturnToMainMenu 
   sta wRegControl+IO1Port
   ;C64 will be reset...
   pla
   pla ; pop the jsr return address to return to BASIC
   
+  rts 

PopPageUpdate:
   pla
   pla ; pop the jsr return address
PageUpdate:   
   ;jump to indexed settings page
   lda bPageNum
   asl   ;mult by 2
   tax
   lda tblSettingsPages,x
   sta smcJmpPage+1
   lda tblSettingsPages+1,x
   sta smcJmpPage+2
smcJmpPage
   jmp $0000 ;updated above

CommonInit:
   ;do common init stuff
   jsr PrintBanner

   ldx #21 ;row
   ldy #0 ;col
   jsr SetCursor
   lda TblEscC+EscTRBannerColor
   sta $0286  ;set text color   
   ;draw line
   lda #192
   ldx #40
-  jsr SendChar
   dex
   bne -
   clc
   lda #<MsgMenuPageSelections
   ldy #>MsgMenuPageSelections
   jsr PrintString 
   
   ; print page #/# info
   ldx bPageNum  ;current page num
   inx ;make 1 based
   txa
   jsr PrintIntByte
   lda #'/'
   jsr SendChar   
   lda bTotalPages  ;num of pages
   jsr PrintIntByte  
   
   lda #<MsgMenuExitSelection
   ldy #>MsgMenuExitSelection
   jsr PrintString
   ldx #2 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   rts


MsgMenuPageSelections:
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "<= CRSR =>", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Next/Previous page", EscC,EscNameColor, " ("
   !tx 0 
MsgMenuExitSelection:
   !tx ")", ChrReturn, EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Space/F1", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,  "Exit to Main menu"
   !tx 0 
MsgWaiting:
   !tx EscC,EscTimeColor, " Waiting:", 0

