
   ;VIC registers
   BorderColorReg  = $d020 
   BackgndColorReg = $d021
  
   ;Kernal routines:
   SendChar        = $ffd2
   ScanKey         = $ff9f ;SCNKEY
   GetIn           = $ffe4 ;GETIN
   SetCursor       = $fff0 ;PLOT
   
   ;BASIC routines:
   PrintString     = $ab1e

   ;chr$ symbols
   ChrBlack        = 144
   ChrWhite        = 5
   ChrRed          = 28
   ChrCyan         = 159
   ChrPurple       = 156
   ChrGreen        = 30
   ChrBlue         = 31
   ChrYellow       = 158 
   ChrOrange       = 129
   ChrBrown        = 149
   ChrLtRed        = 150
   ChrDrkGrey      = 151
   ChrMedGrey      = 152
   ChrLtGreen      = 153
   ChrLtBlue       = 154
   ChrLtGrey       = 155
                   
   ChrF1           = 133
   ChrF2           = 137
   ChrF3           = 134
   ChrF4           = 138
   ChrF5           = 135
   ChrF6           = 139
   ChrF7           = 136
   ChrF8           = 140
   ChrToLower      = 14
   ChrToUpper      = 142
   ChrRvsOn        = 18
   ChrRvsOff       = 146
   ChrClear        = 147
   ChrReturn       = 13
   ChrShiftReturn  = 141
   ChrQuestionMark = 63
   ChrSpace        = 32
   ChrCRSRUp       = 145
   ChrCRSRDn       = 17
   ChrCRSRLeft     = 157
   ChrCRSRRight    = 29
   ChrUpArrow      = 94
   ChrLeftArrow    = 95
   ChrHome         = 19
   ChrFillLeft     = 181
   ChrFillRight    = 182
   ChrRun          = 131
   ChrStop         = 3
   ChrQuote        = 34
                   
;Poke colors       
   PokeBlack       = 0
   PokeWhite       = 1
   PokeRed         = 2
   PokeCyan        = 3
   PokePurple      = 4
   PokeGreen       = 5
   PokeBlue        = 6
   PokeYellow      = 7
   PokeOrange      = 8
   PokeBrown       = 9
   PokeLtRed       = 10
   PokeDrkGrey     = 11
   PokeMedGrey     = 12
   PokeLtGreen     = 13
   PokeLtBlue      = 14
   PokeLtGrey      = 15


