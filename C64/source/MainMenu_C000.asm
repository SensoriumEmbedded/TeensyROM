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


; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout

   ;Zero page RAM Registers:
   PtrAddrLo   = $fb
   PtrAddrHi   = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   ;other RAM Registers/code space
   RegMenuPageStart = $0334 ;$0334-033b is "free space"
   SIDVoicCont      = $0335
   SIDAttDec        = $0336
   SIDSusRel        = $0337
   SIDDutyHi        = $0338
   PRGLoadStartReloc= $033c ;$033c-03fb is the tape buffer (192 bytes)
   
   ;RAM coppies:
   MainCodeRAM = $c000    ;this file
   SIDCodeRAM = $1000 

   ScreenMemStart    = $0400
   BorderColorReg    = $d020 
   BackgndColorReg   = $d021
   SIDLoc            = $d400
   IO1Port           = $de00
   TODHoursBCD       = $dc0b
   TODMinBCD         = $dc0a
   TODSecBCD         = $dc09
   TODTenthSecBCD    = $dc08
   
   ;!!!!!These need to match Teensy Code: Menu_Regs.h !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   MaxItemNameLength = 28
   
   rRegStatus        =  0 ;//Busy when doing SD/USB access.  note: loc 0(DE00) gets written to at reset
   rRegStrAddrLo     =  1 ;//lo byte of start address of the prg file being transfered to mem
   rRegStrAddrHi     =  2 ;//Hi byte of start address
   rRegStrAvailable  =  3 ;//zero when inactive/complete 
   rRegStreamData    =  4 ;//next byte of data to transfer, auto increments when read
   wRegControl       =  5 ;//RegCtlCommands: execute specific functions
   rRegPresence1     =  6 ;//for HW detect: 0x55
   rRegPresence2     =  7 ;//for HW detect: 0xAA
   rRegLastHourBCD   =  8 ;//Last TOD Hours read
   rRegLastMinBCD    =  9 ;//Last TOD Minutes read
   rRegLastSecBCD    = 10 ;//Last TOD Seconds read
   rWRegCurrMenuWAIT = 11 ;//RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItem      = 12 ;//select Menu Item for name, type, execution, etc
   rRegNumItems      = 13 ;//num items in menu list
   rRegItemType      = 14 ;//regItemTypes: type of item 
   rRegItemNameStart = 15 ;//MaxItemNameLength bytes long (incl term)
   rRegItemNameTerm  = rRegItemNameStart + MaxItemNameLength
   StartSIDRegs      = rRegItemNameTerm+1  ;//start of SID Regs, matching SID Reg order ($D400)
   rRegSIDFreqLo1    = StartSIDRegs +  0 
   rRegSIDFreqHi1    = StartSIDRegs +  1
   rRegSIDDutyLo1    = StartSIDRegs +  2
   rRegSIDDutyHi1    = StartSIDRegs +  3
   rRegSIDVoicCont1  = StartSIDRegs +  4
   rRegSIDAttDec1    = StartSIDRegs +  5
   rRegSIDSusRel1    = StartSIDRegs +  6

   rRegSIDFreqLo2    = StartSIDRegs +  7
   rRegSIDFreqHi2    = StartSIDRegs +  8
   rRegSIDDutyLo2    = StartSIDRegs +  9
   rRegSIDDutyHi2    = StartSIDRegs + 10
   rRegSIDVoicCont2  = StartSIDRegs + 11
   rRegSIDAttDec2    = StartSIDRegs + 12
   rRegSIDSusRel2    = StartSIDRegs + 13

   rRegSIDFreqLo3    = StartSIDRegs + 14
   rRegSIDFreqHi3    = StartSIDRegs + 15
   rRegSIDDutyLo3    = StartSIDRegs + 16
   rRegSIDDutyHi3    = StartSIDRegs + 17
   rRegSIDVoicCont3  = StartSIDRegs + 18
   rRegSIDAttDec3    = StartSIDRegs + 19
   rRegSIDSusRel3    = StartSIDRegs + 20

   rRegSIDFreqCutLo  = StartSIDRegs + 21
   rRegSIDFreqCutHi  = StartSIDRegs + 22
   rRegSIDFCtlReson  = StartSIDRegs + 23
   rRegSIDVolFltSel  = StartSIDRegs + 24
   EndSIDRegs        = StartSIDRegs + 25

   rRegSIDStrStart   = StartSIDRegs + 26
   ;  9: 3 chars per voice (oct, note, shrp)
   ;  1: Out of voices indicator
   ;  3: spaces betw
   ; 14 total w// term:  ON# ON# ON# X
   rRegSIDOutOfVoices= StartSIDRegs + 38
   rRegSIDStringTerm = StartSIDRegs + 39
    

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
   
   ;color scheme:
   BorderColor      = pokePurple
   BackgndColor     = pokeBlack
   TimeColor        = ChrOrange
   MenuMiscColor    = ChrGreen
   ROMNumColor      = ChrDrkGrey
   OptionColor      = ChrYellow
   SourcesColor     = ChrLtBlue
   TypeColor        = ChrBlue
   NameColor        = ChrLtGreen
   MaxMenuDispItems = 16
   M2SDataColumn    = 14

