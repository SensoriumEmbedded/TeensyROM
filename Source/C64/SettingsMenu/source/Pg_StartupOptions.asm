; MIT License
; 
; Copyright (c) 2026 Travis Smith
; 
; Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
; and associated documentation files (the "Software"), to deal in the Software without 
; restriction, including without limitation the rights to use, copy, modify, merge, publish, 
; distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
; the Software is furnished to do so, subject to the following conditions:
; 
; The above copyright notice and this permission notice shall be included in all copies or 
; substantial portions of the Software.
; 
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
; BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
; NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
; DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


StartupValColumn = 26   ;Column for TR startup values

StartupOptionsMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgStartupOptionsMenu
   ldy #>MsgStartupOptionsMenu
   jsr PrintString 

   ;print startup SID filename
   lda #rCtlMakeSIDStrWAIT
   ldx #7 ;row
   ldy #2 ;col
   jsr PrintFileName
  
   ;print listening IP address/port
   
   ;print Autolaunch filename
   lda #rCtlMakeAutoLStrWAIT
   ldx #17 ;row
   ldy #2 ;col
   jsr PrintFileName
 
ShowStartupOptionsSettings:
   lda TblEscC+EscNameColor
   sta $0286  ;set text color
   
   ldx #5 ;row Play SID
   ldy #StartupValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudSIDPauseMask  
   eor #rpudSIDPauseMask  
   jsr PrintOnOff

   ldx #9 ;row Synch Time
   ldy #StartupValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask  
   jsr PrintOnOff

   ldx #10 ;row TCP Listen 
   ldy #StartupValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2TRTCPListen  
   jsr PrintOnOff

   ;lda #rCtlMakeEthLocalIPWAIT
   ;ldx #11 ;row
   ;ldy #14 ;col
   ;jsr PrintFileName
   ;lda #<MsgPort2112
   ;ldy #>MsgPort2112
   ;jsr PrintString 

   ldx #15 ;Auto-Launch Enabled 
   ldy #StartupValColumn ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults2+IO1Port
   and #rpud2TRAutoLaunch  
   jsr PrintOnOff

   
WaitStartupOptionsMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitStartupOptionsMenuKey

+  cmp #'a'  ;Power-up Music State toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudSIDPauseMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowStartupOptionsSettings  

+  cmp #'b'  ;Power-up Synch Time toggle
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudNetTimeMask  
   sta rwRegPwrUpDefaults+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowStartupOptionsSettings  

+  cmp #'c'  ;Toggle TCP Listener
   bne +
   lda rwRegPwrUpDefaults2+IO1Port
   eor #rpud2TRTCPListen  
   sta rwRegPwrUpDefaults2+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowStartupOptionsSettings  
   
+  cmp #'d'  ;Toggle Auto-Launch Enable
   bne +
   lda rwRegPwrUpDefaults2+IO1Port
   eor #rpud2TRAutoLaunch  
   sta rwRegPwrUpDefaults2+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowStartupOptionsSettings  
   
   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitStartupOptionsMenuKey   
   
MsgStartupOptionsMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Startup Options ", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " On Main Menu Startup:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Play selected SID:", ChrReturn
   !tx EscC,EscSourcesColor,  "  SID file: ('S' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "  Synch RTC via net:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Enable TCP Listener:", ChrReturn
;   !tx EscC,EscSourcesColor,  "     IP/Port:", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscArgSpaces+5, "(port 2112 of current IP)", ChrReturn, ChrReturn, ChrReturn
   !tx EscC,EscTimeColor,  " On TeensyROM Boot/Power-up:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Auto-Launch Enable:", ChrReturn
   !tx EscC,EscSourcesColor,  "  Auto-Launch file: ('A' to sel)", ChrReturn, ChrReturn, ChrReturn
   !tx 0 

;MsgPort2112:
;   !tx ":2112"
;   !tx 0 
