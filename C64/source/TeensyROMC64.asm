
; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout

   ;RAM Registers:
   PtrAddrLo   = $fb
   PtrAddrHi   = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   RegMenuPageStart = $4e
   
   ;RAM coppies:
   VanishCodeRAM = $c000    
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

   rt16k  = 0
   rt8kHi = 1
   rt8kLo = 2
   rtPrg  = 3


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

;********************************   Cartridge begin   ********************************   

;8k Cartridge/ROM   ROML $8000-$9FFF
;  Could make 16k if needed

* = $9fff                     ; set a byte to cause fill up to -$9fff (or $bfff if 16K)
   !byte 0
   
* = $8000  ;Cart Start
;  jump vectors and autostart key     
   !word Coldstart    ; Cartridge cold-start vector   ;!byte $09, $80 = !word $8009
   !word Warmstart    ; Cartridge warm-start vector
   !byte $c3, $c2, $cd, $38, $30    ; CBM8O - Autostart key
;  KERNAL RESET ROUTINE
Coldstart:
   sei
   stx $d016            ; Turn on VIC for PAL / NTSC check
   jsr $fda3            ; IOINIT - Init CIA chips
   jsr $fd50            ; RANTAM - Clear/test system RAM
   jsr $fd15            ; RESTOR - Init KERNAL RAM vectors
   jsr $ff5b            ; CINT   - Init VIC and screen editor
   cli                  ; Re-enable IRQ interrupts
;  BASIC RESET  Routine
Warmstart:
   jsr $e453            ; Init BASIC RAM vectors
   jsr $e3bf            ; Main BASIC RAM Init routine
   jsr $e422            ; Power-up message / NEW command
   ldx #$fb
   txs                  ; Reduce stack pointer for BASIC
   

;******************************* Main Code Start ************************************   
   
;   Setup stuff:

   jsr SIDCopyToRAM
   jsr SIDOn

;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
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
   lda #0 ;initialize to first page/ROM
   sta RegMenuPageStart
   jsr ListROMs

WaitForKey:     
   ;jsr ScanKey  ;only needed if ints are off
   jsr GetIn    
   beq WaitForKey

   cmp #'a'  ;'a'-'q' inclusive 
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
   lda rRegROMType
   cmp #rtPrg  ;Is it a PRG?
   bne ROMStart
   
   jsr SIDOff
   jsr PRGtoMem
   jmp VanishBASICRun
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
   beq WaitForKey  ;already on first page
   sec
   sbc #MaxMenuItems
   sta RegMenuPageStart
   jsr ListROMs
   jmp WaitForKey  
notF2:

   cmp #ChrF3  ;F3 - ???
   bne notF3
   ;code...
   jmp WaitForKey  
notF3:

   cmp #ChrF5  ;F5 - Exit to BASIC
   bne notF5
   lda #RCtlVanishReset ;put the TeensyROM to sleep  and reset
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

ROMStart:   
   lda #RCtlStartRom
   sta wRegControl
   ;TR should take it from here...
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
   bne cont1
   lda #1 ;no data to read!
   jmp ErrOut

cont1:   
   sta PtrAddrHi
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
   lda #2 ;Overflow!
   jmp ErrOut
   
xfercomplete:
   rts
  
  
ErrOut:   
   ;ErrNum stored in acc
   pha
   lda #<MsgError
   ldy #>MsgError
   jsr PrintString   
   pla
   jsr PrintHexByte
   lda #13
   jsr SendChar
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

   
; ******************************* Vanish, Code to Copy ******************************* 

VanishBASICRun:
   ;copy code to RAM and execute from there so we can kill the TeensyROM
   ldy#0
CodeCopyLoop:
   lda CodeToCopy,y
   sta VanishCodeRAM,y
   iny
   cpy #(EndCoppiedCode - CodeToCopy)
   bne CodeCopyLoop   
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
   
   jmp VanishCodeRAM

;!align 255, 0	; align to page (256 bytes) No longer needed
CodeToCopy:  ;must be <256 bytes total
   lda #RCtlVanish ;put the TeensyROM to sleep (Deassert Game & ExROM)
   sta wRegControl
   jmp (BasicWarmStartVect)  
   
   ;;xColdstart:
   ;sei
   ;jsr $fd50            ; RANTAM - Clear/test system RAM (determine bytes free)
   ;jsr $fd15            ; RESTOR - Init KERNAL RAM vectors
   ;jsr $ff5b            ; CINT   - Init VIC and screen editor
   ;;;jsr $ff87  ;RAMTAS Memory initialization
   ;cli                  ; Re-enable IRQ interrupts
   ;jmp (BasicColdStartVect)    
EndCoppiedCode = *


; ******************************* SID music/code ******************************* 

SIDCopyToRAM:
;have to copy SID code to RAM because it self modifies...
   lda #>SIDCode
   ldy #<SIDCode   
   sta PtrAddrHi
   sty PtrAddrLo 
   ldx #(>EndSIDCode)+1 ;last page+1, will copy ((EndSIDCode-SIDCode) | 0xFF) bytes
   
   lda #>SIDCodeRAM
   ldy #<SIDCodeRAM   
   sta Ptr2AddrHi
   sty Ptr2AddrLo 
   
   ldy #0 ;initialize
SIDCopyLoop:
   lda (PtrAddrLo), y 
   sta (Ptr2AddrLo),y
   iny
   bne SIDCopyLoop   
   inc PtrAddrHi
   inc Ptr2AddrHi
   cpx PtrAddrHi
   bne SIDCopyLoop
   rts
   
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

SIDCode = *
      !binary "source\SleepDirt_extra_ntsc_1000_6581.sid.seq",, $7c+2   ;;skip header and 2 byte load address
EndSIDCode = *


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
MsgError:
   !tx ChrReturn, "Error #", 0
MsgNoHW:
   !tx "Hardware Not Detected", ChrReturn, 0
   
TblROMType:
   !tx "16k ","8Hi ","8Lo ","PRG " ;4 bytes each, no term
   
   
EndOfAllCode = *

