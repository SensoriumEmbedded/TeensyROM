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
   ;!set SidDisp = 1; if defined, displayed speed info when changed and border tweaks durring int.
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "source\CommonDefs.i" ;Common between crt loader and main code in RAM
   !src "source\Menu_Regs.i"  ;IO space registers matching Teensy code

   ;other RAM Registers
   ;$0334-033b is "free space"

   SIDVoicCont      = $0338 ;midi2sid polyphonic voice/envelope controls
   SIDAttDec        = $0339
   SIDSusRel        = $033a
   SIDDutyHi        = $033b
   
   M2SDataColumn    = 14

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

   ;Get video standard and TOD frequency
   ; https://codebase64.org/doku.php?id=base:efficient_tod_initialisation
   ; Detecting TOD frequency by Silver Dream ! / Thorgal / W.F.M.H.
   sei             ; accounting for NMIs is not needed when
   lda #$00        ; used as part of application initialisation
   sta $dd08       ; TO2TEN start TOD - in case it wasn't running
-  cmp $dd08       ; TO2TEN wait until tenths
   beq -           ; register changes its value
   lda #$ff        ; count from $ffff (65535) down
   sta $dd04       ; TI2ALO both timer A register
   sta $dd05       ; TI2AHI set to $ff
   lda #%00010001  ; bit seven = 0 - 60Hz TOD mode
   sta $dd0e       ; CI2CRA start the timer
   lda $dd08       ; TO2TEN
-  cmp $dd08       ; poll TO2TEN for change
   beq -
   lda $dd05       ; TI2AHI expect (approximate) $7f4a $70a6 $3251 $20c0
   cli
   ;$7f4a for 60Hz TOD clock and 985248.444(PAL) CPU/CIA clock    10
   ;$70a6 for 60Hz TOD clock and 1022727.14(NTSC) CPU/CIA clock   11
   ;$3251 for 50Hz TOD clock and 985248.444(PAL) CPU/CIA clock    00
   ;$20c0 for 50Hz TOD clock and 1022727.14(NTSC) CPU/CIA clock   01
   ;convert from MSB timing to: bit 0: 1=NTSC, 0=PAL;    bit 1: 1=60Hz, 0=50Hz
   cmp #$29
   bcs +  ;(>29)
   ldx #%00000001  ;50/NTSC
   jmp ++
+  cmp #$51
   bcs +  ;(>51)
   ldx #%00000000  ;50/PAL
   jmp ++
+  cmp #$78
   bcs +  ;(>78)
   ldx #%00000011  ;60/NTSC
   jmp ++
+  ldx #%00000010  ;60/PAL
++ stx wRegVid_TOD_Clks+IO1Port   

   ;load SID to TR RAM
   lda #rCtlLoadSIDWAIT ; sends SID Parse messages
   sta wRegControl+IO1Port
   jsr WaitForTRDots

   jsr SIDLoadInit  ;Load SID first to set timer rate regs
   jsr IRQEnable  ; start the IRQ wedge, initial default is SID playback disable.  
 
   jsr ListMenuItems

   ;check default registers for music & time settings
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudMusicMask
   sta smcSIDPlayEnable+1  ;set default SID playback

   lda rwRegPwrUpDefaults+IO1Port
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
   
WaitForJSorKey:     
   jsr DisplayTime
   ;Check joystick first:
   lda GamePort2  
   ;and GamePort1  keyboard input scan interferes with this port
   lsr
   bcs +
   jsr CursorUp    ;js Up
   jmp JSDelay
   
+  lsr
   bcs +
   jsr CursorDown  ;js Down
   jmp JSDelay
   
+  lsr
   bcs +
   jsr PrevPage    ;js Left
   jmp JSDelay
   
+  lsr
   bcs +
   jsr NextPage    ;js Right
   jmp JSDelay
   
+  lsr
   bcs ReadKeyboard
   jsr SelectItem  ;js Fire
   ;jmp JSDelay

JSDelay:   
   lda rwRegCursorItemOnPg+IO1Port ;HighlightCurrent
   jsr InverseRow
   ;pause after action:
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudJoySpeedMask  ;upper 4 bits = speed 0-15 (00-f0)
   eor #rpudJoySpeedMask  ;make 15-0 (f0-00 step $10)
   lsr ;conv to $78-$00 step $08
   tay
   iny ;can't be 0, now $79-$01 step $08
   ;default of 9 ends up at $31 in y
   ;prev testing:ldy #$18   ;$10 is pretty quick, but controllable(?);  $80 is fairly slow
-- ldx #$ff
-  dex
   bne -
   dey
   bne --
   ;lda #0
   ;sta 198 ;clear keyboard buffer (only needed for port 1)
   ;jmp WaitForJSorKey