;******************************* Main Code Start ************************************   

* = MainCodeRAM
Start:

;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
   lda #$00
   jsr SIDCodeRAM ;Initialize music
   jsr SIDMusicOn ;Start the music!
   
;check for HW:
   lda rRegPresence1+IO1Port
   cmp #$55
   bne NoHW
   lda rRegPresence2+IO1Port
   cmp #$AA
   beq +
NoHW:
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
-  jmp -

+  lda #rCtlVanish ;Deassert Game & ExROM
   sta wRegControl+IO1Port

   jsr ListMenuItemsInit
   ;jsr SynchEthernetTime

WaitForKey:     
   jsr DisplayTime
   jsr GetIn    
   beq WaitForKey

   cmp #'a'  
   bmi +   ;skip if below 'a'
   cmp #'a'+ MaxMenuDispItems + 1  
   bpl +   ;skip if above MaxMenuDispItems
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'a'  ;now 0-?
   clc
   adc RegMenuPageStart   
   ;ROMSelected, ROM num in acc
   cmp rRegNumItems+IO1Port 
   bpl WaitForKey   ;skip if above num of ROMs
   sta rwRegSelItem+IO1Port ;select Item from list
   jsr SelectMenuItem
   jmp WaitForKey

+  cmp #ChrCSRSDn  ;Next Page
   bne +
   lda RegMenuPageStart
   clc
   adc #MaxMenuDispItems
   cmp rRegNumItems+IO1Port
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
   sta wRegControl+IO1Port
-  jmp -  ;should be resetting to BASIC

+  cmp #ChrF3  ;SD Card Menu
   bne +
   lda #rmtSD
   jsr ListMenuItemsChangeInit
   jmp WaitForKey  

+  cmp #ChrF4  ;toggle music
   bne +
   ldx #>irqRastSID
   cpx $315  ;see if the IRQ is pointing at our SID routine
   beq on
   jsr SIDMusicOn ;sid is off, turn it on
   jmp WaitForKey
on jsr SIDMusicOff ;sid is on, turn it off
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

+  cmp #ChrF8  ;MIDI to SID
   bne +
   jsr MIDI2SID
   jsr ListMenuItems
   jmp WaitForKey



+  jmp WaitForKey

   
; ******************************* Subroutines ******************************* 
;                           list out rom number, type, & names

Sssssssssssssssssssssssubroutines:
ListMenuItemsChangeInit:  ;Prep: Load acc with menu to change to
   sta rWRegCurrMenuWAIT+IO1Port  ;must wait on a write (load dir)
   jsr WaitForTR
ListMenuItemsInit:
   lda #0       ;initialize to first Item
   sta RegMenuPageStart
