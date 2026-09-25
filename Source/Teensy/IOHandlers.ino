// MIT License
// 
// Copyright (c) 2023 Travis Smith
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, copy, modify, merge, publish, 
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
// the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

void IOHandlerNextInit()
{
   Printf_dbg("Default IO Handler\n");
   IOHandlerInit(IO1[rwRegNextIOHndlr]);
}

void IOHandlerSelectInit()
{ //called after cart loaded, PRG x-fer finished, or exit to basic (rsIOHWSelInit)
   PendingfBusSnoop = NULL; //clean slate for whatever InitHndlr below is about to stage
   //An out-of-range selection used to feed a byte read past the end of the menu straight into
   //IOHandlerInit, which rejects it and returns early -- leaving the PRG-load handshake unset
   //and the C64 polling rRegIOHSwapPoll with no timeout.  Fall back to the stored handler,
   //which the rwRegNextIOHndlr write path already clamps.
   const StructMenuItem* Item = MenuItemSel();
   //Falling back is the right action, but say so.  "This item has no handler assigned" and
   //"the selection no longer names an item at all" take the same branch and are not the same
   //state -- only one of them means something went inconsistent.  Main loop, so printing is
   //free; Serial rather than Printf_dbg, which a release build compiles away.
   if (Item == NULL) Serial.printf("Menu sel out of range, using stored IO handler\n");
   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy && Item != NULL && Item->IOHndlrAssoc != IOH_None)
   {
      Printf_dbg("IO Handler set by Teensy Menu\n");
      IOHandlerInit(Item->IOHndlrAssoc);
   }
   else IOHandlerNextInit();
}

void IOHandlerInit(uint8_t NewIOHandler)
{ //called from above and directly from SetUpMainMenuROM
   SetMIDIHandlersNULL();
   MIDIRxIRQEnabled = false;
   MIDIRxBytesToSend = 0;
   rIORegMIDIStatus = 0;
   BigBufCount = 0;
   
   if (NewIOHandler>=IOH_Num_Handlers)
   {
      //Rejecting the handler is right; returning early was not.  The handoff below is the only
      //thing that ever releases the PRG-load handshake, and the C64's poll of rRegIOHSwapPoll
      //(PRGLoadStartReloc.s) is a bare compare-and-loop with no timeout -- so skipping it left
      //the C64 spinning on rihsBusy with no exit but a power cycle.  Fall through, don't return.
      //Serial, not Printf_dbg: a release build compiles Printf_dbg to nothing, and this line is
      //the only record that a handler index went out of range.
      Serial.printf("***IOHandler out of range: %d\n", NewIOHandler);
   }
   else
   {
      Serial.printf("Loading IO handler: %s\n", IOHandler[NewIOHandler]->Name);

      if (IOHandler[NewIOHandler]->InitHndlr != NULL) IOHandler[NewIOHandler]->InitHndlr();

      Serial.flush();
      CurrentIOHandler = NewIOHandler;
   }

   //PRG-load handshake handoff (see HandshakeSnoop in IOH_TeensyROM.c):
   //if the handshake currently owns fBusSnoop, let its own completion-read do the handoff later;
   //otherwise (e.g. a cart loaded directly from the menu, no handshake involved) apply it now,
   //since nothing else will.  One exit, so a rejected handler releases it too.
   if (fBusSnoop == &HandshakeSnoop) HandshakeReady = true;
   else fBusSnoop = PendingfBusSnoop;
}

// F0 SysEx single call, message larger than buffer is truncated
void NothingOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   //Setting handler to NULL creates ambiguous error
}

void SetMIDIHandlersNULL()
{
   usbHostMIDI.setHandleNoteOff             (NULL); // 8x
   usbHostMIDI.setHandleNoteOn              (NULL); // 9x
   usbHostMIDI.setHandleAfterTouchPoly      (NULL); // Ax
   usbHostMIDI.setHandleControlChange       (NULL); // Bx
   usbHostMIDI.setHandleProgramChange       (NULL); // Cx
   usbHostMIDI.setHandleAfterTouch          (NULL); // Dx
   usbHostMIDI.setHandlePitchChange         (NULL); // Ex
   usbHostMIDI.setHandleSystemExclusive     (NothingOnSystemExclusive); // F0   
   usbHostMIDI.setHandleTimeCodeQuarterFrame(NULL); // F1
   usbHostMIDI.setHandleSongPosition        (NULL); // F2
   usbHostMIDI.setHandleSongSelect          (NULL); // F3
   usbHostMIDI.setHandleTuneRequest         (NULL); // F6
   usbHostMIDI.setHandleRealTimeSystem      (NULL); // F8-FF (except FD)

   usbDevMIDI.setHandleNoteOff              (NULL); // 8x
   usbDevMIDI.setHandleNoteOn               (NULL); // 9x
   usbDevMIDI.setHandleAfterTouchPoly       (NULL); // Ax
   usbDevMIDI.setHandleControlChange        (NULL); // Bx
   usbDevMIDI.setHandleProgramChange        (NULL); // Cx
   usbDevMIDI.setHandleAfterTouch           (NULL); // Dx
   usbDevMIDI.setHandlePitchChange          (NULL); // Ex
   usbDevMIDI.setHandleSystemExclusive      (NothingOnSystemExclusive); // F0   
   usbDevMIDI.setHandleTimeCodeQuarterFrame (NULL); // F1
   usbDevMIDI.setHandleSongPosition         (NULL); // F2
   usbDevMIDI.setHandleSongSelect           (NULL); // F3
   usbDevMIDI.setHandleTuneRequest          (NULL); // F6
   usbDevMIDI.setHandleRealTimeSystem       (NULL); // F8-FF (except FD)
}
