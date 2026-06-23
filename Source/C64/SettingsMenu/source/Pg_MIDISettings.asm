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


MIDIValColumn = 33   ;Column for MIDI values

MIDIMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgMIDIMenu
   ldy #>MsgMIDIMenu
   jsr PrintString 

ShowMIDIMenuSettings:
   lda TblEscC+EscNameColor
   sta $0286  ;set text color
   
   ;could do this more eligantly since bits are in order in registers...
   ;  see enum RegMIDISettingsMasks, eepAdMIDISettings, rwRegMIDISettings
   
   ldx #5 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetNoteOffOnEn  
   jsr PrintOnOff
   
   ldx #6 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetAfterTouchPolyEn  
   jsr PrintOnOff
   
   ldx #7 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetControlChangeEn  
   jsr PrintOnOff
   
   ldx #8 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetProgramChangeEn  
   jsr PrintOnOff
   
   ldx #9 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetAfterTouchEn  
   jsr PrintOnOff
   
   ldx #10 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetPitchChangeEn  
   jsr PrintOnOff
   
   ldx #11 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetSystemExclusiveEn  
   jsr PrintOnOff
   
   ldx #12 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings+IO1Port
   and #rMIDISetTimeCodeQuarterFrameEn  
   jsr PrintOnOff

;enum RegMIDISettingsMasks2, eepAdMIDISettings2, rwRegMIDISettings2

   ldx #13 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings2+IO1Port
   and #rMIDISet2SongPositionEn  
   jsr PrintOnOff
   
   ldx #14 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings2+IO1Port
   and #rMIDISet2SongSelectEn  
   jsr PrintOnOff
   
   ldx #15 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings2+IO1Port
   and #rMIDISet2TuneRequestEn  
   jsr PrintOnOff
   
   ldx #16 ;row SetNoteOffOn
   ldy #MIDIValColumn ;col
   clc
   jsr SetCursor
   lda rwRegMIDISettings2+IO1Port
   and #rMIDISet2RealTimeSystemEn  
   jsr PrintOnOff
   
WaitMIDIMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitMIDIMenuKey
   
   ;Again, could be done more eligantly with a range check/application, given reg knowledge
   ;+  cmp #'1' ;increment color parameter number 
   ;   bmi +   ;skip if below '1'
   ;   cmp #'1'+NumColorRefs
   ;   bpl +   ;skip if above NumColorRefs
   ;   sec       ;set to subtract without carry
   ;   sbc #'1'   ;make zero based
   ;   tax
   ;   ldy TempTblEscC, x
   ;   iny

+  cmp #'a'  ;Toggle SetNoteOffOn
   bne +
   lda #rMIDISetNoteOffOnEn
-  eor rwRegMIDISettings+IO1Port
   sta rwRegMIDISettings+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowMIDIMenuSettings  

+  cmp #'b'  ;Toggle AfterTouchPoly
   bne +
   lda #rMIDISetAfterTouchPolyEn
   jmp -

+  cmp #'c'  ;Toggle ControlChange
   bne +
   lda #rMIDISetControlChangeEn
   jmp -

+  cmp #'d'  ;Toggle ProgramChange
   bne +
   lda #rMIDISetProgramChangeEn
   jmp -

+  cmp #'e'  ;Toggle 
   bne +
   lda #rMIDISetAfterTouchEn
   jmp -

+  cmp #'f'  ;Toggle 
   bne +
   lda #rMIDISetPitchChangeEn
   jmp -

+  cmp #'g'  ;Toggle 
   bne +
   lda #rMIDISetSystemExclusiveEn
   jmp -

+  cmp #'h'  ;Toggle 
   bne +
   lda #rMIDISetTimeCodeQuarterFrameEn
   jmp -

;2nd midi reg:

+  cmp #'i'  ;Toggle SongPosition
   bne +
   lda #rMIDISet2SongPositionEn
-  eor rwRegMIDISettings2+IO1Port
   sta rwRegMIDISettings2+IO1Port
   jsr WaitForTRWaitMsg
   jmp ShowMIDIMenuSettings  

+  cmp #'j'  ;Toggle 
   bne +
   lda #rMIDISet2SongSelectEn
   jmp -

+  cmp #'k'  ;Toggle 
   bne +
   lda #rMIDISet2TuneRequestEn
   jmp -

+  cmp #'l'  ;Toggle 
   bne +
   lda #rMIDISet2RealTimeSystemEn
   jmp -
   
+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitMIDIMenuKey   
   
MsgMIDIMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Config: MIDI Message Filters ", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor,  " Packet types to transfer:", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Note Off/On        (8x/9x):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "AfterTouch Poly       (Ax):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Control Change        (Bx):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Program Change        (Cx):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "AfterTouch            (Dx):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Pitch Change          (Ex):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "g", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "System Exclusive      (F0):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "h", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "TimeCode QuarterFrame (F1):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "i", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Song Position         (F2):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "j", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Song Select           (F3):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "k", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Tune Request          (F6):", ChrReturn
   !tx "  ", EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "l", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Real Time System   (F8-FF):", ChrReturn, ChrReturn
   !tx EscC,EscArgSpaces+4, "Only types marked \"", EscC,EscNameColor, "On", EscC,EscSourcesColor, "\" will be", ChrReturn
   !tx EscC,EscArgSpaces+5, "passed through to the C64/128"
   !tx 0 