ListMenuItems:  ;Prep: load RegMenuPageStart with first ROM on menu page
   lda #<MsgBanner
   ldy #>MsgBanner
   jsr PrintString 
   lda #<MsgFrom
   ldy #>MsgFrom
   jsr PrintString 
   ;print menu source:
   lda rWRegCurrMenuWAIT+IO1Port ;don't have to wait on a read
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
   lda rRegNumItems+IO1Port
   bne +
   lda #<MsgNoItems
   ldy #>MsgNoItems
   jsr PrintString
   jmp finishMenu
+  lda RegMenuPageStart
   sta rwRegSelItem+IO1Port
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
   lda #<rRegItemNameStart+IO1Port
   ldy #>rRegItemNameStart+IO1Port
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
   ldx #<TblItemType
   ldy #>TblItemType
   lda rRegItemType+IO1Port 
   jsr Print4CharTable
;print ROM #
   lda #ROMNumColor
   jsr SendChar
   lda rwRegSelItem+IO1Port
   jsr PrintHexByte
   
;line is done printing, check for next...
   pla ;menu select letter
   inc rwRegSelItem+IO1Port
   ldx rwRegSelItem+IO1Port
   cpx rRegNumItems+IO1Port
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
;Pre-Load rwRegSelItem+IO1Port with Item # to execute/select
SelectMenuItem:
   ldy rRegItemType+IO1Port ;grab this now it will change if new directory is loaded
   lda #rCtlStartSelItemWAIT
   sta wRegControl+IO1Port
   jsr WaitForTR ;if it's a good ROM/crt image, it won't return from this
   cpy #rtPrg
   beq XferCopyRun  ;if it's a program, x-fer and launch, otherwise reprint menu and return
   jsr ListMenuItemsInit
   rts
   
XferCopyRun:
   ;copy PRGLoadStart code to tape buffer area in case this (Cxxx) area gets overwritten
   ;192 byte limit, watch size of PRGLoadStart block!  check below
   lda rRegStrAvailable+IO1Port
   bne +
   lda #<MsgErrNoData;no data to read!
   ldy #>MsgErrNoData
   jmp ErrOut
   ;no going back now...
+  jsr SIDMusicOff    
   lda #<MsgLoading
   ldy #>MsgLoading
   jsr PrintString
   lda #<rRegItemNameStart+IO1Port
   ldy #>rRegItemNameStart+IO1Port
   jsr PrintString

   lda #>PRGLoadStart
   ldy #<PRGLoadStart   
   sta PtrAddrHi
   sty PtrAddrLo 
   lda #>PRGLoadStartReloc
   ldy #<PRGLoadStartReloc   
   sta Ptr2AddrHi
   sty Ptr2AddrLo 
   ldy #$00
-  lda (PtrAddrLo), y 
   sta (Ptr2AddrLo),y
   iny
   cpy #PRGLoadEnd-PRGLoadStart  ;check length in build report here
   bne -   
   jmp PRGLoadStartReloc

PRGLoadStart:
   ;this code is relocated to PRGLoadStartReloc and run from there as it 
   ;could overwrite all upper RAM.  Will not execute correctly from here (string pointers)
   ;stream PRG file from TeensyROM to RAM and set end of prg/start of variables
   ;assumes TeensyROM is set up to transfer, PRG selected and waited to complete
   ;rRegStrAvailable+IO1Port is zero when inactive/complete

   ;jsr $A644 ;new   
   lda rRegStrAddrHi+IO1Port
   sta PtrAddrHi
   lda rRegStrAddrLo+IO1Port   
   sta PtrAddrLo
   ldy #0   ;zero offset
   
-  lda rRegStrAvailable+IO1Port ;are we done?
   beq +   ;exit the loop
   lda rRegStreamData+IO1Port ;read from rRegStreamData+IO1Port increments address & checks for end
   sta (PtrAddrLo), y 
   iny
   bne -
   inc PtrAddrHi
   bne -
   ;good luck if we get to here... Trying to overflow and write to zero page
   lda #<(MsgOverflow - PRGLoadStart + PRGLoadStartReloc) ; corrected for reloc 
   ldy #>(MsgOverflow - PRGLoadStart + PRGLoadStartReloc)
   jsr PrintString   ;$ab1e
   jmp (BasicWarmStartVect)
   ;last byte of prg (+1) = y+PtrAddrLo/Hi, store this in 2D/2E
