
; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout

   ;RAM Registers:
   PtrAddrLo   = $fb
   PtrAddrHi   = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   RegMenuPageStart = $4e
   
   ;RAM coppies:
   MainCodeRAM = $c000    ;this file
   SIDCodeRAM = $1000 

   ;These need to match Teensy Code!
   IO1Port           = $de00
   BorderColorReg    = $d020 
   BackgndColorReg   = $d021
   
   MAX_ROMNAME_CHARS = 25
   
   rRegStatus     = IO1Port + 0
   rRegStrAddrLo  = IO1Port + 1
   rRegStrAddrHi  = IO1Port + 2
   rRegStrData    = IO1Port + 3  
   wRegControl    = IO1Port + 4
   rRegPresence1  = IO1Port + 5
   rRegPresence2  = IO1Port + 6
   rwRegSelect    = IO1Port + 7
   rRegNumROMs    = IO1Port + 8
   rRegROMType    = IO1Port + 9
   rRegROMName    = IO1Port + 10 ;MAX_ROMNAME_CHARS max len
   
   RCtlVanish      = 0
   RCtlVanishReset = 1
   RCtlStartRom    = 2
   RCtlLoadFromSD  = 3
   RCtlLoadFromUSB = 4

   rtNone = 0
   rt16k  = 1
   rt8kHi = 2
   rt8kLo = 3
   rtPrg  = 4


   ;Kernal routines:
   IRQDefault = $ea31
   SendChar   = $ffd2
   ScanKey    = $ff9f ;SCNKEY
   GetIn      = $ffe4 ;GETIN
   SetCursor  = $fff0 ;PLOT
   
   ;BASIC routines:
   BasicColdStartVect = $a000 ; $e394  58260
   BasicWarmStartVect = $a002 ; $e37b  58235
   PrintString =  $ab1e

   ;chr$ symbols
   ChrBlack   = 144
   ChrWhite   = 5
   ChrRed     = 28
   ChrCyan    = 159
   ChrPurple  = 156
   ChrGreen   = 30
   ChrBlue    = 31
   ChrYellow  = 158 
   ChrOrange  = 129
   ChrBrown   = 149
   ChrLtRed   = 150
   ChrDrkGrey = 151
   ChrMedGrey = 152
   ChrLtGreen = 153
   ChrLtBlue  = 154
   ChrLtGrey  = 155
   
   ChrF1      = 133
   ChrF2      = 137
   ChrF3      = 134
   ChrF4      = 138
   ChrF5      = 135
   ChrF6      = 139
   ChrF7      = 136
   ChrF8      = 140
   ChrToLower = 14
   ChrToUpper = 142
   ChrRvsOn   = 18
   ChrRvsOff  = 146
   ChrClear   = 147
   ChrReturn  = 13
   ChrSpace   = 32
   
;poke colors
   pokeBlack   = 0
   pokeWhite   = 1
   pokeRed     = 2
   pokeCyan    = 3
   pokePurple  = 4
   pokeGreen   = 5
   pokeBlue    = 6
   pokeYellow  = 7
   pokeOrange  = 8
   pokeBrown   = 9
   pokeLtRed   = 10
   pokeDrkGrey = 11
   pokeMedGrey = 12
   pokeLtGreen = 13
   pokeLtBlue  = 14
   pokeLtGrey  = 15
   
   ROMNumColor  = ChrMedGrey
   OptionColor  = ChrYellow
   TypeColor    = ChrLtBlue
   NameColor    = ChrGreen
   BorderColor  = pokePurple
   BackgndColor = pokeBlack
   MaxMenuItems = 17

;******************************* Main Code Start ************************************   

* = MainCodeRAM
  
;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
   jsr SIDOn
   
;check for HW:
   lda rRegPresence1
   cmp #$55
   bne NoHW
   lda rRegPresence2
   cmp #$AA
   beq HWGood
NoHW:
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
   jmp AllDone

HWGood:
   lda #RCtlVanish ;Deassert Game & ExROM
   sta wRegControl

   lda #1 ;initialize to first page/ROM;  ROM#1 shown first (0 is for transfers)
   sta RegMenuPageStart
   jsr ListROMs

WaitForKey:     
   ;jsr ScanKey  ;only needed if ints are off
   jsr GetIn    
   beq WaitForKey

   cmp #'a'  
   bmi notAlpha   ;skip if below 'a'
   cmp #'a'+ MaxMenuItems + 1  
   bpl notAlpha   ;skip if above MaxMenuItems
   ;jsr SendChar ;print entered char
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'a'  ;now 0-?
   clc
   adc RegMenuPageStart   
;ROMSelected, ROM num in acc
   cmp rRegNumROMs 
   bpl WaitForKey   ;skip if above num of ROMs
   sta rwRegSelect ;select ROM/PRG
   jsr RunPRGROM
   jmp WaitForKey
