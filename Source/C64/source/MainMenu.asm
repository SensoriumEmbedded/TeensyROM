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
   ;!set Debug = 1 ;if defined, skips HW checks/waits 
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "source\Menu_Regs.i"  ;IO space registers matching Teensy code

   ;color scheme:
   BorderColor      = PokePurple
   BackgndColor     = PokeBlack
   TimeColor        = ChrOrange
   MenuMiscColor    = ChrGreen
   AssignedIOHColor = ChrLtGrey
   OptionColor      = ChrYellow
   SourcesColor     = ChrLtBlue
   TypeColor        = ChrBlue
   NameColor        = ChrLtGreen
   M2SDataColumn    = 14

   ;Zero page RAM Registers:
   PtrAddrLo   = $fb
   PtrAddrHi   = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   ;other RAM Registers/code space
   ;$0334-033b is "free space"
   MusicPlaying     = $0335 ;is the music playing?
   MusicInterrupted = $0336 ;Music muted for item selection
   SIDVoicCont      = $0338 ;midi2sid polyphonic voice/envelope controls
   SIDAttDec        = $0339
   SIDSusRel        = $033a
   SIDDutyHi        = $033b
   
   ;$033c-03fb is the tape buffer (192 bytes)
   PRGLoadStartReloc= $033c 
   
   ;RAM coppies:
   MainCodeRAM = $2400    ;this file
   SIDCodeRAM = $1000 

;******************************* Main Code Start ************************************   

* = MainCodeRAM
Start:

;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
   
!ifndef Debug {
;check for HW:
   lda rRegPresence1+IO1Port
   cmp #$55
   bne NoHW
   lda rRegPresence2+IO1Port
   cmp #$AA
   beq +
NoHW
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
   ;jmp (BasicWarmStartVect)
-  jmp -
}

+  lda #rCtlVanishROM ;Deassert Game & ExROM
   sta wRegControl+IO1Port

   lda #$00
   sta MusicInterrupted ;init reg
   jsr SIDCodeRAM ;Initialize music
   
   jsr ListMenuItems

   ;check default registers for music & time settings
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudMusicMask
   sta MusicPlaying
   beq +
   jsr SIDMusicOn

+  lda rwRegPwrUpDefaults+IO1Port
   and #rpudNetTimeMask
   beq +
   jsr SynchEthernetTime
   jmp HighlightCurrent
   
+  lda #0  ;set clock to midnight if not synching
   sta TODHoursBCD  ;stop TOD regs incrementing
   sta TODMinBCD
   sta TODSecBCD
   sta TODTenthSecBCD ;have to write 10ths to release latch, start incrementing

   
   
HighlightCurrent:   
   lda rwRegCursorItemOnPg+IO1Port 
   jsr InverseRow
WaitForKey:     
   jsr DisplayTime
   jsr GetIn    
   beq WaitForKey

+  cmp #ChrReturn
   bne +
   lda rwRegCursorItemOnPg+IO1Port 
SelectItem
   sta rwRegSelItemOnPage+IO1Port ;select Item from page
   jsr SelectMenuItem
   lda MusicInterrupted ;turn music back on if it was before...
   beq ++
   lda #0
   sta MusicInterrupted
   jsr SIDMusicOn 
++ jsr ListMenuItems ; reprint menu
   jmp HighlightCurrent ;highlight the same one (or dir change sets to 0)
   
+  cmp #ChrCRSRDn  ;Move cursor down
   bne +
   ldx rwRegCursorItemOnPg+IO1Port 
   txa
   jsr InverseRow ;unhighlight the current
   inx   
   cpx rRegNumItemsOnPage+IO1Port ;last item?
   beq ++ 
   stx rwRegCursorItemOnPg+IO1Port 
   jmp HighlightCurrent
++ jsr PageDown   ;at bottom of page, page down
   jmp HighlightCurrent
   
+  cmp #ChrCRSRUp  ;Move cursor up
   bne +
   lda rwRegCursorItemOnPg+IO1Port  
   tax
   jsr InverseRow ;unhighlight the current
   cpx #0
   bne ++
   jsr PageUp  ;at top of page, page up
   ldx rRegNumItemsOnPage+IO1Port 
++ dex   
   stx rwRegCursorItemOnPg+IO1Port 
   jmp HighlightCurrent

+  cmp #ChrUpArrow ;Up directory
   bne +  
   lda #rCtlUpDirectoryWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg
   jsr ListMenuItems ; reprint menu
   jmp HighlightCurrent 