+  ldx PtrAddrHi
   tya
   clc
   adc PtrAddrLo
   bcc +
   inx
+  sta $2d  ;start of BASIC variables pointer (Lo)
   stx $2e  ; (Hi)
   sta $ae  ;End of load address (Lo)
   stx $af  ; (Hi)
   
   lda #<(MsgRunning - PRGLoadStart + PRGLoadStartReloc) ; corrected for reloc
   ldy #>(MsgRunning - PRGLoadStart + PRGLoadStartReloc)
   jsr PrintString   ;$ab1e
   ;as is done at $A52A    https://skoolkid.github.io/sk6502/c64rom/asm/A49C.html#A52A
   jsr $a659	;reset execution to start, clear variables and flush stack
   jsr $a533	;rebuild BASIC line chaining
   ;Also see https://codebase64.org/doku.php?id=base:runbasicprg
   jmp $a7ae ;BASIC warm start/interpreter inner loop/next statement (Run)
   ;jmp (BasicWarmStartVect)  
MsgRunning:
   !tx ChrReturn, ChrReturn, "running", ChrReturn, 0
MsgOverflow:
   !tx ChrReturn, "overflow!", ChrReturn, 0
PRGLoadEnd = *
     
     
WaitForTR:  ;wait for ready status, uses acc and X
   ldx#5 ;require 5 consecutive reads of ready to continue
   inc ScreenMemStart+40*2-2 ;spinner @ end of 'Time' print loc.
-  lda rRegStatus+IO1Port
   cmp #rsReady
   bne WaitForTR
   dex
   bne -
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

SynchEthernetTime:
   lda #rCtlGetTimeWAIT
   sta wRegControl+IO1Port
   jsr WaitForTR 
   lda rRegLastHourBCD+IO1Port
   sta TODHoursBCD  ;stop TOD regs incrementing
   lda rRegLastMinBCD+IO1Port
   sta TODMinBCD
   lda rRegLastSecBCD+IO1Port
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
   
; ******************************* SID stuff ******************************* 

MIDI2SID:
   jsr SIDMusicOff
   lda #<MsgBanner
   ldy #>MsgBanner
   jsr PrintString 
   lda #<MsgM2SPolyMenu
   ldy #>MsgM2SPolyMenu
   jsr PrintString 
   ;clear SID regs
   lda #0
   tax
-  sta SIDLoc, x
   inx
   cpx #(EndSIDRegs-StartSIDRegs)
   bne -

   ;  set default local settings:
   lda #0x0f ; full volume
   sta SIDLoc+rRegSIDVolFltSel-StartSIDRegs
   lda #0x02 ; 12.5% duty cycle (12 bit resolution, lo reg left at 0)
   sta SIDDutyHi
   sta SIDLoc+rRegSIDDutyHi1-StartSIDRegs
   sta SIDLoc+rRegSIDDutyHi2-StartSIDRegs
   sta SIDLoc+rRegSIDDutyHi3-StartSIDRegs
   lda #0x40 ; pulse wave
   sta SIDVoicCont
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs
   lda #0x23 ; Att=16mS, Dec=72mS
   sta SIDAttDec
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   lda #0x34 ; Sus=20%, Rel=114mS
   sta SIDSusRel
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   
M2SDispUpdate:  ;upadte all M2S status display values
   lda #NameColor
   jsr SendChar
   ldx # 5 ;row  Triangle
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x10  
   jsr PrintOnOff
   
   ldx # 6 ;row  Sawtooth
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x20  
   jsr PrintOnOff
   
   ldx # 7 ;row  Pulse
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x40  
   jsr PrintOnOff
   
   ldx #8 ;row  Duty Cycle
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDutyPct
   ldy #>TblM2SDutyPct
   lda SIDDutyHi  ;duty cycle most sig nib = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'%'
   jsr SendChar
   
   ldx # 9 ;row  Noise
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   lda SIDVoicCont
   and #0x80  
   jsr PrintOnOff
 
   ldx #11 ;row  attack
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SAttack
   ldy #>TblM2SAttack
   lda SIDAttDec  ;attack = bits 7:4
   jsr Print4CharTableHiNib
   lda #'S'
   jsr SendChar

   ldx #12 ;row  decay
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDecayRelease
   ldy #>TblM2SDecayRelease
   lda SIDAttDec  ;decay = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'S'
   jsr SendChar

   ldx #13 ;row  sustain
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SSustPct
   ldy #>TblM2SSustPct
   lda SIDSusRel   ;sustain = bits 7:4
   jsr Print4CharTableHiNib
   lda #'%'
   jsr SendChar

   ldx #14 ;row  release
   ldy #M2SDataColumn ;col
   clc
   jsr SetCursor
   ldx #<TblM2SDecayRelease
   ldy #>TblM2SDecayRelease
   lda SIDSusRel  ;release = bits 3:0
   and #$0f
   jsr Print4CharTable
   lda #'S'
   jsr SendChar

