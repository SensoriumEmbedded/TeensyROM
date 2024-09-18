#importonce

.namespace vic {
  // The 47 VIC-II registers
  .label SP0X   = $d000
  .label SP0Y   = $d001
  .label SP1X   = $d002
  .label SP1Y   = $d003
  .label SP2X   = $d004
  .label SP2Y   = $d005
  .label SP3X   = $d006
  .label SP3Y   = $d007
  .label SP4X   = $d008
  .label SP4Y   = $d009
  .label SP5X   = $d00a
  .label SP5Y   = $d00b
  .label SP6X   = $d00c
  .label SP6Y   = $d00d
  .label SP7X   = $d00e
  .label SP7Y   = $d00f
  .label MSIGX  = $d010 // MSB of sprite x positions. 1 bit per sprite
  .label SCROLY = $d011
  .label RASTER = $d012
  .label LPENX  = $d013
  .label LPENY  = $d014
  .label SPENA  = $d015 // Sprite enable
  .label SCROLX = $d016
  .label YXPAND = $d017
  .label VMCSB  = $d018
  .label VICIRQ = $d019
  .label IRQMSK = $d01a
  .label SPBGPR = $d01b
  .label SPMC   = $d01c
  .label XXPAND = $d01d
  .label SPSPCL = $d01e
  .label SPBGCL = $d01f
  .label EXTCOL = $d020
  .label BGCOL0 = $d021
  .label BGCOL1 = $d022
  .label BGCOL2 = $d023
  .label BGCOL3 = $d024
  .label SPMC0  = $d025
  .label SPMC1  = $d026
  .label SP0COL = $d027
  .label SP1COL = $d028
  .label SP2COL = $d029
  .label SP3COL = $d02a
  .label SP4COL = $d02b
  .label SP5COL = $d02c
  .label SP6COL = $d02d
  .label SP7COL = $d02e

// These are not in the VIC address space. Move them to the CIA namespace?
  .label CLRRAM = $d800
  .label COLCLK = $d81a
  .label TODTN1 = $dc08
  .label CIAICR = $dc0d
  .label TODTN2 = $dd08
  .label CI2ICR = $dd0d

  .label BLACK    = $00
  .label WHITE    = $01
  .label RED      = $02
  .label CYAN     = $03
  .label PURPLE   = $04
  .label VIOLET   = $04
  .label GREEN    = $05
  .label BLUE     = $06
  .label YELLOW   = $07
  .label ORANGE   = $08
  .label BROWN    = $09
  .label LT_RED   = $0a
  .label DK_GRAY  = $0b
  .label MED_GRAY = $0c
  .label LT_GREEN = $0d
  .label LT_BLUE  = $0e
  .label LT_GRAY  = $0f
}