notAlpha:
;check for function keys:

   cmp #ChrF1  ;F1 - Next Page
   bne notF1
   ;code...
   lda RegMenuPageStart
   clc
   adc #MaxMenuItems
   cmp rRegNumROMs
   bpl WaitForKey  ;already last page
   sta RegMenuPageStart
   jsr ListROMs
   jmp WaitForKey
notF1:

   cmp #ChrF2  ;F2 - Prev Page
   bne notF2
   lda RegMenuPageStart
   cmp #1     ;ROM#1 shown first (0 is for transfers)
   beq WaitForKey  ;already on first page
   sec
   sbc #MaxMenuItems
   sta RegMenuPageStart
   jsr ListROMs
   jmp WaitForKey  
notF2:

   cmp #ChrF3  ;F3 - Exe USB Host file
   bne notF3
   ;check if file is present
   
   ;copy to RAM and execute
   lda #0     ;0 is received file in RAM
   sta rwRegSelect ;select ROM/PRG   
   jsr RunPRGROM
   jmp WaitForKey
notF3:

   cmp #ChrF5  ;F5 - Exit to BASIC
   bne notF5
   lda #RCtlVanishReset ;reset to BASIC
   sta wRegControl
   jmp AllDone  ;should be resetting to BASIC
notF5:

   cmp #ChrF7  ;F7 - toggle music
   bne notF7
   ldx #>irqSID
   cpx $315  ;see if the IRQ is pointing at our SID routine
   beq +
   jsr SIDOn ;sid is off, turn it on
   jmp WaitForKey
+  jsr SIDOff ;sid is on, turn it off
   jmp WaitForKey  
notF7:

   jmp WaitForKey

AllDone:
   jmp AllDone ;(BasicWarmStartVect) 

   
; ******************************* Subroutines ******************************* 
;                           list out rom number, type, & names
;Prep: load RegMenuPageStart with first ROM on menu page
Ssssssssssssssubroutines:
ListROMs:
   lda #<MsgWelcome
   ldy #>MsgWelcome
   jsr PrintString  
   lda RegMenuPageStart
   sta rwRegSelect
   lda #'A' ;initialize to start of page
NextMenuLine:
   pha ;remember menu letter
   lda #ChrReturn
   jsr SendChar
   
;print option letter
   lda #OptionColor
   jsr SendChar
   lda #ChrSpace
   jsr SendChar
   pla
   pha
   jsr SendChar
   lda #'-'
   jsr SendChar
; print name
   lda #NameColor
   jsr SendChar
   lda #<rRegROMName
   ldy #>rRegROMName
   jsr PrintString
;align to col
   sec
   jsr SetCursor ;read current to load y (row)
   ldy #MAX_ROMNAME_CHARS + 3  ;col
   clc
   jsr SetCursor
; print type
   lda #TypeColor
   jsr SendChar
   lda rRegROMType 
   clc
   rol
   rol  ;mult by 4
   tay
-  lda TblROMType,y
   jsr SendChar   ;type (4 chars)
   iny
   tya
   and #3
   bne -
;print ROM #
   lda #ROMNumColor
   jsr SendChar
   lda rwRegSelect
   jsr PrintHexByte
   
;line is done printing, check for next...
   pla ;menu select letter
   inc rwRegSelect
   ldx rwRegSelect
   cpx rRegNumROMs
   beq RomListDone
   clc
   adc #01
   cmp #'A' + MaxMenuItems
   bne NextMenuLine  

RomListDone:
   ldx #20 ;row
   ldy #0  ;col
   clc
   jsr SetCursor
   lda #<MsgSelect
   ldy #>MsgSelect
   jsr PrintString
   rts
   
   
PRGtoMem:
   ;stream PRG file from TeensyROM to $0801
   ;assumes TeensyROM is set up to transfer, PRG selected
   ;rRegStrAddrHi is zero when inactive/complete
   
   lda #RCtlStartRom  ;tell TR we're ready to transfer
   sta wRegControl
   
   lda rRegStrAddrHi
   bne +
   lda #<MsgErrNoData;no data to read!
   ldy #>MsgErrNoData
   jmp ErrOut
+  sta PtrAddrHi
   lda rRegStrAddrLo   
   sta PtrAddrLo
   ldy #0   ;zero offset
xferloop:
   lda rRegStrData ;read from rRegStrData increments address/checks for end
   sta (PtrAddrLo), y 
   lda rRegStrAddrHi ;are we done?
   beq xfercomplete 
   iny
   bne xferloop
   inc PtrAddrHi
   bne xferloop
   lda #<MsgErrOverflow ;Overflow!
   ldy #>MsgErrOverflow
   jmp ErrOut
   
xfercomplete:
   rts
  
  
ErrOut:   
   ;Error msg pointer stored in acc/y
   pha
   tya
   pha
   ldx #19 ;row
   ldy #0  ;col
   clc
   jsr SetCursor
   lda #<MsgError
   ldy #>MsgError
   jsr PrintString   
   pla
   tay
   pla
   jsr PrintString
   rts