+  cmp #ChrHome ;top of directory
   bne +  
   lda #1  ;first page
   sta rwRegPageNumber+IO1Port
   lda #0  ;set to first item
   sta rwRegCursorItemOnPg+IO1Port 
   jsr ListMenuItems ; reprint menu
   jmp HighlightCurrent 
   
+  cmp #'a'  
   bmi +   ;skip if below 'a'
   cmp #'z'+1
   bpl +   ;skip if above 'z'
   sta wRegSearchLetterWAIT+IO1Port
   jsr WaitForTRWaitMsg
   jsr ListMenuItems ; reprint menu
   jmp HighlightCurrent 

+  cmp #ChrCRSRRight  ;Next Page
   bne +
   lda rwRegCursorItemOnPg+IO1Port 
   jsr InverseRow ;unhighlight the current (in case 1 page)
   jsr PageDown
   jmp HighlightCurrent

+  cmp #ChrCRSRLeft  ;Prev Page
   bne +
   lda rwRegCursorItemOnPg+IO1Port 
   jsr InverseRow ;unhighlight the current (in case 1 page)
   jsr PageUp
   lda #0
   sta rwRegCursorItemOnPg+IO1Port ;set cursor to the first item in dir
   jmp HighlightCurrent  

+  cmp #ChrF1  ;Teensy mem Menu
   bne +
   lda #rmtTeensy
   jsr ListMenuItemsChangeInit
   jmp HighlightCurrent  

+  cmp #ChrF2  ;Exit to BASIC
   bne +
   lda #rCtlBasicReset ;reset to BASIC
   sta wRegControl+IO1Port
-  jmp -  ;should be resetting to BASIC

+  cmp #ChrF3  ;SD Card Menu
   bne +
   lda #rmtSD
   jsr ListMenuItemsChangeInit
   jmp HighlightCurrent  

+  cmp #ChrF4  ;toggle music
   bne +
   jsr ToggleSIDMusic
   jmp WaitForKey  

+  cmp #ChrF5  ;USB Drive Menu
   bne +
   lda #rmtUSBDrive
   jsr ListMenuItemsChangeInit
   jmp HighlightCurrent  

+  cmp #ChrF6  ;Settings Menu
   bne +
   jsr SettingsMenu
   jsr ListMenuItems
   jmp HighlightCurrent  

+  cmp #ChrF7  ;Exe USB Host file
   bne +
   lda #rmtUSBHost
   jsr ListMenuItemsChangeInit
   jmp HighlightCurrent

+  cmp #ChrF8  ;MIDI to SID
   bne +
   jsr MIDI2SID
   jsr ListMenuItems
   jmp HighlightCurrent


+  jmp WaitForKey

   
; ******************************* Subroutines ******************************* 

___Subroutines________________________________:


;                           list out item number, type, & names
ListMenuItemsChangeInit:  ;changing menu source.  Prep: Load acc with menu to change to
   sta rWRegCurrMenuWAIT+IO1Port  ;must wait on a write (load dir)
   jsr WaitForTRWaitMsg
ListMenuItems:
   jsr PrintBanner 
   
   ldx #20 ;row
   ldy #0  ;col
   clc
   jsr SetCursor
   lda #<MsgSelect1
   ldy #>MsgSelect1
   jsr PrintString
   
   lda rwRegPageNumber+IO1Port
   jsr PrintIntByte
   lda #'/'
   jsr SendChar   
   lda rRegNumPages+IO1Port
   jsr PrintIntByte
   
   lda #<MsgSelect2
   ldy #>MsgSelect2
   jsr PrintString
   
   ldx #1  ;row
   ldy #0  ;col
   ;clc
   jsr SetCursor
   lda #<MsgSource
   ldy #>MsgSource
   jsr PrintString 
   ;print menu source from table:
   lda rWRegCurrMenuWAIT+IO1Port ;don't have to wait on a read
   asl ;double it to point to word
   tax
   lda TblMsgMenuName,x
   ldy TblMsgMenuName+1,x
   jsr PrintString
   
   ;display parent dir/path
   lda #ChrReturn
   jsr SendChar
   lda #rsstShortDirPath 
   jsr PrintSerialString

   ;There should always be at least one item, even if it's "Empty"
   lda #0       ;initialize to first Item on Page
   sta rwRegSelItemOnPage+IO1Port
nextLine
   lda #ChrReturn
   jsr SendChar 
   lda #ChrCRSRRight
   jsr SendChar
; print name
   lda #NameColor
   jsr SendChar
   lda #rsstItemName
   jsr PrintSerialString
