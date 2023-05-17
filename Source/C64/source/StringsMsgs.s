; MIT License
; 
; Copyright (c) 2023 Travis Smith
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

PrintString:
   sta PtrAddrLo
   sty PtrAddrHi
   ldy #0
-  lda (PtrAddrLo), y
   beq +
   jsr SendChar
   iny
   bne -
+  rts

DisplayTime:
   ldx #1 ;row
   ldy #29  ;col
   clc
   jsr SetCursor
   lda #TimeColor
   jsr SendChar
   lda TODHoursBCD ;latches time in regs (stops incrementing)
   tay ;save for re-use
   and #$1f
   bne nz   ;if hours is 0, make it 12...
   tya
   ora #$12
   tay ;re-save for re-use
nz tya
   and #$10
   bne +
   lda #ChrSpace
   jmp ++
+  lda #'1'
++ jsr SendChar
   tya
   and #$0f  ;ones of hours
   jsr PrintHexNibble
   lda #':'
   jsr SendChar
   lda TODMinBCD
   jsr PrintHexByte
   lda #':'
   jsr SendChar
   lda TODSecBCD
   jsr PrintHexByte
   ;lda #'.'
   ;jsr SendChar
   lda TODTenthSecBCD ;have to read 10ths to release latch
   ;jsr PrintHexNibble
   tya ;am/pm (pre latch release)
   and #$80
   bne +
   lda #'a'
   jmp ++
+  lda #'p'
++ jsr SendChar
   lda #'m'
   jsr SendChar
   rts
   
PrintHexByte:
   ;Print byte value stored in acc in hex (2 chars)
   pha
   lsr
   lsr
   lsr
   lsr
   jsr PrintHexNibble
   pla
   ;pha   ; preserve acc on return?
   jsr PrintHexNibble
   ;pla
   rts
   
PrintHexNibble:   
   ;Print value stored in lower nible acc in hex
   ;trashes acc
   and #$0f
   cmp #$0a
   bpl l 
   clc
   adc #'0'
   jmp pr
l  clc
   adc #'a'-$0a
pr jsr SendChar
   rts

PrintOnOff:
   ;Print "On" or "Off" based on Zero flag
   ;uses A and Y regs
   bne +
   lda #<MsgOff
   ldy #>MsgOff
   jmp ++
+  lda #<MsgOn
   ldy #>MsgOn
++ jsr PrintString 
   rts

Print4CharTableHiNib
   lsr
   lsr
   lsr
   lsr ; move to lower nibble
Print4CharTable:   
;prints 4 chars from a table of continuous 4 char sets (no termination)
;X=table base lo, y=table base high, acc=index to item# (63 max)
;   and #0xfc 
   stx PtrAddrLo
   sty PtrAddrHi
   asl
   asl  ;mult by 4
   tay
-  lda (PtrAddrLo),y
   jsr SendChar   ;type (4 chars)
   iny
   tya
   and #3
   bne -
   rts
   
   
; ******************************* Strings/Messages ******************************* 
___Strings_Mesages_____________________________:

MsgBanner:    
   !tx ChrClear, ChrToLower, ChrPurple, ChrRvsOn, "             TeensyROM v0.2             ", ChrRvsOff, 0
MsgFrom:    
   !tx ChrReturn, SourcesColor, "From ", 0 
MsgSelect:
   !tx SourcesColor, "Sources:          "
   !tx ChrRvsOn, OptionColor, "Up", ChrRvsOff, MenuMiscColor, "/", ChrRvsOn, OptionColor, "Dn", ChrRvsOff, MenuMiscColor, "CRSR: Page", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F1", ChrRvsOff, SourcesColor,  " Teensy Mem   "
   !tx " ", ChrRvsOn, OptionColor, "F2", ChrRvsOff, MenuMiscColor, " Exit to BASIC", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F3", ChrRvsOff, SourcesColor,  " SD Card      "
   !tx " ", ChrRvsOn, OptionColor, "F4", ChrRvsOff, MenuMiscColor, " Music on/off", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F5", ChrRvsOff, SourcesColor,  " USB Drive    "
   !tx " ", ChrRvsOn, OptionColor, "F6", ChrRvsOff, MenuMiscColor, " Settings", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F7", ChrRvsOff, SourcesColor,  " USB Host     "
   !tx " ", ChrRvsOn, OptionColor, "F8", ChrRvsOff, MenuMiscColor, " MIDI to SID"
    !tx 0
MsgNoHW:
   !tx ChrReturn, ChrReturn, ChrToLower, ChrYellow, "TeensyROM hardware not detected!!!", ChrReturn, 0
MsgNoItems:
   !tx ChrReturn, OptionColor, " Nothing to show!", 0

MsgMenuSD:
   !tx "SD Card:", 0
MsgMenuTeensy:
   !tx "Teensy Mem:", 0
MsgMenuUSBHost:
   !tx "USB Host:", 0
MsgMenuUSBDrive:
   !tx "USB Drive:", 0

MsgSettingsMenu:
   !tx ChrReturn, SourcesColor, "Settings Menu:", ChrReturn
   !tx ChrReturn, OptionColor 
   !tx "    Power-up Defaults", ChrReturn
   !tx "       ", ChrRvsOn, OptionColor, "F1", ChrRvsOff, SourcesColor, "  Synch Time:", ChrReturn
   !tx "       ", ChrRvsOn, OptionColor, "F3", ChrRvsOff, SourcesColor, " Music State:", ChrReturn
   !tx "       ", ChrRvsOn, OptionColor, "F5", ChrRvsOff, SourcesColor, "   Time Zone:", NameColor, " UTC", ChrReturn
   !tx ChrReturn, OptionColor 
   !tx "    Immediate", ChrReturn
   !tx "       ", ChrRvsOn, OptionColor, "F2", ChrRvsOff, SourcesColor, " Synch Time via Ethernet", ChrReturn
   !tx "       ", ChrRvsOn, OptionColor, "F4", ChrRvsOff, SourcesColor, " Toggle Music On/Off", ChrReturn, ChrReturn
   !tx 0  ;near max of 256 bytes

