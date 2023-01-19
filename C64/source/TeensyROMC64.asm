
   !convtab pet   ;key in and text out conv to PetSCII throughout
 
   ;PacketSize = 64

   ;Registers:
   PtrAddrLo  = $fb
   PtrAddrHi  = $fc
   ;LastPage   = $fd
   
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
   
   RCtlExitToBASIC = 0
   RCtlStartRom    = 1
   RCtlLoadFromSD  = 2
   RCtlLoadFromUSB = 3

   rt16k  = 0
   rt8kHi = 1
   rt8kLo = 2
   rtPrg  = 3

   ;Kernal routines:
   SendChar = $ffd2
   ScanKey  = $ff9f ;SCNKEY
   GetIn    = $ffe4 ;GETIN
   
   ;BASIC routines:
   BasicWarmStartVect = $a002
   PrintString =  $ab1e


CompType = 3 ;Choose Compile type:  1=BASIC 0801, 2=UPPER c000, 3=ROMCRT 8000

!if CompType = 1 {
; Load into Basic RAM (0801) directly and run from there 
; no crunch needed 
; 
   BasicStart = $0801
   *=BasicStart
   ;BASIC SYS header
   !byte $0b,$08,$01,$00,$9e  ; Line 1 SYS
   !tx "2064" ;dec address for sys start in text
   !byte $00,$00,$00
   code = 2064 ;$0810
}

!if CompType = 2 {
;Option #2
; Load into upper memory, use cruncher to to transfer/call  
; SET cruncherArgs=-x$c000 -c64 -g55 -fshort
;
   code       = $c000  ;49152
}

!if CompType = 3 {
;Option #3
; Store in EEPROM/cartridge
; no crunch needed  
; SET build=%filename%.bin
; SET compilerArgs=-r %buildPath%\%compilerReport% --vicelabels %buildPath%\%compilerSymbols% --msvc --color --format plain -v3 --outfile
; burn to eeprom, file won't import to emulator due to 2 missing start addr bytes in file
;
* = $9fff                     ; set a byte to cause fill up to -$9fff (or $bfff if 16K)
   !byte 0
;  jump vectors and autostart key     
   CartStart = $8000
   *=CartStart
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
   code       = *
}
   

;******************************* Real Code Start ************************************   
   *=code  ; Start location for code
   
;   Setup stuff:
   ;sei
     
   lda #6  ;blue
   sta BorderColor
   lda #0   ;black
   sta BackgroundColor
   
   lda #<MsgWelcome
   ldy #>MsgWelcome
   jsr PrintString  

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
;list out rom number, type, & names
   ldx #0 ;initialize to first ROM
NextROM:

   lda#13  ;next line
   jsr SendChar
   lda#5   ;White Number/letter
   jsr SendChar
   lda#32  ;space
   jsr SendChar
   stx rwRegSelect  
   txa
   jsr PrintHexNibble  ;selection
   lda#32  ;space
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
   
   lda#158  ;Yellow Name string
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
   jsr ScanKey  ; (since interrupts are disabled)
   jsr GetIn    
   beq WaitForKey

   cmp #'0'  ;'1'-'9' inclusive 
   bmi notAlphaNum   ;skip if below '1'
   cmp #'9'+ 1  
   bpl notAlphaNum   ;skip if above '9'
   
   jsr SendChar ;print entered char
   ;convert to ROM number
   sec       ;set to subtract without carry
   sbc #'0'  ;now 0-9 
   sta rwRegSelect ;select ROM/PRG
   lda rRegROMType
   cmp #rtPrg  ;Is it a PRG?
   bne ROMStart
   
   jsr PRGtoMem
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
   jmp (BasicWarmStartVect)     
   
notAlphaNum:
   ;check for function keys:

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
   