;align to col
   sec
   jsr SetCursor ;read current to load x (row)
   ldy #MaxItemDispLength + 1  ;set y = col
   clc
   jsr SetCursor
; Assigned IO Handler? '+' if so
   lda rRegItemTypePlusIOH+IO1Port 
   and #$80  ;bit 7 indicates an assigned IOHandler, now we care!
   beq +
   lda #<MsgHasHandler
   ldy #>MsgHasHandler
   jsr PrintString
; print type, incl color
+  ldx #<TblItemType
   ldy #>TblItemType
   lda rRegItemTypePlusIOH+IO1Port 
   and #$7f  ;bit 7 indicates an assigned IOHandler, don't care
   jsr Print4CharTable
   
;line is done printing, check for next...
MenuLineDone
   inc rwRegSelItemOnPage+IO1Port
   ldx rwRegSelItemOnPage+IO1Port
   cpx rRegNumItemsOnPage+IO1Port
   bne nextLine
   ;all items listed
   rts


;Execute/select an item from the list
; Dir, ROM, copy PRG to RAM and run, etc
;Pre-Load rwRegSelItemOnPage+IO1Port with Item # to execute/select
SelectMenuItem:
   lda rRegItemTypePlusIOH+IO1Port ;Read Item type selected
   and #$7f  ;bit 7 indicates an assigned IOHandler, we don't care here
   cmp #rtDirectory  ;check for dir selected
   beq DirUpdate  
   
   pha
   lda MusicPlaying ;turn music off if it's on
   beq ++
   sta MusicInterrupted
   jsr SIDMusicOff     
++ pla
   
   cmp #rtFileHex  ;check for .hex file selected
   beq FWUpdate  
   ;not a dir or hex file, prep for messaging
   jsr PrintBanner
   lda #NameColor
   jsr SendChar

   lda #rCtlStartSelItemWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots ;if it's a ROM/crt image, it won't return from this unless error

   lda rRegStrAvailable+IO1Port 
   bne XferCopyRun   ;if it's a PRG (x-fer ready), x-fer it and launch. Won't return!
   
   ;If at this point, couldn't load item, and wasn't a dir, .hex, .prg or .p00
   jsr AnyKeyMsgWait   
   rts ;SelectMenuItem

DirUpdate:
   lda #rCtlStartSelItemWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg
   lda #0
   sta rwRegCursorItemOnPg+IO1Port  ;set cursor to the first item in dir
   rts ;SelectMenuItem

FWUpdate:
   jsr PrintBanner ;clear screen for messaging
   lda #NameColor
   jsr SendChar
   lda #ChrReturn
   jsr SendChar
   lda #rsstItemName
   jsr PrintSerialString
   lda #<MsgFWVerify  ;Verification prompt
   ldy #>MsgFWVerify
   jsr PrintString 

-  jsr GetIn    ; wait for user confirmation
   beq -
   cmp #'n'  
   beq +
   cmp #'y'  
   bne -

   lda #<MsgFWInProgress  ;In Progress Warning
   ldy #>MsgFWInProgress
   jsr PrintString 
   lda #rCtlStartSelItemWAIT ;kick off the update routine
   sta wRegControl+IO1Port   
   jsr WaitForTRDots   

   ;if we get to this point without rebooting, there's been an error...
   lda #<MsgFWUpdateFailed 
   ldy #>MsgFWUpdateFailed
   jsr PrintString 
   jsr AnyKeyMsgWait
+  rts ;SelectMenuItem

XferCopyRun:
   ;copy PRGLoadStart code to tape buffer area in case this area gets overwritten
   ;192 byte limit, watch size of PRGLoadStart block!  check below
   ;no going back now...
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
   ;rts ;SelectMenuItem never returns

AnyKeyMsgWait:
   lda #<MsgAnyKey  ;wait for any key to continue 
   ldy #>MsgAnyKey
   jsr PrintString 
-  jsr GetIn    
   beq -
   rts

;WaitForTR* uses acc, X and Y
WaitForTRDots:  ;prints a dot per second while waiting, doesn't move cursor
   ldy TODSecBCD ;reset dot second counter
   jmp WaitForTRMain
WaitForTRWaitMsg:  ;Print Waiting message in upper right and waits
   ldx #1 ;row   Show "Waiting:" over time disp
   ldy #29  ;col
   clc
   jsr SetCursor
   lda #<MsgWaiting
   ldy #>MsgWaiting
   jsr PrintString
   ldy #$ff ;don't print dots