PrintHexByte:
   ;Print byte value stored in acc in hex
   ;  2 chars plus space
   ;preserves acc
   pha
   ror
   ror
   ror
   ror
   jsr PrintHexNibble
   pla
   pha
   jsr PrintHexNibble
   lda #ChrSpace
   jsr SendChar
   pla
   rts
   
PrintHexNibble:   
   ;Print value stored in lower nible acc in hex
   ;trashes acc
   and #$0f
   cmp #$0a
   bpl letter 
   clc
   adc #'0'
   jmp printret
letter:
   clc
   adc #'a'-$0a
printret:
   jsr SendChar
   rts

;Execute a ROM or copy PRG to RAM and run
;Pre-Load rwRegSelect with ROM/PRG# to execute
RunPRGROM:
   ldx rRegROMType
   cpx #rtNone
   bne +
   lda #<MsgErrNoFile ;No File loaded 
   ldy #>MsgErrNoFile
   jmp ErrOut
+  pla ;pull the jsr return info from the stack, we're not going back!
   cpx #rtPrg  
   bne ROMStart
;it's a PRG...
   jsr SIDOff
   jsr PRGtoMem
   ;load keyboard buffer with "run\n":  
   lda #'r'
   sta $0277  ;kbd buff 0
   lda #'u'
   sta $0278 ;kbd buff 1
   lda #'n'
   sta $0279  ;kbd buff 2
   lda #ChrReturn
   sta $027a  ;kbd buff 3
   lda #4
   sta $C6  ;# chars in kbd buff (4 of 10 max)
   ;lda #RCtlVanish ;Deassert Game & ExROM (already done at start)
   ;sta wRegControl
   jmp (BasicWarmStartVect)  
ROMStart:   
   lda #RCtlStartRom
   sta wRegControl
x  jmp x ;TR should take it from here and reset...
   

; ******************************* SID music/code ******************************* 
   
;Initialize SID interrupt
SIDOn:
   lda #$00
   jsr SIDCodeRAM ;Initialize music
   lda #$7f
   sta $dc0d   ;CIA int ctl
   lda $dc0d   ;CIA int ctl
   sei
   lda #$01
   sta $d01a  ;irq enable
   lda #$64
   sta $d012  ;raster val
   lda $d011  ;VIC ctl reg
   AND #$7f
   sta $d011  ;VIC ctl reg
   lda #<irqSID
   ldx #>irqSID
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi
   cli
   rts

SIDOff:
   sei
   lda #<IRQDefault
   ldx #>IRQDefault
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi
   lda #$81
   sta $dc0d  ;CIA int ctl
   lda #0
   sta $d01a  ;irq enable
   inc $d019
   lda $dc0d  ;CIA int ctl
   jsr SIDCodeRAM
   cli 
   rts

irqSID:
   inc $d019   ;ACK raster IRQs
   inc BorderColorReg ;tweak display border
   jsr SIDCodeRAM+3 ;Play the music
   dec BorderColorReg ;tweak it back
   jmp IRQDefault

; ******************************* Strings/Messages ******************************* 

MsgWelcome:    
   !tx ChrClear, ChrToLower, ChrPurple, ChrRvsOn, "            TeensyROM v0.01             ", ChrRvsOff, 0
MsgSelect:
   !tx ChrPurple, "Select from above, or...", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F1", ChrRvsOff, NameColor, " Next Page       "
   !tx " ", ChrRvsOn, OptionColor, "F2", ChrRvsOff, NameColor, " Previous Page   "
   !tx " ", ChrRvsOn, OptionColor, "F3", ChrRvsOff, NameColor, " From USB Host   "
   !tx " ", ChrRvsOn, OptionColor, "F4", ChrRvsOff, NameColor, " From USB Drive  "
   !tx " ", ChrRvsOn, OptionColor, "F5", ChrRvsOff, NameColor, " Exit to BASIC   "
   !tx " ", ChrRvsOn, OptionColor, "F6", ChrRvsOff, NameColor, " From Ethernet   "
   !tx " ", ChrRvsOn, OptionColor, "F7", ChrRvsOff, NameColor, " Music on/off    "
   !tx " ", ChrRvsOn, OptionColor, "F8", ChrRvsOff, NameColor, " Credits"
   !tx 0
MsgNoHW:
   !tx ChrReturn, ChrReturn, ChrToLower, ChrYellow, "TeensyROM hardware not detected!!!", ChrReturn, 0
MsgError:
   !tx ChrRed, "Error: ", 0
MsgErrNoData:
   !tx "No Data Available", 0
MsgErrOverflow:
   !tx "Overflow", 0
MsgErrNoFile:
   !tx "No File Available", 0
   
TblROMType:
   !tx "None","16k ","8Hi ","8Lo ","PRG " ;4 bytes each, no term
   
   
EndOfAllCode = *

