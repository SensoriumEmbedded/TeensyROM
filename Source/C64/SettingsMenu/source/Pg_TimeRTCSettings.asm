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



TimeRTCMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgTimeRTCMenu
   ldy #>MsgTimeRTCMenu
   jsr PrintString 

ShowTimeRTCSettings:
   lda TblEscC+EscNameColor
   sta $0286  ;set text color

   ldx #5  ;row 12/24 hour clock
   ldy #25 ;col
   clc
   jsr SetCursor
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr
   beq + ;branch if 12 hour
   lda #'2'
   jsr SendChar   
   lda #'4'
   jmp ++
+  lda #'1'
   jsr SendChar
   lda #'2'
++ jsr SendChar

   ldx #6 ;row Time Zone
   ldy #28 ;col
   clc
   jsr SetCursor
   ldx #'+'
   ldy rwRegTimezone+IO1Port
   tya
   and #$80
   beq +
   ;neg number
   tya
   eor#$ff  ;1's comp
   tay
   iny ;2's comp
   ldx #'-' 
   ;x=sign char, y=abs val
+  txa
   jsr SendChar
   tya
   ror  ;divide by two, carry bit holds half hour
   php  ;save carry bit on stack
   jsr PrintIntByte
   plp
   bcc++
   lda #'.'
   jsr SendChar   
   lda #'5'
   jsr SendChar   
++ lda #' '
   jsr SendChar
   lda #' '
   jsr SendChar
  
   
WaitTimeRTCMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitTimeRTCMenuKey

+  cmp #'a'  ;12/24 hour clock
   bne +
   lda rwRegPwrUpDefaults+IO1Port
   eor #rpudClock12_24hr  
   sta rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr  
   sta smc24HourClockDisp+1 ;24(non-zero) vs 12(zero) hr display 
   jsr WaitForTRWaitMsg
   jmp ShowTimeRTCSettings  

+  cmp #'b'  ;Power-up Time Zone Increment
   bne +
   ldx rwRegTimezone+IO1Port
   inx
   ;tz range is -12 to +14 from UTC (x2)
   cpx #29
   bne UpdTimeZone
   ldx #-24
   jmp UpdTimeZone

+  cmp #'B'  ;Power-up Time Zone Decrement
   bne +
   ldx rwRegTimezone+IO1Port
   dex
   cpx #-25
   bne UpdTimeZone
   ldx #28
UpdTimeZone
   stx rwRegTimezone+IO1Port
   jsr WaitForTRWaitMsg
   jsr SetC64TODfromRTC  ;update/resynch time with RTC to include updated time zone
   jmp ShowTimeRTCSettings  

+  cmp #'c'  ;Synch Time now
   bne +
   jsr PrintBanner ;SourcesColor
   lda TblEscC+EscSourcesColor
   sta $0286  ;set text color
   jsr SetRTCfromEthernet
   jsr SetC64TODfromRTC
   jsr AnyKeyMsgWait ; For looking at messages/IP address
   jmp TimeRTCMenu ;force to reprint all 
   
+  cmp #'D'  ;RTC Hours Increment
   bne +
   lda #rCtlRTCAdj_Hrs_Up_WAIT
DoRTCAdjust
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg   
   jsr SetC64TODfromRTC_Preloaded ;rsRTCAdjust also loads time regs   
   jmp WaitTimeRTCMenuKey  

+  cmp #'d'  ;RTC Hours Decrement
   bne +
   lda #rCtlRTCAdj_Hrs_Dn_WAIT
   jmp DoRTCAdjust

+  cmp #'E'  ;RTC Min Increment
   bne +
   lda #rCtlRTCAdj_Min_Up_WAIT
   jmp DoRTCAdjust

+  cmp #'e'  ;RTC Min Decrement
   bne +
   lda #rCtlRTCAdj_Min_Dn_WAIT
   jmp DoRTCAdjust

+  cmp #'F'  ;RTC Sec Increment
   bne +
   lda #rCtlRTCAdj_Sec_Up_WAIT
   jmp DoRTCAdjust

+  cmp #'f'  ;RTC Sec Decrement
   bne +
   lda #rCtlRTCAdj_Sec_Dn_WAIT
   jmp DoRTCAdjust

   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitTimeRTCMenuKey   
   
MsgTimeRTCMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: Time Format/Real Time Clock ", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor,  " Format/Location:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "12/24 hour clock:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b/B", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, " Local Time Zone:", EscC,EscNameColor, " UTC", ChrReturn
   
   !tx ChrReturn, EscC,EscTimeColor,  " RTC Adjustment:", ChrReturn
   !tx EscC,EscArgSpaces+4, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Synch RTC via Ethernet now", ChrReturn   
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d/D", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RTC Hours   Down/Up", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e/E", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RTC Minutes Down/Up", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f/F", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "RTC Seconds Down/Up", ChrReturn, ChrReturn, ChrReturn
 
   !tx 0 