;continue into the main loop...
M2SUpdateKeyInLoop:
;refresh dynamic SID regs from MIDI:   todo: move this to an interrupt?
   lda SIDVoicCont  ;waveform in upper nibble
   ora IO1Port+rRegSIDVoicCont1 ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs 
   lda IO1Port+rRegSIDFreqHi1 
   sta SIDLoc+rRegSIDFreqHi1-StartSIDRegs 
   lda IO1Port+rRegSIDFreqLo1 
   sta SIDLoc+rRegSIDFreqLo1-StartSIDRegs 

   lda SIDVoicCont  ;waveform in upper nibble
   ora IO1Port+rRegSIDVoicCont2 ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs 
   lda IO1Port+rRegSIDFreqHi2 
   sta SIDLoc+rRegSIDFreqHi2-StartSIDRegs 
   lda IO1Port+rRegSIDFreqLo2 
   sta SIDLoc+rRegSIDFreqLo2-StartSIDRegs 

   lda SIDVoicCont  ;waveform in upper nibble
   ora IO1Port+rRegSIDVoicCont3 ;latch bit (0) from MIDI
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs 
   lda IO1Port+rRegSIDFreqHi3 
   sta SIDLoc+rRegSIDFreqHi3-StartSIDRegs 
   lda IO1Port+rRegSIDFreqLo3 
   sta SIDLoc+rRegSIDFreqLo3-StartSIDRegs 

   jsr DisplayTime
   ldx #20 ;row   ;print note vals
   ldy #3  ;col
   clc
   jsr SetCursor
   lda #NameColor
   jsr SendChar
   lda #<IO1Port+rRegSIDStrStart
   ldy #>IO1Port+rRegSIDStrStart
   jsr PrintString 
   
   jsr GetIn
   beq M2SUpdateKeyInLoop
   
   cmp #'t'  ;Triangle
   bne +
   lda #0x10
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'w'  ;saWtooth
   bne +
   lda #0x20 
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'p'  ;Pulse
   bne +
   lda #0x40 
   eor SIDVoicCont
   and #0x70  ;never combine with noise
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'u'  ;dUty cycle
   bne +
   ldx SIDDutyHi  ;duty cycle most sig nib = bits 3:0, upper unused
   inx
   stx SIDDutyHi ;apply change at time of update
   stx SIDLoc+rRegSIDDutyHi1-StartSIDRegs
   stx SIDLoc+rRegSIDDutyHi2-StartSIDRegs
   stx SIDLoc+rRegSIDDutyHi3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'n'  ;Noise
   bne +
   lda #0x80 
   ;eor SIDVoicCont  ;doesn't play nice with others
   sta SIDVoicCont
   jmp M2SDispUpdate

+  cmp #'a'  ;Attack
   bne +
   lda SIDAttDec  ;attack = bits 7:4
   clc
   adc #$10
   sta SIDAttDec ;apply change at time of update
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'d'  ;Decay
   bne +
   lda SIDAttDec  ;decay = bits 3:0
   tax
   and #$0f
   cmp #$0f
   bne dok
   txa
   and #$f0 ;Wrap Around without overflow
   jmp dcnt
