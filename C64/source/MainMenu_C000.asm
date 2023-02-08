
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

   ScreenMemStart    = $0400
   BorderColorReg    = $d020 
   BackgndColorReg   = $d021
   IO1Port           = $de00
   TODHoursBCD       = $dc0b
   TODMinBCD         = $dc0a
   TODSecBCD         = $dc09
   TODTenthSecBCD    = $dc08
   
   ;!!!!!These need to match Teensy Code: Menu_Regs.h !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   MaxItemNameLength = 28
   
   rRegStatus        = IO1Port + 0
   rRegStrAddrLo     = IO1Port + 1
   rRegStrAddrHi     = IO1Port + 2
   rRegStreamData    = IO1Port + 3  
   wRegControl       = IO1Port + 4
   rRegPresence1     = IO1Port + 5
   rRegPresence2     = IO1Port + 6
   rRegLastHourBCD   = IO1Port + 7
   rRegLastMinBCD    = IO1Port + 8
   rRegLastSecBCD    = IO1Port + 9
   rWRegCurrMenuWAIT = IO1Port + 10 
   rwRegSelItem      = IO1Port + 11
   rRegNumItems      = IO1Port + 12
   rRegItemType      = IO1Port + 13
   rRegItemName      = IO1Port + 14 ;MaxItemNameLength bytes long (incl term)
   
   rsReady      = 0x5a
   rsChangeMenu = 0x9d
   rsStartItem  = 0xb1
   rsGetTime    = 0xe6
   ;rsError      = 0x24

   rmtSD        = 0
   rmtTeensy    = 1
   rmtUSBHost   = 2
   rmtUSBDrive  = 3
   
   rCtlVanish           = 0
   rCtlVanishReset      = 1
   rCtlStartSelItemWAIT = 2
   rCtlGetTimeWAIT      = 3

   rtNone = 0  ;synch with TblItemType below
   rt16k  = 1
   rt8kHi = 2
   rt8kLo = 3
   rtPrg  = 4
   rtUnk  = 5
   rtCrt  = 6
   rtDir  = 7
   
;!!!!!!!!!!!!!!!!  End Teensy matching  !!!!!!!!!!!!!!!!!!


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
   ChrCSRSUp  = 145
   ChrCSRSDn  = 17
   
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
   
   TimeColor        = ChrBrown
   MenuMiscColor    = ChrGreen
   ROMNumColor      = ChrDrkGrey
   OptionColor      = ChrYellow
   SourcesColor     = ChrLtBlue
   TypeColor        = ChrBlue
   NameColor        = ChrLtGreen
   BorderColor      = pokePurple
   BackgndColor     = pokeBlack
   MaxMenuDispItems = 16

;******************************* Main Code Start ************************************   

* = MainCodeRAM
  
;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
   jsr SIDOn ;Start the music!
   
;check for HW:
   lda rRegPresence1
   cmp #$55
   bne NoHW
   lda rRegPresence2
   cmp #$AA
   beq +
NoHW:
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
-  jmp -

+  lda #rCtlVanish ;Deassert Game & ExROM
   sta wRegControl

   jsr ListMenuItemsInit
   ;jsr SynchEthernetTime

WaitForKey:     
   jsr DisplayTime
   ;jsr ScanKey  ;only needed if ints are off
   jsr GetIn    
   beq WaitForKey

   cmp #'a'  
   bmi +   ;skip if below 'a'
   cmp #'a'+ MaxMenuDispItems + 1  
   bpl +   ;skip if above MaxMenuDispItems
   ;jsr SendChar ;print entered char
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'a'  ;now 0-?
   clc
   adc RegMenuPageStart   
   ;ROMSelected, ROM num in acc
   cmp rRegNumItems 
   bpl WaitForKey   ;skip if above num of ROMs
   sta rwRegSelItem ;select Item from list
   jsr SelectMenuItem
   jmp WaitForKey

+  cmp #ChrCSRSDn  ;Currsor Down - Next Page
   bne +
   lda RegMenuPageStart
   clc
   adc #MaxMenuDispItems
   cmp rRegNumItems
   bpl WaitForKey  ;already on last page
   sta RegMenuPageStart
   jsr ListMenuItems
   jmp WaitForKey

+  cmp #ChrCSRSUp  ;Prev Page
   bne +
   lda RegMenuPageStart
   ;cmp #0     
   beq WaitForKey  ;already on first page
   sec
   sbc #MaxMenuDispItems
   sta RegMenuPageStart
   jsr ListMenuItems
   jmp WaitForKey  

+  cmp #ChrF1  ;Teensy mem Menu
   bne +
   lda #rmtTeensy
   jsr ListMenuItemsChangeInit
   jmp WaitForKey  