ReadKeyboard:
   jsr GetIn    
   beq WaitForJSorKey

+  cmp #ChrReturn
   bne +
   jsr SelectItem
   jmp HighlightCurrent ;highlight the same one (or dir change sets to 0)
   
+  cmp #ChrCRSRDn  ;Move cursor down
   bne +
   jsr CursorDown
   jmp HighlightCurrent
   
+  cmp #ChrCRSRUp  ;Move cursor up
   bne +
   jsr CursorUp
   jmp HighlightCurrent

+  cmp #ChrCRSRRight  ;Next Page
   bne +
   jsr NextPage
   jmp HighlightCurrent

+  cmp #ChrCRSRLeft  ;Prev Page
   bne +
   jsr PrevPage
   jmp HighlightCurrent  

+  cmp #'+'  ;increase SID speed
   bne +
   ldx rwRegSIDSpeedHi+IO1Port
   dex   
   jmp updatespeed  

+  cmp #'-'  ;decrease SID speed
   bne +
   ldx rwRegSIDSpeedHi+IO1Port
   inx
updatespeed
   stx rwRegSIDSpeedHi+IO1Port
   stx $dc05  ; =timer Hi, dc04=timer Low
   
   !ifdef SidDisp {
   ;print the full timer value
   lda #ChrReturn
   jsr SendChar
   txa
   jsr PrintHexByte
   lda rwRegSIDSpeedLo+IO1Port
   jsr PrintHexByte
   }
   
   jmp WaitForJSorKey  

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
   jmp WaitForJSorKey  

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

+  cmp #ChrF7  ;Help Menu
   bne +
   jsr HelpMenu
   jsr ListMenuItems
   jmp HighlightCurrent

+  cmp #ChrF8  ;MIDI to SID
   bne +
   jsr MIDI2SID
   jsr ListMenuItems
   jmp HighlightCurrent


+  jmp WaitForJSorKey

   
; ******************************* Subroutines ******************************* 

___Subroutines________________________________:


;                           list out item number, type, & names
ListMenuItemsChangeInit:  ;changing menu source.  Prep: Load acc with menu to change to
   sta rWRegCurrMenuWAIT+IO1Port  ;must wait on a write (load dir)
   jsr WaitForTRWaitMsg
ListMenuItems:
   jsr PrintBanner 
   
   ldx #23 ;row
   ldy #0  ;col
   clc
   jsr SetCursor
   lda #<MsgMainOptions1
   ldy #>MsgMainOptions1
   jsr PrintString
   ;page x/y
   lda rwRegPageNumber+IO1Port
   jsr PrintIntByte
   lda #'/'
   jsr SendChar   
   lda rRegNumPages+IO1Port
   jsr PrintIntByte
   
   lda #<MsgMainOptions2
   ldy #>MsgMainOptions2
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



SelectItem:
;Execute/select an item from the list
   lda rwRegCursorItemOnPg+IO1Port 
   sta rwRegSelItemOnPage+IO1Port ;select Item from page
   jsr InverseRow ;unhighlight the current
   
   lda rRegItemTypePlusIOH+IO1Port ;Read Item type selected
   and #$7f  ;bit 7 indicates an assigned IOHandler, we don't care here
   cmp #rtDirectory  ;check for dir selected
   bne + 
   ;DirUpdate:
   lda #rCtlStartSelItemWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRWaitMsg
   lda #0
   sta rwRegCursorItemOnPg+IO1Port  ;set cursor to the first item in dir
   jmp ListAndDone

+  cmp #rtNone ;do nothing for 'none' type
   beq AllDone 
   
   ;any type except None and sub-dir, clear screen and stop interrupts
   pha ;store the type
   jsr IRQDisable  ;turn off interrupt (also stops SID playback, if on)
   jsr PrintBanner ;clear screen for messaging for remaining types:
   lda #NameColor
   jsr SendChar
   
   pla ;restore the type
   cmp #rtFileHex  ;check for .hex file selected
   bne +
   
   ;FWUpdate  
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
   beq IRQEnContinue
   cmp #'y'  
   bne -

   lda #<MsgFWInProgress  ;In Progress Warning
   ldy #>MsgFWInProgress
   jsr PrintString  
   jsr StartSelItem_WaitForTRDots    ;kick off the update routine

   ;if we get to this point without rebooting, there's been an error...
   lda #<MsgFWUpdateFailed 
   ldy #>MsgFWUpdateFailed
   jsr PrintString 
   jsr AnyKeyMsgWait
   jmp IRQEnContinue
      
      
+  cmp #rtFileSID  ;check for .sid file selected
   bne +
   jsr StartSelItem_WaitForTRDots
   jsr SIDLoadInit ;check success, return or transfer to RAM and start playing
   lda #rpudMusicMask ;enable playback
   sta smcSIDPlayEnable+1
   jmp IRQEnContinue
    
    
   ;not a dir, "none", hex file, or SID, try to start/execute
