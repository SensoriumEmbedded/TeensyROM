

; ********************************   Symbols   ********************************   
   !convtab pet   ;key in and text out conv to PetSCII throughout
   !src "..\MainMenuCRT\source\c64defs.i"  ;C64 colors, mem loctions, etc.
   !src "..\MainMenuCRT\source\CommonDefs.i" ;Common between crt loader and main code in RAM

;enum ASIDregsMatching  //synch with IOH_ASID.c
   ;// registers:
   ASIDAddrReg         = 0xc2;   // (Read only)  Data type and SID Address Register 
   ASIDDataReg         = 0xc4;   // (Read only)  ASID data, increment queue Tail 
   ASIDQueueUsed       = 0xc8;   // (Read only)  Current Queue amount used
   ASIDContReg         = 0xca;   // (Write only) Control Reg 
   
   ;// Control Reg Commands
   ;// Timer controls match TblMsgTimerState, start at 0
   ASIDContTimerOff    = 0x00;   //disable Frame Timer
   ASIDContTimerOnAuto = 0x01;   //enable Frame Timer, auto seed time
   ASIDContTimerOn50Hz = 0x02;   //enable Frame Timer, 50Hz seed time
   NumTimerStates      = 0x03;   //Always last, number of states
   ;// ...                    
   ASIDContIRQOn       = 0x10;   //enable ASID IRQ
   ASIDContIRQOff      = 0x11;   //disable ASID IRQ
   ASIDContExit        = 0x12;   //Disable IRQ, Send TR to main menu
   ;// ...              
   ASIDContBufTiny     = 0x20;   //Set buffer to size Tiny  
   ASIDContBufSmall    = 0x21;   //Set buffer to size Small  
   ASIDContBufMedium   = 0x22;   //Set buffer to size Medium 
   ASIDContBufLarge    = 0x23;   //Set buffer to size Large  
   ASIDContBufXLarge   = 0x24;   //Set buffer to size XLarge 
   ASIDContBufXXLarge  = 0x25;   //Set buffer to size XXLarge
   ASIDContBufFirstItem= ASIDContBufTiny;    //First seq item on list
   ASIDContBufLastItem = ASIDContBufXXLarge; //Last seq item on list
   ASIDContBufMask     = 0x07;   //Mask to get zero based item #

   ;// queue message types/masks
   ASIDAddrType_Skip   = 0x00;   // No data/skip, also indicates End Of Frame
   ASIDAddrType_Char   = 0x20;   // Character data
   ASIDAddrType_Start  = 0x40;   // ASID Start message
   ASIDAddrType_Stop   = 0x60;   // ASID Stop message
   ASIDAddrType_SID1   = 0x80;   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2   = 0xa0;   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3   = 0xc0;   // Lower 5 bits are SID3 reg address
   ASIDAddrType_Error  = 0xe0;   // Error from parser
                            
   ASIDAddrType_Mask   = 0xe0;   // Mask for Type
   ASIDAddrAddr_Mask   = 0x1f;   // Mask for Address
; //end enum synch


   SpinIndSID3Write   = C64ScreenRAM+40*2+2 ;spin indicator: SID3 write
   SpinIndSID2Write   = C64ScreenRAM+40*2+3 ;spin indicator: SID2 write
   SpinIndSID1Write   = C64ScreenRAM+40*2+4 ;spin indicator: SID1 write
   SpinIndUnexpType   = C64ScreenRAM+40*2+5 ;spin indicator: error: Unexpected reg type or skip received
   SpinIndPacketError = C64ScreenRAM+40*2+6 ;spin indicator: error from packet parser, see AddErrorToASIDRxQueue in IOH_ASID.c
   SIDRegColorStart   = C64ColorRAM +40*2+7
   MuteColorStart     = C64ColorRAM +40*1+32 ;start of "Mute" display
   QueueUsedIndicator = C64ScreenRAM+40*2+34 ;Queue used indicator
   
   RegFirstColor  = PokeWhite
   RegSecondColor = PokeDrkGrey
   RegOffColor    = PokeBlack  ;then to black/off
   
   StartMsgToken  = 1  ;In character queue, indicates start message
   StopMsgToken   = 2  ;In character queue, indicates stop message
   
	BasicStart = $0801
   code       = $080D ;2061
   Charset    = $3000
   
   *=BasicStart
   ;BASIC SYS header
   !byte $0b,$08,$01,$00,$9e  ; Line 1 SYS
   !tx "2061" ;"2061" Address for sys start in text
   !byte $00,$00,$00
   
   *=code  ; Start location for code


