; MIT License
; 
; Copyright (c) 2025 Travis Smith
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


; This color util is ~900 bytes long, could make it a stand-alone app


ColorConfigMenu:
   jsr PrintBanner ;SourcesColor
   lda #<MsgColorMenu
   ldy #>MsgColorMenu
   jsr PrintString 

   lda #<MsgSettingsMenu2SpaceRet
   ldy #>MsgSettingsMenu2SpaceRet
   jsr PrintString 

   ;copy current colors to local temp storage
   lda #<TblEscC
   ldy #>TblEscC
   jsr CopyColorsToLclTemp

PrintAllColorRef:
   ;print all the current color names from temp buffer:
   ldx #NumColorRefs
-  jsr PrintColorRef
   dex
   bne -

WaitColorMenuKey:
   jsr DisplayTime   
   jsr CheckForIRQGetIn    
   beq WaitColorMenuKey

+  cmp #'1' ;increment color parameter number 
   bmi +   ;skip if below '1'
   cmp #'1'+NumColorRefs
   bpl +   ;skip if above NumColorRefs
   sec       ;set to subtract without carry
   sbc #'1'   ;make zero based
   tax
   ldy TempTblEscC, x
   iny
SavePrintColorUpdate
   tya
   and #$0f ;15 colors max
   sta TempTblEscC, x
   inx ; make 1 based
   jsr PrintColorRef   
   jmp WaitColorMenuKey  

+  cmp #'!' ;decrement color parameter number 
   bmi +   ;skip if below '!' (shift 1)
   cmp #'!'+NumColorRefs
   bpl +   ;skip if above NumColorRefs
   sec       ;set to subtract without carry
   sbc #'!'   ;make zero based
   tax
   ldy TempTblEscC, x
   dey
   jmp SavePrintColorUpdate

+  cmp #'a' ;decrement color parameter number 
   bmi +   ;skip if below 'a' 
   cmp #'a'+NumPresetColors
   bpl +   ;skip if above NumPresetColors
   sec       ;set to subtract without carry
   sbc #'a'   ;make zero based
   asl ;mult by two
   tax
   ldy TblPresets+1, x ;ldy #>Preset_TRDef_TblEscC
   lda TblPresets, x   ;lda #<Preset_TRDef_TblEscC
   jsr CopyColorsToLclTemp
   jmp PrintAllColorRef

+  cmp #ChrReturn  ;Apply Settings
   bne +
   ;copy local temp storage to current colors 
   ldx #NumColorRefs
-  txa
   pha
   lda TempTblEscC-1, x ;zero based offset
   sta TblEscC-1, x
   sta rwRegColorRefStart-1+IO1Port, x ;zero based offset
   jsr WaitForTRWaitMsg  ;must wait for rwRegColorRefStart writes
   pla 
   tax
   dex
   bne -
   jsr ScreenColorOnly ;update screen colors
   jmp ColorConfigMenu ;reprint screen

+  cmp #ChrF4  ;toggle music
   bne +
   jsr ToggleSIDMusic
   jmp WaitColorMenuKey  

+  cmp #ChrF1     ;back to Main Menu
   beq +
   cmp #ChrSpace  ;back to Main Menu
   beq +
   jmp WaitColorMenuKey   
+  rts

PrintColorRef:
   ;print temp color ref from X register (1 based) in correct location
   ; X register (not disturbed) color ref # to print
   ; A and Y are trashed

   ;set cursor position:
   txa
   pha
   clc
   adc #4 ;color start row
   tax    ;row #
   ldy #4 ;col #
   ;clc  ;should be clear from adc
   jsr SetCursor
   pla 
   tax
   
   ;set text color/reverse
   ;ldx smcSavedColorRefNum+1 ;restore color ref num
   ldy TempTblEscC-1, x ;read the color, conv to zero based offset
   cpy TblEscC+EscBackgndColor ;compare color chosen to current background color   
   bne +
   ;color=background: offset print color, leave RVS off
   tya
   dey ;matches background offset char color by 1
   jmp ++
   ;color!=background:  print actual color, RVS on
