
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "../MainMenuCRT/source/c64defs.i"  ;C64 colors, mem locations, etc.
   ;!src "../MainMenuCRT/source/CommonDefs.i" ;Common between crt loader and main code in RAM
   !src "../MainMenuCRT/source/Menu_Regs.i"  ;IO space registers matching Teensy code

   ;r0   = $fb
   ;r0L  = $fb
   ;r0H  = $fc
   ;r1   = $fd
   ;r1L  = $fd
   ;r1H  = $fe


   * = $0801    ;BasicStart

   !word BasicEnd                  ; BASIC pointer to next line
   !word 2026                      ; BASIC line number (setting to year)
   !byte $9e                       ; BASIC token code for "SYS" command
   !byte (SysAddress/1000)+$30     ; Calculate 1st digit of SysAddress
   !byte ((SysAddress/100)%10)+$30 ; Calculate 2nd digit of SysAddress
   !byte ((SysAddress/10)%10)+$30  ; Calculate 3rd digit of SysAddress
   !byte SysAddress%10+$30         ; Calculate 4th digit of SysAddress
   !byte $00                       ; BASIC end of line marker
BasicEnd    
   !word $0000                     ; BASIC end of program

SysAddress:

;screen setup:     
   ;copy colors from IO1 to local RAM
   ldx #NumColorRefs
-  lda rwRegColorRefStart-1+IO1Port, x ;zero based offset
   sta TblEscC-1, x
   dex
   bne -
   
   jsr ScreenColorOnly ;update screen colors now that we have them

   ;store default register for 12/24 hour time display locally
   lda rwRegPwrUpDefaults+IO1Port
   and #rpudClock12_24hr
   sta smc24HourClockDisp+1  ;local copy of 12/24 hr
   
   ;always read current TOD from Teensy RTC, battery backed or not...
   jsr SetC64TODfromRTC

   jmp PageUpdate  ;jump to default page

bPageNum:  ;current page num/default
   !byte 0
   
bTotalPages: ;num of pages in tblSettingsPages
   !byte 6
   
tblSettingsPages:
   !word HelpMenu
   !word HelpMenu2
   !word GeneralSettings
   !word ColorConfigMenu   
   !word MIDIMenu
   !word EthernetMenu
   
   !src "source/SupportFunctions.asm"
   !src "source/StringFunctions.asm"
   !src "source/StringMsgs.asm"
   ;settings pages:
   !src "source/ColorConfig.asm"
   !src "source/MIDISettings.asm"
   !src "source/EthernetSettings.asm"
   !src "source/GeneralSettings.asm"
   !src "source/HelpInfo.asm"
   !src "source/HelpInfo2.asm"
   
EndOfCode:
   !byte $00 ;byte to mark end address in build report