ASIDInit:
;screen setup:     
   lda #BorderColor
   sta BorderColorReg
   lda #BackgndColor
   sta BackgndColorReg
 
   ;lda $d018
   ;and #$f0
   ;ora #$0c
   lda #$1c  ;Point to custom char set at $3000
   sta $d018

   jsr UpdateAllSIDAddress
   jsr SIDinit

;set up ASID interrupt:
   ; DISABLE MASKABLE INTERRUPTS, AND THEN TURN THEM OFF
   sei              
   lda #%01111111   ; BIT 7 (OFF) MEANS THAT ANY 1S WRITTEN TO CIA ICRS TURN THOSE BITS OFF
   sta $dc0d        ;    CIA#1 INTERRUPT CONTROL REGISTER (IRC): DISABLE ALL INTERRUPTS
   sta $dd0d        ;    CIA#2 ICR: DISABLE ALL INTERRUPTS
   lda $dc0d        ; ACK (CLEAR) ANY PENDING CIA1 INTERRUPTS (READING CLEARS 'EM)
   lda $dd0d        ;    SAME FOR CIA2
   asl $d019        ; TOSS ANY PENDING VIC INTERRUPTS (WRITING CLEARS 'EM, VIA RMW MAGIC)
   ; HOOK INTERRUPT ROUTINE (NORMALLY POINTS TO $EA31)
   lda #<ASIDInterrupt 
   sta $0314
   lda #>ASIDInterrupt
   sta $0315 
   cli              ; RESTORE INTERRUPTS, HOOKING COMPLETE   
   
   lda #ASIDContIRQOn  ;enable the interrupt from TR
   sta ASIDContReg+IO1Port

ShowKeyboardCommands: ;including SID addresses
   jsr ClearASIDScreen
   lda #<MsgASIDPlayerCommands1
   ldy #>MsgASIDPlayerCommands1
   jsr PrintString 
   ;sid1 addr:
   lda smcSID1address+2 ;high byte
   ldx smcSID1address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands2
   ldy #>MsgASIDPlayerCommands2
   jsr PrintString 
   ;sid2 addr:
   lda smcSID2address+2 ;high byte
   ldx smcSID2address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands3
   ldy #>MsgASIDPlayerCommands3
   jsr PrintString 
   ;sid3 addr:
   lda smcSID3address+2 ;high byte
   ldx smcSID3address+1 ;low byte
   jsr PrintSIDaddress
   
   lda #<MsgASIDPlayerCommands4
   ldy #>MsgASIDPlayerCommands4
   jsr PrintString 
   ;print timer state:
   lda memFrameTimer
   asl ;double it to point to word
   tax
   lda TblMsgTimerState,x
   ldy TblMsgTimerState+1,x
   jsr PrintString
   
   lda #<MsgASIDPlayerCommands5
   ldy #>MsgASIDPlayerCommands5
   jsr PrintString    
   ; print buffer size
   lda memBufferSize
   and #ASIDContBufMask ;turn to zero based
   asl ;double it to point to word
   tax
   lda TblMsgBufferSize,x
   ldy TblMsgBufferSize+1,x
   jsr PrintString
   
   lda #<MsgASIDPlayerCommands6
   ldy #>MsgASIDPlayerCommands6
   jsr PrintString    
   
   
   inc smcScreenFull+1 ;set screen full flag
   ;end of ShowKeyboardCommands, continue to main loop...
   
ASIDMainLoop: 
 
;check queue for characters to send
   ldy memTextCircQueueTail 
   cpy memTextCircQueueHead
   beq RegIndicatorUpdate  ;skip if head==tail
   ;check screen full flag.
smcScreenFull
   ldx #$01 ;non-zero means screen is full
   beq +  
   jsr ClearASIDScreen
   ldy memTextCircQueueTail ;reload tail into Y
   ;print next character from queueu
+  lda memTextCircQueue,y
   inc memTextCircQueueTail ;increment tail

   cmp StartMsgToken
   bne +
   lda #<MsgASIDStart
   ldy #>MsgASIDStart
   jsr PrintString 
   jmp CheckForScreenFull
 
+  cmp StopMsgToken
   bne +
   lda #<MsgASIDStop
   ldy #>MsgASIDStop
   jsr PrintString 
   jmp CheckForScreenFull

+  jsr SendChar
   ;check cursor position if return char
   cmp #ChrReturn
   bne RegIndicatorUpdate
CheckForScreenFull
   sec
   jsr SetCursor ;read current to load x (row)
   cpx #23
   bmi RegIndicatorUpdate ;Finished ;skip if row is <23
   inc smcScreenFull+1 ;set screen full flag
   
 
RegIndicatorUpdate: 
;SID reg color & queue used update: 25 SID registers updated once per 10th/sec
   lda TODTenthSecBCD ;read 10ths releases latch
smcLastUpd
   cmp #0
   beq GetKeypress ;skip if tenths hasn't changed
   sta smcLastUpd+1
   ;inc SpinIndUnexpType  ;temp to check update cycle

;update Queue used indicator
   lda ASIDQueueUsed+IO1Port  ;0-31 representing how full the queue is
   tax
   lsr
   lsr
   lsr
   and #$03 ;shouldn't be needed, but just in case
   sta smcFineIndic+1 ;acc now 0-3
   
   lda #$f0 ;empty
   ldy #$03
smcFineIndic
   cpy #0
   beq +
   sta QueueUsedIndicator, y
   dey
   jmp smcFineIndic
   
+  txa ;original value
   and #$07  ;lower 3 bits only
   ora #$f0  ;custom char bar graph start
   sta QueueUsedIndicator, y

   lda #$f8 ;full
-  cpy #0
   beq +
   dey
   sta QueueUsedIndicator, y
   jmp -

;update register indicator colors     
+  ldx #0
regcolorupdate
   lda SIDRegColorStart,x
   and #$0f   ;only lower 4 bits are valid for color read
   beq nextsidreg   ;skip reg if 0 (black) already
   
   cmp #RegFirstColor ;color set by interrupt
   bne +
   lda #RegSecondColor ;RegFirstColor -> RegSecondColor
   jmp setcolor   
+
   lda #RegOffColor ;RegSecondColor (or anything else) -> Off (black)
 
setcolor 
   sta SIDRegColorStart,x
nextsidreg
   inx  ;next register
   cpx #25 ;25 registers 0-24
   bne regcolorupdate

 
;get/check key press
GetKeypress:
   jsr ScanKey ;needed since timer/raster interrupts are disabled
   jsr GetIn
   bne + 
   jmp ASIDMainLoop
   
+  cmp #'x'  ;Exit TR ASID player
   bne +  
   lda #ASIDContExit  ;turn off the interrupt and go back to main menu
   sta ASIDContReg+IO1Port
   ;C64 is resetting now, probably won't get far...
   ; IRQ back to default
   sei
   lda #<IRQDefault
   ldx #>IRQDefault
   sta $314   ;CINV, HW IRQ Int Lo
   stx $315   ;CINV, HW IRQ Int Hi
   lda #%10000001 
   sta $dc0d  ;CIA int ctl
   lda #0
   sta $d01a  ;irq enable
   inc $d019
   lda $dc0d  ;CIA int ctl
   cli 
   jsr SIDinit
   rts ;return to BASIC (if not TR main menu)

+  cmp #'s'  ;screen on/off
   bne +  
smcScreenOnOff
   lda #0
   bne TurnOff
   ;Turn screen back on
   dec smcScreenOnOff+1
   lda #$00   ;turn off the display 
   ldx #PokeBlack  ;set to black
   jmp SetScreen
TurnOff   
   inc smcScreenOnOff+1
   lda #$1b   ;turn the display back on
   ldx #BorderColor  ;set to regular/default
SetScreen
   sta $d011 
   stx BorderColorReg
   jmp ASIDMainLoop

+  cmp #'v'  ;voices off
   bne +  
   jsr SIDinit
   jmp ASIDMainLoop

+  cmp #'t'  ;Frame Timer state
   bne +  
   ldx memFrameTimer
   inx
   cpx #NumTimerStates
   bne ++
   ldx #$00 ;roll over
++ stx memFrameTimer
   stx ASIDContReg+IO1Port
   jmp ShowKeyboardCommands

+  cmp #'B'  ;Increase Buffer Size
   bne +  
   ldx memBufferSize
   inx
   cpx #ASIDContBufLastItem+1
   bne UpdateBufferSize
   ldx #ASIDContBufFirstItem ;roll over to first
   jmp UpdateBufferSize

+  cmp #'b'  ;Decrease Buffer Size
   bne +  
   ldx memBufferSize
   dex
   cpx #ASIDContBufFirstItem-1
   bne UpdateBufferSize
   ldx #ASIDContBufLastItem ;roll over to last
UpdateBufferSize  ;mem & control from x register
   stx memBufferSize
   stx ASIDContReg+IO1Port
   jmp ShowKeyboardCommands

+  cmp #'m'  ;mute toggle
   bne +  
   lda memPausePlay
   beq ++
   ;stream is off, unmute:
   dec memPausePlay
   lda #ASIDContIRQOn  ;enable the interrupt from TR
   sta ASIDContReg+IO1Port
   jsr SetMuteIndicator
   jmp ASIDMainLoop
   ;stream is on, mute it and kill voices:
++ inc memPausePlay
   lda #ASIDContIRQOff  ;disable the interrupt from TR
   sta ASIDContReg+IO1Port
   jsr SIDinit
   jsr SetMuteIndicator
   jmp ASIDMainLoop
   
+  cmp #'c'  ;Refresh/Clear Screen
   bne +  
   jsr ClearASIDScreen
   jmp ASIDMainLoop

+  cmp #'?'  ;Show Keyboard Commands
   bne + 
   jmp ShowKeyboardCommands   

+  cmp #'d'  ;Show Register Decoder
   bne +  
   jsr ClearASIDScreen
   lda #<MsgASIDPlayerDecoder
   ldy #>MsgASIDPlayerDecoder
   jsr PrintString 
   inc smcScreenFull+1 ;set screen full flag
   jmp ASIDMainLoop

+  cmp #'1'  ;1st SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID1addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID1addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  cmp #'2'  ;2nd SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID2addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID2addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  cmp #'3'  ;3rd SID address
   bne + 
   ;increment to next table entry, or wrap around
   ldx memSID3addrNum
   inx
   cpx memNumSIDaddresses
   bne ++
   ldx #0
++ stx memSID3addrNum
   jsr UpdateAllSIDAddress
   jmp ShowKeyboardCommands

+  jmp ASIDMainLoop


   
   !align $ff, 0 , 0  ;page align Text queue and interrupt to reduce index/branch times.

memTextCircQueue:
   !fill 256, $00	; reserve 256 bytes for circular text queue

ASIDInterrupt: 
;read the addr/type and data:
   ldx ASIDAddrReg+IO1Port
   ldy ASIDDataReg+IO1Port
   
;apply the addr/type and data:
   txa
   and #ASIDAddrType_Mask
   ;Acc holds msg type
   
   cmp #ASIDAddrType_SID1 ;SID1 Register write
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID1address
   sta $faca,x
   lda #RegFirstColor ;set the indicator color
   sta SIDRegColorStart,x
   inc SpinIndSID1Write
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_SID2 ;SID2 Register write
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID2address
   sta $facb,x
   ;lda #RegFirstColor ;set the indicator color
   ;sta SIDRegColorStart,x
   inc SpinIndSID2Write
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_SID3 ;SID3 Register write
   bne + 
   txa
   and #ASIDAddrAddr_Mask
   tax ;x now holds SID offset address
   tya ;acc now holds data to write
smcSID3address
   sta $facc,x
   ;lda #RegFirstColor ;set the indicator color
   ;sta SIDRegColorStart,x
   inc SpinIndSID3Write
   jmp ASIDIntFinished
   
+  cmp #ASIDAddrType_Start ;Start Message
   bne + 
   jsr SIDinit
   lda StartMsgToken
   jmp AddAccToCharQueue
   
+  cmp #ASIDAddrType_Stop ;Stop Message
   bne + 
   jsr SIDinit
   lda StopMsgToken
   jmp AddAccToCharQueue
   
+  cmp #ASIDAddrType_Char  ;Sending Character, add to queue
   bne + 
   tya
AddAccToCharQueue
   ldx memTextCircQueueHead
   sta memTextCircQueue,x
   inc memTextCircQueueHead ;increment head
   jmp ASIDIntFinished

+  cmp #ASIDAddrType_Error ; Packet error flagged by TR
   bne + 
   inc SpinIndPacketError
   jmp ASIDIntFinished

   ;unexpected type or skip received
+  inc SpinIndUnexpType
   
ASIDIntFinished:
   ;jmp IRQDefault    ; EXIT THROUGH THE KERNAL'S IRQ HANDLER ROUTINE
   jmp $ea81  ;jump to the *end* of the interrupt (pull YXA from the stack and RTI)


   !src "source\ASIDsupport.asm"


   !align $1f, 0 , 0  ;32 byte align to index within page.
memNoSID: ;set "none" sid writes to here
   !fill $20, $00	; reserve 32 bytes for "none" sid

   *=Charset
   !binary "bin\ASID-charset.bin"

EOF:
   !byte 0
   

   