+  lda #ChrRvsOn
   jsr SendChar
   tya
   
;a: color num, y: text color
++ sty $0286  ;set text color    
   ;print color string from TblColorNames
   asl
   asl
   asl  ;mult by 8
   tay
-  lda TblColorNames, y 
   jsr SendChar
   iny
   tya
   and #7
   bne -
   lda #ChrRvsOff
   jsr SendChar
   rts

CopyColorsToLclTemp:   
   ;coppies color ref table to local temp table
   ;a=table base lo, y=table base high
   ;lda #<Preset_TRDef_TblEscC
   ;ldy #>Preset_TRDef_TblEscC
   sta smcColorTableAddress+1
   sty smcColorTableAddress+2
   ldx #NumColorRefs
-  dex
smcColorTableAddress
   lda $fff0, x ;zero based offset
   sta TempTblEscC, x
   cpx #0
   bne -
   rts
   
TempTblEscC:  ;order matches enum ColorRefOffsets
        ;Temporary local storage for string escape token (EscC) next character cross-reference
        ;Only used in color config screen
        ;Local Default     EEPROM default  Description
   !byte PokeBlack       ; PokeBlack      ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeDrkGrey     ; PokePurple     ;EscBorderColor      = 1 ; Screen Border
   !byte PokeDrkGrey     ; PokePurple     ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeWhite       ; PokeOrange     ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeLtGrey      ; PokeYellow     ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeDrkGrey     ; PokeLtBlue     ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeMedGrey     ; PokeLtGreen    ;EscNameColor        = 6 ; FIle names and information
   
MsgColorMenu:
   !tx ChrReturn, EscC,EscSourcesColor,  "Color Settings Page:", ChrReturn, ChrReturn
   !tx EscC,EscNameColor,  "Individual colors:", EscC,EscOptionColor, " (up/down)", ChrReturn

   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "1!", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "Screen Background", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "2", ChrQuote, ChrQuote, ChrCRSRLeft, ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "Screen Border", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "3#", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "Top of screen banner color", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "4$", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "Time Display & Waiting msg", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "5%", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "Input key option indicator", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "6&", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "General text/descriptions", ChrReturn
   !tx EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "7'", ChrRvsOff, ChrFillLeft, EscC,EscArgSpaces+9, EscC,EscSourcesColor, "File names & headings", ChrReturn
   !tx ChrReturn

   !tx EscC,EscNameColor,  " Presets:", ChrReturn
   !tx EscC,EscArgSpaces+2,   EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "a", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "TR Default"
   !tx EscC,EscArgSpaces+2+3, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "d", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "CGA"
   !tx ChrReturn
   
   !tx EscC,EscArgSpaces+2,   EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "b", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Black & White"
   !tx EscC,EscArgSpaces+2,   EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "e", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "The Blues"
   !tx ChrReturn

   !tx EscC,EscArgSpaces+2,   EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "c", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "C64 Mono"
   !tx EscC,EscArgSpaces+2+5, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "f", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Rainbow"
   !tx ChrReturn, ChrReturn
   
   !tx EscC,EscNameColor,  " General:", ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "Return", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor, "Apply Selected Colors", ChrReturn
   !tx ChrReturn
   !tx 0
   
TblColorNames:
   ;8 characters each, no termination
   !tx " Black  "  ; = 0 ,
   !tx " White  "  ; = 1 ,
   !tx "  Red   "  ; = 2 ,
   !tx "  Cyan  "  ; = 3 ,
   !tx " Purple "  ; = 4 ,
   !tx " Green  "  ; = 5 ,
   !tx "  Blue  "  ; = 6 ,
   !tx " Yellow "  ; = 7 ,
   !tx " Orange "  ; = 8 ,
   !tx " Brown  "  ; = 9 ,
   !tx " Lt Red "  ; = 10,
   !tx "Drk Grey"  ; = 11,
   !tx "Med Grey"  ; = 12,
   !tx "Lt Green"  ; = 13,
   !tx "Lt Blue "  ; = 14,
   !tx "Lt Grey "  ; = 15,

