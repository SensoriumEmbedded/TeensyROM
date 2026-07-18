
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "../MainMenuCRT/source/c64defs.i"  ;C64 colors, mem locations, etc.
   !src "../MainMenuCRT/source/Menu_Regs.i"  ;IO space registers matching Teensy code

   ;!set DbgOffline = 1   ;if defined, skips all waits/dependancies on TR HW


   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2026                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of SysAddress
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of SysAddress
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of SysAddress
   !byte SysAddress%10+$30         ; Calculate 4th digit of SysAddress
   !byte $00                       ; BASIC end of line marker
BasicEnd    
   !word $0000                     ; BASIC end of program

   !src "source/SupportFunctions.asm"
   !src "source/StringFunctions.asm"

SysAddress:

;screen setup:     
!ifndef DbgOffline {
   ;copy colors from IO1 to local RAM
   ldx #NumColorRefs
-  lda rwRegColorRefStart-1+IO1Port, x ;zero based offset
   sta TblEscC-1, x
   dex
   bne -
}  
   jsr ScreenColorOnly ;update screen colors now that we have them

   ;store default register for 12/24 hour time display locally
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr
   sta smc24HourClockDisp+1  ;local copy of 12/24 hr
   
   ;jsr SetRTCfromEthernet  ; establishes net connection early
   jsr SetC64TODfromRTC   ;do this even if not part of test
   jsr DisplayTime ;do it once here
   
StartFromBegining:
   jsr PrintBanner
   
; -----------  Start of test  -----------
StartTest:
   lda #<MsgExpansionPortTest
   ldy #>MsgExpansionPortTest
   jsr PrintString 
   
   ;Do DMA test
   lda #rCtlExpPortDMAWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots   

   lda rwRegScratch+IO1Port  ; 0=fail, 1=pass
   beq Failed   ;halt on error
   
   ;
   
   

   
   lda #<MsgPassed
   ldy #>MsgPassed
   jsr PrintString

smcLoopOnPass  
   lda #00
   bne StartTest ;debug/looping  
   jmp +
   
Failed:
   lda #<MsgFailed
   ldy #>MsgFailed
   jsr PrintString 
   
+  lda #<MsgAnyKey
   ldy #>MsgAnyKey
   jsr PrintString 
   
-  jsr GetIn    
   bne -    ;clear the kbd queue in case of accidental press
-  jsr DisplayTime  
   jsr GetIn   
   beq -
   
   cmp #'L' ;Cap-L after first pass to Loop forever
   bne +
   lda #01
   sta smcLoopOnPass+1
   jmp StartFromBegining
   
   ;any other key, return to main menu
+  lda #rCtlReturnToMainMenu 
   sta wRegControl+IO1Port
   ;C64 will be reset...
-  jmp -

MsgExpansionPortTest:
   !tx ChrReturn, EscC,EscSourcesColor, ChrRvsOn, " C64/TeensyROM Expansion Port Test ", ChrReturn
   !tx EscC, EscNameColor
   !tx 0 
   
MsgPassed:
   !tx ChrReturn, ChrReturn, EscC,EscSourcesColor, ChrRvsOn, " Expansion Port Test Passed.", ChrReturn, ChrReturn
;   !tx EscC,EscOptionColor, "  Verify the following:", ChrReturn
;   !tx "   * Ethernet Port Green LED is on", ChrReturn
;   !tx "   * TR main LED (by Menu button) is on", ChrReturn
;   !tx "   * Serial output \"Hello from TR at..\"", ChrReturn
;   !tx "   * Check time shown above.", ChrReturn
;   !tx "   * RTC batt: cycle power & recheck", ChrReturn
   !tx 0 

MsgFailed:
   !tx ChrReturn, EscC,EscOptionColor, ChrRvsOn, " Expansion Port Test Failed!", ChrReturn
   !tx 0 

MsgAnyKey:
   !tx ChrReturn, EscC,EscOptionColor, "Press any key to return", ChrReturn
   !tx 0

   
EndOfCode:
   !byte $00 ;byte to mark end address in build report
   ;Don't exceed $3fxx as DMA test writes over that page (and C0xx too)