WaitForTRMain   ;Main wait loop
   ;!ifndef Debug {   ;} 
   inc ScreenCharMemStart+40*2-2 ;spinner @ top-1, right-1
   cpy #$ff
   beq +    ;skip dot printing if wait message selected
   cpy TODSecBCD  ;no latch/unlatch needed for only reading seconds
   beq +
   ldy TODSecBCD  ;print 1 dot/sec while waiting
   lda #'.'
   jsr SendChar
+  lda rwRegStatus+IO1Port
   cmp #rsC64Message
   beq ++
   cmp #rsReady
   bne WaitForTRMain
++ ldx#5 ;require 1+5 consecutive reads of same rsC64Message/rsReady to continue
-  cmp rwRegStatus+IO1Port
   bne WaitForTRMain
   dex
   bne -
   ; the ball is in the C64 court, either done or display message and cont waiting
   cmp #rsReady
   beq +
   ; display message:
   lda #rsstSerialStringBuf ;pre-populated message
   jsr PrintSerialString
   lda #rsContinue    ;tell fw we're done reading msg, continue
   sta rwRegStatus+IO1Port
   jmp WaitForTRMain
+  rts

SynchEthernetTime:
   lda #rCtlGetTimeWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg 
   lda rRegLastHourBCD+IO1Port
   sta TODHoursBCD  ;stop TOD regs incrementing
   lda rRegLastMinBCD+IO1Port
   sta TODMinBCD
   lda rRegLastSecBCD+IO1Port
   sta TODSecBCD
   lda #9
   sta TODTenthSecBCD ;have to write 10ths to release latch, start incrementing
   rts
   
PageUp:
   ldx rwRegPageNumber+IO1Port
   cpx #1  ;on first page?
   bne +
   ldx rRegNumPages+IO1Port ;roll over to last
   cpx #1
   beq ++ ;do nothing if there's only 1 page
   inx ;avoids a jump to skip next
+  dex   
   stx rwRegPageNumber+IO1Port
   jsr ListMenuItems   
++ rts

PageDown:
   ldx rwRegPageNumber+IO1Port
   cpx rRegNumPages+IO1Port ;on last page?
   bne +
   ldx rRegNumPages+IO1Port 
   cpx #1
   beq ++ ;do nothing if there's only 1 page
   ldx #0 ;on last page, roll over to first
+  inx
   stx rwRegPageNumber+IO1Port
   jsr ListMenuItems
++ lda #0  ;set to first item
   sta rwRegCursorItemOnPg+IO1Port 
   rts
   
InverseRow:
   ;Acc contains item number to toggle reverse
   ;Uses A and Y   
   ;get mem loc from table:
   asl ;double it to point to word
   tay
   lda TblRowToMemLoc+1,y
   sta PtrAddrHi
   lda TblRowToMemLoc,y
   sta PtrAddrLo 

   ldy #40
-  lda (PtrAddrLo), y 
   eor #$80 ; toggle reverse 
   sta (PtrAddrLo),y
   dey
   bne -
   rts

TblRowToMemLoc:
   !word ScreenCharMemStart+40*(3+ 0)-1
   !word ScreenCharMemStart+40*(3+ 1)-1
   !word ScreenCharMemStart+40*(3+ 2)-1
   !word ScreenCharMemStart+40*(3+ 3)-1
   !word ScreenCharMemStart+40*(3+ 4)-1
   !word ScreenCharMemStart+40*(3+ 5)-1
   !word ScreenCharMemStart+40*(3+ 6)-1
   !word ScreenCharMemStart+40*(3+ 7)-1
   !word ScreenCharMemStart+40*(3+ 8)-1
   !word ScreenCharMemStart+40*(3+ 9)-1
   !word ScreenCharMemStart+40*(3+10)-1
   !word ScreenCharMemStart+40*(3+11)-1
   !word ScreenCharMemStart+40*(3+12)-1
   !word ScreenCharMemStart+40*(3+13)-1
   !word ScreenCharMemStart+40*(3+14)-1
   !word ScreenCharMemStart+40*(3+15)-1
   !word ScreenCharMemStart+40*(3+16)-1
   !word ScreenCharMemStart+40*(3+17)-1
   !word ScreenCharMemStart+40*(3+18)-1
   !word ScreenCharMemStart+40*(3+19)-1
   !word ScreenCharMemStart+40*(3+20)-1
   ; check MaxItemsPerPage
   
   
   !src "source\SettingsMenu.asm"
   !src "source\PRGLoadStartReloc.s"
   !src "source\SIDRelated.s"
   !src "source\StringFunctions.s"
   !src "source\StringsMsgs.s"

EndOfAllMenuCode = *
   !byte 0