dok   
   inx
   txa
dcnt
   sta SIDAttDec ;apply change at time of update
   sta SIDLoc+rRegSIDAttDec1-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec2-StartSIDRegs
   sta SIDLoc+rRegSIDAttDec3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'s'  ;Sustain
   bne +
   lda SIDSusRel  ;sustain = bits 7:4
   clc
   adc #$10
   sta SIDSusRel ;apply change at time of update
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'r'  ;Release
   bne +
   lda SIDSusRel  ;release = bits 3:0
   tax
   and #$0f
   cmp #$0f
   bne rok
   txa
   and #$f0 ;Wrap Around without overflow
   jmp rcnt
rok   
   inx
   txa
rcnt
   sta SIDSusRel ;apply change at time of update
   sta SIDLoc+rRegSIDSusRel1-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel2-StartSIDRegs
   sta SIDLoc+rRegSIDSusRel3-StartSIDRegs
   jmp M2SDispUpdate

+  cmp #'x'  ;Exit M2S
   bne +
   jsr SIDVoicesOff
   rts 

+  jmp M2SUpdateKeyInLoop

   
SIDMusicOn:  ;Start SID interrupt
   lda #$7f    ;disable all ints
   sta $dc0d   ;CIA1 int ctl
   lda $dc0d   ;CIA1 int ctl    reading clears
   sei
   lda #$01    ;raster compare enable
   sta $d01a   ;irq mask reg
   sta $d019   ;ACK any raster IRQs
   lda #100    ;mid screen
   sta $d012   ;raster scan line compare reg
   lda $d011   ;VIC ctl reg fine scrolling/control
   AND #$7f    ;bit 7 is bit 8 of scan line compare
   sta $d011   ;VIC ctl reg fine scrolling/control
   lda #<irqRastSID
   ldx #>irqRastSID
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   cli
   rts

SIDMusicOff:  ;stop SID interrupt
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
   ;jsr SIDCodeRAM  ;turns voices off, but resets song to start
   jsr SIDVoicesOff
   cli 
   lda #BorderColor
   sta BorderColorReg   ;restore border in case we ended in mid region
   rts

SIDVoicesOff:
   lda #0x00 ; turn 3 voices off
   sta SIDLoc+rRegSIDVoicCont1-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont2-StartSIDRegs
   sta SIDLoc+rRegSIDVoicCont3-StartSIDRegs 
   rts
   
irqRastSID:
   inc $d019   ;ACK raster IRQs
   inc BorderColorReg ;tweak display border
   jsr SIDCodeRAM+3 ;Play the music
   lda #<irqRast2
   ldx #>irqRast2
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   lda #200    ;loweer part of screen
   sta $d012   ;raster scan line compare reg
   jmp IRQDefault

irqRast2:
   inc $d019   ;ACK raster IRQs
   dec BorderColorReg ;tweak it back
   lda #<irqRastSID
   ldx #>irqRastSID
   sta $314    ;CINV, HW IRQ Int Lo
   stx $315    ;CINV, HW IRQ Int Hi
   lda #100    ;upper part of screen
   sta $d012   ;raster scan line compare reg
   
   jmp IRQDefault

; ******************************* Strings/Messages ******************************* 
MmmmmmmmmmessagesText:
MsgBanner:    
   !tx ChrClear, ChrToLower, ChrPurple, ChrRvsOn, "             TeensyROM v0.5             ", ChrRvsOff, 0
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
   !tx " ", ChrRvsOn, OptionColor, "F6", ChrRvsOff, MenuMiscColor, " Ethernet Time Sync", ChrReturn
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
MsgError:
   !tx ChrRed, "Error: ", 0
MsgErrNoData:
   !tx "No Data Available", 0
;MsgErrNoFile:
;   !tx "No File Available", 0
   
TblItemType: ;must match rtNone, rt16k, etc order!
   !tx "--- ","16k ","8Hi ","8Lo ","Prg ","Unk ","Crt ","Dir " ;4 bytes each, no term
   
EndOfAllCode = *

