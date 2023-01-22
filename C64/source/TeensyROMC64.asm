
; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
 
   ;Registers:
   PtrAddrLo  = $fb
   PtrAddrHi  = $fc
   Ptr2AddrLo  = $fd
   Ptr2AddrHi  = $fe
   
   ;RAM coppies:
   VanishCodeRAM = $c000    
   SIDCodeRAM = $1000 

   ;These need to match Teensy Code!
   IO1Port           = $de00
   BorderColor       = $d020 
   BackgroundColor   = $d021
   
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
   RCtlStartRom    = 1
   RCtlLoadFromSD  = 2
   RCtlLoadFromUSB = 3

   rt16k  = 0
   rt8kHi = 1
   rt8kLo = 2
   rtPrg  = 3

   ;Kernal routines:
   IRQDefault = $ea31
   SendChar   = $ffd2
   ScanKey    = $ff9f ;SCNKEY
   GetIn      = $ffe4 ;GETIN
   
   ;BASIC routines:
   BasicWarmStartVect = $a002
   PrintString =  $ab1e



;********************************   Cartridge begin   ********************************   

;8k Cartridge/ROM   ROML $8000-$9FFF
;  Could make 16k if needed
; SID Code is 5109-126=4983 bytes ($1377)
SIDCode = $8C00  ;should leave ~136bytes at end

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
   lda #6  ;blue
   sta BorderColor
   lda #0   ;black
   sta BackgroundColor
   
   lda #<MsgWelcome
   ldy #>MsgWelcome
   jsr PrintString  

;check for HW:
   lda rRegPresence1
   cmp #$55
   bne NoHW
   lda rRegPresence2
   cmp #$AA
   beq ListROMs
NoHW:
   lda #<MsgNoHW
   ldy #>MsgNoHW
   jsr PrintString  
   jmp AllDone

ListROMs:
;                           list out rom number, type, & names
   ldx #0 ;initialize to first ROM
NextROM:

   lda #13  ;next line
   jsr SendChar
   lda #5   ;White Number/letter
   jsr SendChar
   lda #32  ;space
   jsr SendChar
   stx rwRegSelect  
   txa
   jsr PrintHexNibble  ;selection
   lda #32  ;space
   jsr SendChar

   lda#154  ;Lt Blue Type
   jsr SendChar
   lda rRegROMType 
   clc
   rol
   rol
   tax
NxtTypChar:
   lda TblROMType,x
   jsr SendChar   ;type (4 chars)
   inx
   txa
   and #3
   bne NxtTypChar
   
   lda #158  ;Yellow Name string
   jsr SendChar
   lda #<rRegROMName
   ldy #>rRegROMName
   jsr PrintString
   
   ldx rwRegSelect
   inx
   cpx rRegNumROMs
   bne NextROM

   lda #<MsgSelect
   ldy #>MsgSelect
   jsr PrintString

WaitForKey:     
   ;jsr ScanKey  ;only needed if ints are off
   jsr GetIn    
   beq WaitForKey

   cmp #'0'  ;'1'-'9' inclusive 
   bmi notNum   ;skip if below '1'
   cmp #'9'+ 1  
   bpl notNum   ;skip if above '9'
   jsr SendChar ;print entered char
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'0'  ;now 0-9 
   jmp ROMSelected
   
notNum:
   cmp #'a'  ;'a'-'z' inclusive 
   bmi notAlphaNum   ;skip if below 'a'
   cmp #'z'+ 1  
   bpl notAlphaNum   ;skip if above 'z'
   jsr SendChar ;print entered char
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'a'-10  ;now 10-25 
   
ROMSelected:   ;ROM num in acc
   cmp rRegNumROMs 
   bpl WaitForKey   ;skip if above num of ROMs
   sta rwRegSelect ;select ROM/PRG
   lda rRegROMType
   cmp #rtPrg  ;Is it a PRG?
   bne ROMStart
   
   jsr SIDOff
   jsr PRGtoMem
   jmp VanishBASICRun
   
notAlphaNum: ;check for function keys:
   cmp #136  ;F7 - toggle music
   bne notF7
   ldx #>irqSID
   cpx $315  ;CINV,,\ HW IRQ Int Hi
   beq SIDisOn
   jsr SIDOn ;sid is off, turn it on
   jmp WaitForKey
SIDisOn:
   jsr SIDOff ;sid is on, turn it off
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
   ;  2 chars plus a space
   ;trashes acc
   pha
   ror
   ror
   ror
   ror
   jsr PrintHexNibble
   pla
   jsr PrintHexNibble
   lda #' '
   jsr SendChar
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
 
VanishBASICRun:
   ;copy code to c000 and execute from there so we can kill the TeensyROM
   lda #>CodeToCopy
   sta PtrAddrHi
   ldy #<CodeToCopy   
   sty PtrAddrLo ;should be zero
CodeCopyLoop:
   lda (PtrAddrLo), y 
   sta VanishCodeRAM,y
   iny
   cpy #<EndCoppiedCode
   bne CodeCopyLoop   
   ;load keyboard buffer with "run\n":  
   lda #'r'
   sta $0277  ;kbd buff 0
   lda #'u'
   sta $0278 ;kbd buff 1
   lda #'n'
   sta $0279  ;kbd buff 2
   lda #13
   sta $027a  ;kbd buff 3
   lda #4
   sta $C6  ;# chars in kbd buff (10 max)
   jmp VanishCodeRAM


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
   

; ******************************* Strings/Messages ******************************* 

MsgWelcome:       ;clr screen, wht char, lower case
   !tx 147, 5, 14, "TeensyROM v0.01", 13, 0
MsgSelect:
   !tx 13, 13, 5, "Select from above: ", 0
MsgError:
   !tx 13, "Error #", 0
MsgNoHW:
   !tx "Hardware Not Detected", 13, 0
TblROMType:
   !tx "16k ","8Hi ","8Lo ","PRG " ;4 bytes each, no term
   
; ******************************* Code to Copy, SID music/code ******************************* 

!align 255, 0	; align to page (256 bytes)
CodeToCopy:  ;must be <256 bytes total, page aligned
   lda #RCtlVanish ;put the TeensyROM to sleep (Deassert Game & ExROM)
   sta wRegControl                               
   jmp (BasicWarmStartVect)    
EndCoppiedCode = *

* = SIDCode
      !binary "source\SleepDirt_extra_ntsc_1000_6581.sid.seq",, $7c+2   ;;skip header and 2 byte load address
EndSIDCode = *

irqSID:
   inc $d019   ;ACK raster IRQs
   ;inc $d020 ;tweak display border
   jsr SIDCode+3 ;Play the music
   ;dec $d020 ;tweak it back
   jmp IRQDefault