+  jsr StartSelItem_WaitForTRDots ;if it's a ROM/crt image, it won't return from this unless error

   lda rRegStrAvailable+IO1Port 
   bne XferCopyRun   ;if it's a PRG (x-fer ready), x-fer it and launch. Won't return!!!
   
   ;If at this point, couldn't load item, and wasn't a dir, none, .hex, .prg or .p00
   jsr AnyKeyMsgWait   

IRQEnContinue   
   jsr IRQEnable
ListAndDone
   jsr ListMenuItems ; reprint menu
AllDone
   rts

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
   cpy #PRGLoadEnd-PRGLoadStart  ;check length in build report here against PRGLoadStartReloc available
   bne -   
   jmp PRGLoadStartReloc     
   ;rts ;SelectItem never returns

AnyKeyMsgWait:
   lda #<MsgAnyKey  ;wait for any key to continue 
   ldy #>MsgAnyKey
   jsr PrintString 
-  jsr GetIn    
   beq -
   rts

StartSelItem_WaitForTRDots:
   lda #rCtlStartSelItemWAIT ;kick off the selection
   sta wRegControl+IO1Port   
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

CursorUp:
   lda rwRegCursorItemOnPg+IO1Port  
   tax
   jsr InverseRow ;unhighlight the current
   cpx #0
   bne ++
   jsr PageUp  ;at top of page, page up
   ldx rRegNumItemsOnPage+IO1Port 
++ dex   
   stx rwRegCursorItemOnPg+IO1Port 
   rts

CursorDown:
   ldx rwRegCursorItemOnPg+IO1Port 
   txa
   jsr InverseRow ;unhighlight the current
   inx   
   cpx rRegNumItemsOnPage+IO1Port ;last item?
   beq ++ 
   stx rwRegCursorItemOnPg+IO1Port 
   rts
++ jsr PageDown   ;at bottom of page, page down
   rts

PrevPage:
   lda rwRegCursorItemOnPg+IO1Port 
   jsr InverseRow ;unhighlight the current (in case 1 page)
   jsr PageUp
   lda #0
   sta rwRegCursorItemOnPg+IO1Port ;set cursor to the first item in dir
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

NextPage:
   lda rwRegCursorItemOnPg+IO1Port 
   jsr InverseRow ;unhighlight the current (in case 1 page)
   jsr PageDown
   rts

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
   sta smcInverseRowSrc+2
   sta smcInverseRowDest+2
   lda TblRowToMemLoc,y
   sta smcInverseRowSrc+1 
   sta smcInverseRowDest+1 
   ldy #40
smcInverseRowSrc
-  lda $fffe, y 
   eor #$80 ; toggle reverse 
smcInverseRowDest
   sta $fffe,y
   dey
   bne -
   rts

HelpMenu:
   jsr PrintBanner
   lda #<MsgHelpMenu
   ldy #>MsgHelpMenu
   jsr PrintString 

   lda #<MsgSettingsMenu2SpaceRet
   ldy #>MsgSettingsMenu2SpaceRet
   jsr PrintString 

WaitHelpMenuKey:
   jsr DisplayTime   
   jsr GetIn    
   beq WaitHelpMenuKey

+  cmp #ChrF1  ;Teensy mem Menu
   bne +
   lda #rmtTeensy
   jmp MenuChangeInit

+  cmp #ChrF2  ;Exit to BASIC
   bne +
   lda #rCtlBasicReset ;reset to BASIC
   sta wRegControl+IO1Port
-  jmp -  ;should be resetting to BASIC

+  cmp #ChrF3  ;SD Card Menu
   bne +
   lda #rmtSD
   jmp MenuChangeInit

+  cmp #ChrF4  ;toggle music
   bne +
   jsr ToggleSIDMusic
   jmp WaitHelpMenuKey  

+  cmp #ChrF5  ;USB Drive Menu
   bne +
   lda #rmtUSBDrive
   jmp MenuChangeInit

+  cmp #ChrF6  ;Settings Menu
   bne +
   jmp SettingsMenu  ;return from there

+  cmp #ChrF7  ;Help
   bne +
   jmp HelpMenu ;refresh (could ignore)

+  cmp #ChrF8  ;MIDI to SID
   bne +
   jmp MIDI2SID  ;return from there

+  cmp #ChrSpace  ;back to Main Menu
   bne WaitHelpMenuKey   
   rts


MenuChangeInit:  ;changing menu source.  Prep: Load acc with menu to change to
   sta rWRegCurrMenuWAIT+IO1Port  ;must wait on a write (load dir)
   jsr WaitForTRWaitMsg
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
