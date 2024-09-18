#importonce

.namespace kernal {
  .label LSTX   = $c5
  .label CINV   = $0314
  .label IRQVEC = $0314

  .label CBINV  = $0316
  .label NMINV  = $0318

  // KERNAL vectors
  .label IOPEN  = $031a
  .label ICLOSE = $031c
  .label ICHKIN = $031e
  .label ICKOUT = $0320
  .label ICLRCH = $0322
  .label IBASIN = $0324
  .label IBSOUT = $0326
  .label ISTOP  = $0328
  .label IGETIN = $032a
  .label ICLALL = $032c
  .label USRCMD = $032e
  .label ILOAD  = $0330
  .label ISAVE  = $0332

  .label FREE1  = $0334 // 8 free bytes (0334 - 033b)
  .label TBUFFER= $033c // cassette buffer (033c - 03fb)
  .label FREE2  = $03fc // 4 free bytes (03fc - 03ff)

  .label POLY1  = $e043
  .label POLY2  = $e059
  .label RMULC  = $e08d
  .label RADDC  = $e092
  .label RND    = $e097
  .label SYS    = $e12a
  .label SAVE   = $e156
  .label VERIFY = $e165
  .label LOAD   = $e168
  .label OPEN   = $e1be
  .label CLOSE  = $e1c7

  .label CLRSCR = $e544
  .label HOME   = $e566

  .label IRQNOR = $ea31

  // KERNAL jump Table
  .label VEC_CINT   = $ff81
  .label VEC_IOINIT = $ff84
  .label VEC_RAMTAS = $ff87
  .label VEC_RESTOR = $ff8a
  .label VEC_VECTOR = $ff8d
  .label VEC_SETMSG = $ff90
  .label VEC_SECOND = $ff93
  .label VEC_TKSA   = $ff96
  .label VEC_MEMBOT = $ff99
  .label VEC_MEMTOP = $ff9c
  .label VEC_SCNKEY = $ff9f
  .label VEC_SETTMO = $ffa2
  .label VEC_ACPTR  = $ffa5
  .label VEC_CIOUT  = $ffa8
  .label VEC_UNTLK  = $ffab
  .label VEC_UNLSN  = $ffae
  .label VEC_LISTEN = $ffb1
  .label VEC_TALK   = $ffb4
  .label VEC_READST = $ffb7
  .label VEC_SETLFS = $ffba
  .label VEC_SETNAM = $ffbd
  .label VEC_OPEN   = $ffc0
  .label VEC_CLOSE  = $ffc3
  .label VEC_CHKIN  = $ffc6
  .label VEC_CHKOUT = $ffc9
  .label VEC_CLRCHN = $ffcc
  .label VEC_CHRIN  = $ffcf
  .label VEC_CHROUT = $ffd2 // https://www.c64-wiki.com/wiki/CHROUT
  .label VEC_LOAD   = $ffd5
  .label VEC_SAVE   = $ffd8
  .label VEC_SETTIM = $ffdb
  .label VEC_RDTIM  = $ffde
  .label VEC_STOP   = $ffe1
  .label VEC_GETIN  = $ffe4
  .label VEC_CLALL  = $ffe7
  .label VEC_UDTIM  = $ffea
  .label VEC_SCREEN = $ffed
  .label VEC_PLOT   = $fff0 // https://www.c64-wiki.com/wiki/PLOT_(KERNAL)
  .label VEC_IOBASE = $fff3

  .label VEC_NMI    = $fffa // NMI vector
  .label VEC_RESET  = $fffc // Hardware reset vector
  .label VEC_IRQ    = $fffe // IRQ / BRK vector
}
