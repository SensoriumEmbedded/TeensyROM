
; Copyright (c) 2025 Travis Smith
; Portions leveraged from: CCGMS Terminal
;  Copyright (c) 2016,2020, Craig Smith, alwyz. All rights reserved.
;  This project is licensed under the BSD 3-Clause License.
; RS232 SwiftLink/Turbo232 (MOS 6551 ACIA) Driver
;  based on Jeff Brown adaptation of Novaterm version


; calls from outside code:
;  sw_setup: Clears RX queue, sets def (2400) baud rate, Sets up Rx NMI vectors, turns on interrupts
;  sw_disable: Disable Rx NMI interrupts
;  sw_enable: Enable Rx NMI interrupts
;  sw_setbaud: Set Rx baud rate based on baud rate constant (below) stored in acc
;  sw_CharOut: Tx single char stored in acc
;  sw_StrOut: Send zero terminated char string stored in RAM (acc/Y pointer) to SwiftLink
;  sw_CharIn: Read a character from the Swiftlink receive queue to acc, or 0 if empty


;----------------------------------------------------------------------
; baud rate constants
; in this context, dictates how fast Rx NMIs arrive from Swiftlink
   SW_Baud_100    = 1
   SW_Baud_150    = 2
   SW_Baud_219    = 3
   SW_Baud_269    = 4
   SW_Baud_300    = 5
   SW_Baud_600    = 6
   SW_Baud_1200   = 7
   SW_Baud_2400   = 8
   SW_Baud_3600   = 9
   SW_Baud_4800   = 10
   SW_Baud_7200   = 11
   SW_Baud_9600   = 12
   SW_Baud_14400  = 13
   SW_Baud_19200  = 14
   SW_Baud_38400  = 15
   ;turbo-232: careful, very fast! Need Super CPU or similar
   SW_Baud_57600  = 18
   SW_Baud_115200 = 17
   SW_Baud_230400 = 16
   
   
   ; SwiftLink registers
   swift = $de00
   sw_data  = swift
   sw_stat  = swift+1
   sw_cmd   = swift+2
   sw_ctrl  = swift+3
   sw_turbo = swift+7


;----------------------------------------------------------------------
sw_disable:
   lda sw_cmd
   ora #%00000010 ; disable receive interrupt
   sta sw_cmd
   rts

;----------------------------------------------------------------------
sw_enable:
   lda sw_cmd
   and #%11111101 ; enable receive interrupt
   sta sw_cmd
   rts

;----------------------------------------------------------------------
sw_setup:
   sei
   lda #%00001001 ; set defaults, enable receive interrupt
   sta sw_cmd
   
   lda #SW_Baud_2400
   jsr sw_setbaud ;sets sw_ctrl reg and default baud rate

   lda #<nmisw
   ldx #>nmisw
   sta $0318 ; NMINV
   stx $0319

   ;clear Rx buffer pointers
   lda #0
   sta rtail
   sta rhead
   sta rused

   cli
   rts

;----------------------------------------------------------------------
; set baud rate based on baud rate constant stored in acc
sw_setbaud:
   cmp #SW_Baud_38400+1 
   bmi + ;skip if <= base sw max of 38400
   ;turbo setting
   and #%00000011
   sta sw_turbo
   lda#0 ;turbo enhanced mode
+  sta sw_ctrl
   rts
   
;----------------------------------------------------------------------
; Send character stored in acc to SwiftLink
; uses acc and X regs
; blocks until ready to transmit, and transmit completed
sw_CharOut:
   tax
   jsr swwait ; wait for Tx Ready, uses acc only
   stx sw_data
   jsr swwait ; wait for Tx Ready, uses acc only
   rts

;----------------------------------------------------------------------
; Send zero terminated char string stored in RAM to SwiftLink
;  usage: lda #<MsgM2SPolyMenu,  ldy #>MsgM2SPolyMenu,  jsr sw_StrOut 
;  blocks until ready to transmit, and transmit completed
sw_StrOut:
   sta smcStringAddr+1
   sty smcStringAddr+2
   ldy #0  ;Y will point to current offset throughout
smcStringAddr
   lda $fffe, y
   beq +
   jsr sw_CharOut  ;uses acc and X regs
   iny
   bne smcStringAddr
   inc smcStringAddr+2
   bne smcStringAddr ;roll-over protection
+  rts   

;----------------------------------------------------------------------
; Read a character from the Swiftlink receive queue, or 0 if empty
sw_CharIn:
   ldx rhead
   cpx rtail
   beq ++ ; skip (empty buffer)
   lda ribuf,x
   inc rhead
	dec rused
   
   ;could use a "paused" flag to skip this if not paused...
	ldx rused	; check buffer usage
	cpx #50		; against restart limit
	bpl +		; skip if larger than 50 (no change)
   tax
   lda sw_cmd
   and #%11111101 ; re-enable receive interrupt
   sta sw_cmd
   txa   
   
+  rts
++ lda #0
   rts

;----------------------------------------------------------------------
; NMI handler
nmisw:
   pha
   txa
   pha
   tya
   pha
   lda sw_stat
   and #%10000000 ; mask out all but "Swiftlink caused interrupt" bit
   beq +    ; get outta here if not from Swiftlink (disk access etc)
   lda sw_cmd
   ora #%00000010 ; disable receive interrupts
   sta sw_cmd
   lda sw_data ;read a byte from Rx
   ldx rtail
   sta ribuf,x ;store it in buffer
   inc rtail
   inc rused
   lda rused
   cmp #200 ; check byte count against tolerance
   bpl +    ; If full, leave Rx Int off (could set paused flag here)
   lda sw_cmd
   and #%11111101 ; re-enable receive interrupt
   sta sw_cmd
+  pla
   tay
   pla
   tax
   pla
   rti

;----------------------------------------------------------------------
swwait:  ;waits for transmit ready, uses acc only
   lda sw_cmd
   ora #%00001000 ; enable transmitter
   sta sw_cmd
   lda sw_stat
   and #%00110000
   beq swwait
   rts

;----------------------------------------------------------------------
rhead:
   !byte 0
rtail:
   !byte 0
rused:
   !byte 0

   ;!align $ff, 0 , 0  ;could page align Rx queue to reduce index times.
ribuf:
   !fill 256, $00	; reserve 256 bytes for circular Rx queue