NumPresetColors = 6
TblPresets:
   !word Preset_TRDef_TblEscC    ;'a'
   !word Preset_BnW_TblEscC      ;'b'
   !word Preset_C64_TblEscC      ;'c'
   !word Preset_CGA_TblEscC      ;'d'
   !word Preset_Blues_TblEscC    ;'e'
   !word Preset_Rainbow_TblEscC  ;'f'
   
Preset_TRDef_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;TR Defaults
   !byte PokeBlack       ;EscBackgndColor     = 0 ; Screen Background
   !byte PokePurple      ;EscBorderColor      = 1 ; Screen Border
   !byte PokePurple      ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeOrange      ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeYellow      ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeLtBlue      ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeLtGreen     ;EscNameColor        = 6 ; FIle names and information
   
Preset_BnW_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;Black & White
   !byte PokeBlack       ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeDrkGrey     ;EscBorderColor      = 1 ; Screen Border
   !byte PokeDrkGrey     ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeWhite       ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeLtGrey      ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeDrkGrey     ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeMedGrey     ;EscNameColor        = 6 ; FIle names and information

Preset_C64_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;C64 colors
   !byte PokeBlue        ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeLtBlue      ;EscBorderColor      = 1 ; Screen Border
   !byte PokeLtBlue      ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeLtBlue      ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeLtBlue      ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeLtBlue      ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeLtBlue      ;EscNameColor        = 6 ; FIle names and information

Preset_CGA_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;CGA colors
   !byte PokeBlack       ;EscBackgndColor     = 0 ; Screen Background
   !byte PokePurple      ;EscBorderColor      = 1 ; Screen Border
   !byte PokeCyan        ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeLtGrey      ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeCyan        ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeCyan        ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeWhite       ;EscNameColor        = 6 ; FIle names and information

Preset_Blues_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;Custom blues & green on black
   !byte PokeBlack       ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeBlack       ;EscBorderColor      = 1 ; Screen Border
   !byte PokeBlue        ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeCyan        ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeBlue        ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeGreen       ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokeLtBlue      ;EscNameColor        = 6 ; FIle names and information

Preset_Rainbow_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;Custom blues & green on black
   !byte PokeWhite       ;EscBackgndColor     = 0 ; Screen Background
   !byte PokeGreen       ;EscBorderColor      = 1 ; Screen Border
   !byte PokeGreen       ;EscTRBannerColor    = 2 ; Top of screen banner color
   !byte PokeOrange      ;EscTimeColor        = 3 ; Time Display & Waiting msg
   !byte PokeBlue        ;EscOptionColor      = 4 ; Input key option indication
   !byte PokeRed         ;EscSourcesColor     = 5 ; General text/descriptions
   !byte PokePurple      ;EscNameColor        = 6 ; FIle names and information

;Preset_C128_TblEscC:  ;matches enum ColorRefOffsets, NumColorRefs long
;;C128 colors
;   !byte PokeDrkGrey     ;EscBackgndColor     = 0 ; Screen Background
;   !byte PokeLtGreen     ;EscBorderColor      = 1 ; Screen Border
;   !byte PokeLtGreen     ;EscTRBannerColor    = 2 ; Top of screen banner color
;   !byte PokeLtGreen     ;EscTimeColor        = 3 ; Time Display & Waiting msg
;   !byte PokeLtGreen     ;EscOptionColor      = 4 ; Input key option indication
;   !byte PokeLtGreen     ;EscSourcesColor     = 5 ; General text/descriptions
;   !byte PokeLtGreen     ;EscNameColor        = 6 ; FIle names and information