MsgCreditsInfo:
   !tx "    ", ChrRvsOn, OptionColor, "F6", ChrRvsOff, SourcesColor,  " Back to Main menu", ChrReturn
   !tx "    ", ChrRvsOn, OptionColor, "F7", ChrRvsOff, SourcesColor,  " Special IO:", ChrReturn
   !tx "    ", ChrRvsOn, OptionColor, "F8", ChrRvsOff, SourcesColor,  " Self Test", ChrReturn
   !tx ChrReturn, ChrReturn, MenuMiscColor 
   !tx "   2023 by Travis Smith @ Sensorium", ChrReturn, ChrReturn
   !tx " TeensyROM is 100% Open Source HW & SW!", ChrReturn
   !tx " github.com/SensoriumEmbedded/TeensyROM", ChrReturn, ChrReturn
   !tx "           Music by Frank Zappa", ChrReturn
   !tx 0

MsgM2SPolyMenu:    
   !tx ChrReturn, ChrReturn, SourcesColor, "MIDI to SID Polyphonic Mode"
   !tx ChrReturn, ChrReturn, OptionColor 
   !tx "   ", ChrRvsOn, "T", ChrRvsOff, "riangle:", ChrReturn
   !tx " Sa", ChrRvsOn, "W", ChrRvsOff, "tooth:", ChrReturn
   !tx "   ", ChrRvsOn, "P", ChrRvsOff, "ulse:", ChrReturn
   !tx "  D", ChrRvsOn, "U", ChrRvsOff, "ty Cycle:", ChrReturn
   !tx "   ", ChrRvsOn, "N", ChrRvsOff, "oise:", ChrReturn
   !tx ChrReturn
   !tx "   ", ChrRvsOn, "A", ChrRvsOff, "ttack:", ChrReturn
   !tx "   ", ChrRvsOn, "D", ChrRvsOff, "ecay:", ChrReturn
   !tx "   ", ChrRvsOn, "S", ChrRvsOff, "ustain:", ChrReturn
   !tx "   ", ChrRvsOn, "R", ChrRvsOff, "elease:", ChrReturn
   !tx ChrReturn
   !tx "  E", ChrRvsOn, "x", ChrRvsOff, "it", ChrReturn
   !tx ChrReturn
   !tx "  Now Playing:", ChrReturn
   !tx "   V1  V2  V3  X", ChrReturn
   !tx 0
TblM2SAttack:  ;4 bytes each, no term
   !tx "  2m","  8m"," 16m"," 24m"," 38m"," 56m"," 68m"," 80m"
   !tx "100m","250m","500m","800m","   1","   3","   5","   8"
TblM2SDecayRelease:  ;4 bytes each, no term
   !tx "  6m"," 24m"," 48m"," 72m","114m","168m","204m","240m"
   !tx "300m","750m"," 1.5"," 2.4","   3","   9","  15","  24"
TblM2SSustPct:  ;4 bytes each, no term
   !tx " 0.0"," 6.7","13.3","20.0","26.7","33.3","40.0","46.7"
   !tx "53.3","60.0","66.7","73.3","80.0","86.7","93.3"," 100"
TblM2SDutyPct:  ;4 bytes each, no term
   !tx " 0.0"," 6.3","12.5","18.8","25.0","31.3","37.5","43.8"
   !tx "50.0","56.3","62.5","68.8","75.0","81.3","87.5","93.8"
MsgOn:
   !tx "On ", 0
MsgOff:
   !tx "Off", 0
MsgLoading:
   !tx ChrClear, ChrYellow, ChrToUpper, "loading: ", 0
MsgWaiting:
   !tx TimeColor, " Waiting:", 0
MsgError:
   !tx ChrRed, "Error: ", 0
MsgErrNoData:
   !tx "No Data Available", 0
MsgTesting:
   !tx NameColor, "  Test", 0
MsgPass:
   !tx "Pass  ", 0
MsgFail:
   !tx ChrRed, "Fail  ", 0
;MsgErrNoFile:
;   !tx "No File Available", 0
   
TblItemType: ;must match rtNone, rt16k, etc order!
   !tx "--- ","16k ","8Hi ","8Lo ","Prg ","Unk ","Crt ","Dir ","C128" ;4 bytes each, no term
TblSpecialIO: ;must match enum IO1Handlers order/qty
   !word SpIO_None
   !word SpIO_MIDI_Datel
   !word SpIO_MIDI_Sequential
   !word SpIO_MIDI_Passport
   !word SpIO_MIDI_NamesoftIRQ
   !word SpIO_Debug
   !word SpIO_TeensyROM
   !word SpIO_SwiftLink

SpIO_None:
   !tx "None              ", 0
SpIO_MIDI_Datel:
   !tx "MIDI:Datel/Siel   ", 0
SpIO_MIDI_Sequential:
   !tx "MIDI:Sequential   ", 0
SpIO_MIDI_Passport:
   !tx "MIDI:Passport/Sent", 0
SpIO_MIDI_NamesoftIRQ:
   !tx "MIDI:Namesoft IRQ ", 0
SpIO_Debug:
   !tx "Debug             ", 0
SpIO_TeensyROM:
   !tx "TeensyROM         ", 0
SpIO_SwiftLink:
   !tx "SwiftLink         ", 0
   
   