+  cmp #ChrF2  ;Exit to BASIC
   bne +
   lda #rCtlVanishReset ;reset to BASIC
   sta wRegControl
-  jmp -  ;should be resetting to BASIC

+  cmp #ChrF3  ;SD Card Menu
   bne +
   lda #rmtSD
   jsr ListMenuItemsChangeInit
   jmp WaitForKey  

+  cmp #ChrF4  ;toggle music
   bne +
   ldx #>irqSID
   cpx $315  ;see if the IRQ is pointing at our SID routine
   beq on
   jsr SIDOn ;sid is off, turn it on
   jmp WaitForKey
on jsr SIDOff ;sid is on, turn it off
   jmp WaitForKey  

+  cmp #ChrF5  ;USB Drive Menu
   bne +
   lda #rmtUSBDrive
   jsr ListMenuItemsChangeInit
   jmp WaitForKey  

+  cmp #ChrF6  ;Synch Ethernet Time
   bne +
   jsr SynchEthernetTime
   jmp WaitForKey  

+  cmp #ChrF7  ;Exe USB Host file
   bne +
   lda #rmtUSBHost
   jsr ListMenuItemsChangeInit
   jmp WaitForKey



+  jmp WaitForKey

   
; ******************************* Subroutines ******************************* 
;                           list out rom number, type, & names

Sssssssssssssssssssssssubroutines:
ListMenuItemsChangeInit:  ;Prep: Load acc with menu to change to
   sta rWRegCurrMenuWAIT  ;must wait on a write (load dir)
   jsr WaitForTR
ListMenuItemsInit:
   lda #0       ;initialize to first Item
   sta RegMenuPageStart
ListMenuItems:  ;Prep: load RegMenuPageStart with first ROM on menu page
   lda #<MsgWelcome
   ldy #>MsgWelcome
   jsr PrintString 
   ;print menu source:
   lda rWRegCurrMenuWAIT ;don't have to wait on a read
   cmp #rmtSD
   bne +
   lda #<MsgMenuSD
   ldy #>MsgMenuSD
   jmp cont1

+  cmp #rmtTeensy
   bne +
   lda #<MsgMenuTeensy
   ldy #>MsgMenuTeensy
   jmp cont1
   
+  cmp #rmtUSBDrive
   bne +
   lda #<MsgMenuUSBDrive
   ldy #>MsgMenuUSBDrive
   jmp cont1
   
+  ;cmp #rmtUSBHost
   ;bne +
   lda #<MsgMenuUSBHost
   ldy #>MsgMenuUSBHost
   ;jmp cont1
   
cont1   
   jsr PrintString
   lda rRegNumItems
   bne +
   lda #<MsgNoItems
   ldy #>MsgNoItems
   jsr PrintString
   jmp finishMenu
+  lda RegMenuPageStart
   sta rwRegSelItem
   lda #'A' ;initialize to start of page
nextLine
   pha ;remember menu letter
   lda #ChrReturn
   jsr SendChar
   
;print option letter
   lda #OptionColor
   jsr SendChar
   lda #ChrSpace
   jsr SendChar
   lda #ChrRvsOn
   jsr SendChar
   pla
   pha
   jsr SendChar
   lda #ChrRvsOff
   jsr SendChar
   lda #'-'
   jsr SendChar
; print name
   lda #NameColor
   jsr SendChar
   lda #<rRegItemName
   ldy #>rRegItemName
   jsr PrintString
;align to col
   sec
   jsr SetCursor ;read current to load y (row)
   ldy #MaxItemNameLength + 3  ;col
   clc
   jsr SetCursor
; print type
   lda #TypeColor
   jsr SendChar
   lda rRegItemType 
   clc
   rol
   rol  ;mult by 4
   tay
-  lda TblItemType,y
   jsr SendChar   ;type (4 chars)
   iny
   tya
   and #3
   bne -
;print ROM #
   lda #ROMNumColor
   jsr SendChar
   lda rwRegSelItem
   jsr PrintHexByte
   
;line is done printing, check for next...
   pla ;menu select letter
   inc rwRegSelItem
   ldx rwRegSelItem
   cpx rRegNumItems
   beq finishMenu
   clc
   adc #01
   cmp #'A' + MaxMenuDispItems
   bne nextLine  
finishMenu
   ldx #20 ;row
   ldy #0  ;col
   clc
   jsr SetCursor
   lda #<MsgSelect
   ldy #>MsgSelect
   jsr PrintString
   rts

