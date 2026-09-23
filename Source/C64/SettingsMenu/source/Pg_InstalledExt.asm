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

WaitInstalledExtMenuKey:
   ;main wait loop
   jsr DisplayTime
   jsr GetIn
   beq WaitInstalledExtMenuKey

+  jsr CheckCommonKeys ;won't return if page changed or exit
   jmp WaitInstalledExtMenuKey

MsgInstalledExtMenu:
   !tx EscC,EscSourcesColor, ChrRvsOn, " Installed Extensions ", ChrReturn, ChrReturn

   !tx EscC,EscTimeColor,  " Not yet implemented.", ChrReturn, ChrReturn
   !tx EscC,EscSourcesColor, " Once TR+ Runtime Firmware Extensions ship,", ChrReturn
   !tx EscC,EscSourcesColor, " each installed extension's name, version", ChrReturn
   !tx EscC,EscSourcesColor, " and slot will be listed here, with an", ChrReturn
   !tx EscC,EscSourcesColor, " option to uninstall.", ChrReturn
   !tx 0
