
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "../MainMenuCRT/source/c64defs.i"  ;C64 colors, mem locations, etc.
   !src "../MainMenuCRT/source/Menu_Regs.i"  ;IO space registers matching Teensy code

   CINV        = $0314         ; KERNAL IRQ RAM vector
   VICIRQ      = $d019         ; VIC-II interrupt register
   CIA1ICR     = $dc0d         ; CIA #1 interrupt control/status
   KIRQ_EXIT   = $ea81         ; KERNAL: pla/tay/pla/tax/pla/rti
   NMINV       = $0318         ; KERNAL NMI RAM vector (default $FE47)
   CIA2ICR     = $dd0d         ; CIA #2 interrupt control/status

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
   
   ;install IRQ Wedge for IRQ test:
   sei
   lda CINV          ; preserve the current vector so we
   sta smcChainIRQ+1     ; chain to it (self-modifying JMP)
   lda CINV+1
   sta smcChainIRQ+2
   lda #<IRQwedge
   sta CINV
   lda #>IRQwedge
   sta CINV+1
   ;install NMI Wedge for NMI test:
   lda NMINV       ; preserve current vector for chaining
   sta smcChainNMI+1     ; (self-modifying JMP)
   lda NMINV+1
   sta smcChainNMI+2
   lda #<NMIwedge
   sta NMINV
   lda #>NMIwedge
   sta NMINV+1
   cli
   
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



NMIwedge:
   pha             ; KERNAL hasn't saved anything —
   txa             ; save A/X/Y ourselves
   pha
   tya
   pha

   ; CIA #2?  Bit 7 of $DD0D is set if any enabled CIA #2
   ; interrupt fired.  READING CLEARS ALL FLAGS and drops
   ; the CIA's /NMI assertion, so stash the value, if needed.
   lda CIA2ICR
   ;sta nmiflags
   bmi cia2_source

   ; Not CIA #2: the line was pulled by RESTORE or by an
   ; external device — indistinguishable by elimination.
external_or_restore:
   lda #2 ;2==NMI 
   sta wRegIRQNMITest+IO1Port

   ; Fall-through default: treat as RESTORE.  The original
   ; handler pushes A/X/Y itself, so restore ours first to
   ; keep the stack balanced.
   pla
   tay
   pla
   tax
   pla
smcChainNMI:
   jmp $fe47       ; operand rewritten by installer with
                   ; whatever NMINV held before us
 
; ------------------- internal source: CIA #2 --------------------
cia2_source:
   ; We already consumed/acknowledged $DD0D (value is in
   ; nmiflags), so the stock handler's RS-232 servicing
   ; can't run correctly — we just exit.  If you use
   ; KERNAL RS-232, handle nmiflags here instead.
;exit:
   pla
   tay
   pla
   tax
   pla
   rti
            

IRQwedge:
   bit VICIRQ
   bmi IRQinternal    ; VIC caused it -> normal chain

   lda CIA1ICR
   ;sta ciaflags ;Need to save it for use later?
   bmi IRQinternal    ; CIA #1 caused it -> normal chain

   ; Neither internal source claims the interrupt:
   ;    the /IRQ line was pulled by an external device.
   lda #1 ;1==IRQ 
   sta wRegIRQNMITest+IO1Port
   jmp KIRQ_EXIT   ; restore A/X/Y and RTI, skipping the
                   ; KERNAL keyboard/jiffy housekeeping
 
; ------------------- internal source: chain ---------------------
IRQinternal:
smcChainIRQ:
      jmp $ea31       ; operand rewritten by installer with
                            ; whatever CINV held before
 

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


