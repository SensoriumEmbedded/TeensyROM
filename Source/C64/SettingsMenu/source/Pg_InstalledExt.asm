; MIT License
;
; Copyright (c) 2026 Travis Smith
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



InstalledExtMenu:
   jsr CommonInit ;print banner and common keys/page#

   lda #<MsgInstalledExtMenu
   ldy #>MsgInstalledExtMenu
   jsr PrintString

   ;update static settings

ShowInstalledExtSettings:
   ;update dynamic settings
   ;The firmware answers this in every build: a build without the loader, and a
   ;board with an empty slot, both say so here rather than leaving the row blank.
   ;PrintFileName ends at PrintSerialStringLoaded and selects nothing, which is safe
   ;because MakeExtHostStr calls SelectSerialStringBuf: it leaves the read pointed at
   ;SerialStringBuf and rewound. A handler that only fills the buffer -- as this one
   ;used to -- leaves the read where the banner's version string ended, and the row
   ;comes out empty.
   lda #rCtlMakeExtHostStrWAIT
   ldx #5 ;row
   ldy #3 ;col
   jsr PrintFileName

WaitInstalledExtMenuKey:
   ;main wait loop
   jsr DisplayTime
   jsr GetIn
   beq WaitInstalledExtMenuKey

+  cmp #'u'  ;Uninstall the extension host
   bne +
   jsr PrintBanner
   lda TblEscC+EscSourcesColor
   sta $0286  ;set text color
   lda #<MsgConfirmUninstall
   ldy #>MsgConfirmUninstall
   jsr PrintString
   ;Name the host being removed, so a confirmation is about a thing rather than
   ;about a menu key.
   lda #rCtlMakeExtHostStrWAIT
   ldx #6 ;row
   ldy #3 ;col
   jsr PrintFileName
   ;Place every row below rather than counting returns from wherever the line above
   ;stopped.  The host line is up to 37 characters starting at column 3, so the
   ;longest of them ends in column 39, and the screen editor then wraps the cursor
   ;to the next row on its own -- two returns from there land a row lower than two
   ;returns from a short name.  Measured on a TR+: "None installed." put this prompt
   ;on row 8 and a clamped "WIDEHOSTNAME  ABI 2  services $fffff>" put it on row 9.
   ;A build with no extension loader says "No extension loader in this firmware",
   ;which is 37 characters as well.
   ldx #8 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   lda #<MsgConfirmPrompt
   ldy #>MsgConfirmPrompt
   jsr PrintString
   ;DisplayTime, as every other wait loop on this menu does.  PrintFileName's WAIT
   ;writes "Waiting:" over the clock at row 1 column 29 and nothing takes it back,
   ;so a bare GetIn loop leaves the board reading as busy for as long as the user
   ;takes to answer.
-  jsr DisplayTime
   jsr GetIn
   beq -
   cmp #'y'
   beq +++
   jmp InstalledExtMenu ;anything but y backs out, including stop
+++
   lda TblEscC+EscSourcesColor
   sta $0286  ;set text color
   ;Put the cursor back under the prompt before handing over: DisplayTime left it at
   ;the clock, and the firmware's reply prints wherever it is.  SendMsgPrintfln leads
   ;with a return, so row 9 puts that reply on row 10, and AnyKeyMsgWait's own leading
   ;return puts the key prompt on row 11 -- which is where both landed when the prompt
   ;text happened to end there.
   ldx #9 ;row
   ldy #0 ;col
   clc
   jsr SetCursor
   ;Does not return when there was a host to remove: the firmware holds the 6510
   ;in reset for the sector erase and reboots, so the C64 restarts into the main
   ;menu and the record is reported there. It does return when the slot was
   ;already empty, and then the firmware's message is the whole answer.
   lda #rCtlUninstallExtHostWAIT
   sta wRegControl+IO1Port
   jsr WaitForTRDots
   jsr AnyKeyMsgWait
   jmp InstalledExtMenu ;force to reprint all

+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitInstalledExtMenuKey

MsgInstalledExtMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Installed Extensions ", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " Extension host in the firmware slot:", ChrReturn, ChrReturn
   !tx ChrReturn
   !tx EscC,EscArgSpaces+2, EscC,EscOptionColor, ChrFillRight, ChrRvsOn, "u", ChrRvsOff, ChrFillLeft, EscC,EscSourcesColor,   "Uninstall the extension host", ChrReturn, ChrReturn

   !tx EscC,EscSourcesColor, " Uninstalling clears the tag that makes", ChrReturn
   ;Keep every line below 40 visible columns. A line that fills the row exactly makes
   ;the screen editor advance on its own, and the ChrReturn here then advances again --
   ;which put a blank row in the middle of this sentence on a real screen.
   !tx EscC,EscSourcesColor, " the slot bootable. The host image", ChrReturn
   !tx EscC,EscSourcesColor, " stays in flash, unreferenced, until", ChrReturn
   !tx EscC,EscSourcesColor, " the next install overwrites it.", ChrReturn, ChrReturn
   !tx EscC,EscTimeColor,  " The TeensyROM reboots to do it.", ChrReturn
   !tx 0

MsgConfirmUninstall:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Uninstall extension host ", ChrRvsOff, ChrReturn, ChrReturn
   !tx 0
MsgConfirmPrompt:
   ;No leading returns: the caller places this with SetCursor, because where the
   ;host line above it stops is not fixed.
   !tx EscC,EscOptionColor, " Remove it?  ", ChrRvsOn, "y", ChrRvsOff, " to remove, any other key to keep"
   !tx 0
