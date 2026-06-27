
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "../MainMenuCRT/source/c64defs.i"  ;C64 colors, mem locations, etc.
   !src "../MainMenuCRT/source/Menu_Regs.i"  ;IO space registers matching Teensy code

   ;!set DbgOffline = 1   ;if defined, skips all waits/dependancies on TR HW


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

   !src "source/SupportFunctions.asm"
   !src "source/StringFunctions.asm"

SysAddress:

;screen setup:     
!ifndef DbgOffline {
   ;copy colors from IO1 to local RAM
   ldx #NumColorRefs
-  lda rwRegColorRefStart-1+IO1Port, x ;zero based offset
   sta TblEscC-1, x
   dex
   bne -
}  
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
   
NumPages = 9 ;num of pages in tblSettingsPages

bTotalPages: 
   !byte NumPages
   
tblSettingsPages:
   ;!word GeneralSettings
   !word IndexMenu
   !word TRSettings
   !word StartupOptionsMenu
   !word ColorConfigMenu   
   !word MIDIMenu
   !word TimeRTCMenu
   !word InfoOtherMenu
   !word EthernetMenu
   !word InfoHotKeyMenu
      
   ;settings pages:
   !src "source/Pg_Index.asm"
   !src "source/Pg_InfoOther.asm"
   !src "source/Pg_TRSettings.asm"
   !src "source/Pg_StartupOptions.asm"
   !src "source/Pg_ColorConfig.asm"
   !src "source/Pg_MIDISettings.asm"
   !src "source/Pg_EthernetSettings.asm"
   !src "source/Pg_TimeRTCSettings.asm"
   !src "source/Pg_InfoHotKey.asm"
   
EndOfCode:
   !byte $00 ;byte to mark end address in build report