;Execute/select an item from the list
; Dir, ROM, copy PRG to RAM and run, etc
;Pre-Load rwRegSelItem with Item # to execute/select
SelectMenuItem:
   ldy rRegItemType ;grab this now it will change if new directory is loaded
   lda #rCtlStartSelItemWAIT
   sta wRegControl
   jsr WaitForTR ;if it's a good ROM image, it won't return from this
   cpy #rtPrg
   beq PRGStart ;if it's a program, x-fer and lunch, otherwise reprint menu and return
   jsr ListMenuItemsInit
   rts
PRGStart
   pla ;pull the jsr return info from the stack, we're not going back!
   jsr SIDOff
   jsr PRGtoMem
   lda #ChrGreen
   jsr SendChar 
   lda #ChrClear
   jsr SendChar 
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
   jmp (BasicWarmStartVect)  
   
WaitForTR:  ;wait for ready status, uses acc and X
   ldx#5 ;require 5 consecutive reads of ready to continue
   inc ScreenMemStart+40*2-2 ;end of 'Time' print loc.
-  lda rRegStatus
   cmp #rsReady
   bne WaitForTR
   dex
   bne -
   rts
   
PRGtoMem:
   ;stream PRG file from TeensyROM to RAM
   ;assumes TeensyROM is set up to transfer, PRG selected and waited to complete
   ;rRegStrAddrHi is zero when inactive/complete
   
   lda rRegStrAddrHi
   bne +
   lda #<MsgErrNoData;no data to read!
   ldy #>MsgErrNoData
   jmp ErrOut
+  sta PtrAddrHi
   lda rRegStrAddrLo   
   sta PtrAddrLo
   ldy #0   ;zero offset
-  lda rRegStreamData ;read from rRegStreamData increments address/checks for end
   sta (PtrAddrLo), y 
   lda rRegStrAddrHi ;are we done?
   beq rt 
   iny
   bne -
   inc PtrAddrHi
   bne -
   lda #<MsgErrOverflow ;Overflow!
   ldy #>MsgErrOverflow
   jmp ErrOut
rt rts
  
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

SynchEthernetTime:
   lda #rCtlGetTimeWAIT
   sta wRegControl
   jsr WaitForTR 
   lda rRegLastHourBCD
   sta TODHoursBCD  ;stop TOD regs incrementing
   lda rRegLastMinBCD
   sta TODMinBCD
   lda rRegLastSecBCD
   sta TODSecBCD
   lda #9
   sta TODTenthSecBCD ;have to write 10ths to release latch, start incrementing
   rts
   
DisplayTime:
   ldx #1 ;row
   ldy #29  ;col
   clc
   jsr SetCursor
   lda #TimeColor
   jsr SendChar
   lda TODHoursBCD ;latches time in regs (stops incrementing)
   tay ;save for am/pm at end
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
   ror
   ror
   ror
   ror
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
MmmmmmmmmmessagesText:
MsgWelcome:    
   !tx ChrClear, ChrToLower, ChrPurple, ChrRvsOn, "            TeensyROM v0.01             ", ChrRvsOff
   !tx ChrReturn, SourcesColor, "From ", 0 
MsgSelect:
   !tx SourcesColor, "Sources:          "
   !tx ChrRvsOn, OptionColor, "Up", ChrRvsOff, MenuMiscColor, "/", ChrRvsOn, OptionColor, "Dn", ChrRvsOff, MenuMiscColor, "CRSR: Page", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F1", ChrRvsOff, SourcesColor,  " Teensy Mem   "
   !tx " ", ChrRvsOn, OptionColor, "F2", ChrRvsOff, MenuMiscColor, " Exit to BASIC", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F3", ChrRvsOff, SourcesColor,  " SD Card      "
   !tx " ", ChrRvsOn, OptionColor, "F4", ChrRvsOff, MenuMiscColor, " Music on/off", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F5", ChrRvsOff, SourcesColor,  " USB Drive    "
   !tx " ", ChrRvsOn, OptionColor, "F6", ChrRvsOff, MenuMiscColor, " Ethernet Time Sync", ChrReturn
   !tx " ", ChrRvsOn, OptionColor, "F7", ChrRvsOff, SourcesColor,  " USB Host     "
   !tx " ", ChrRvsOn, OptionColor, "F8", ChrRvsOff, MenuMiscColor, " Credits/Info"
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


MsgError:
   !tx ChrRed, "Error: ", 0
MsgErrNoData:
   !tx "No Data Available", 0
MsgErrOverflow:
   !tx "Overflow", 0
;MsgErrNoFile:
;   !tx "No File Available", 0
   
TblItemType: ;must match rtNone, rt16k, etc order!
   !tx "None","16k ","8Hi ","8Lo ","Prg ","Unk ","Crt ","Dir " ;4 bytes each, no term
   
   
EndOfAllCode = *

