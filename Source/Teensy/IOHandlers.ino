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


void IOHandlerInitToNext()
{ //called after cart loaded, PRG x-fer finished, or exit to basic (rsIOHWinit)
   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy && TeensyROMMenu[SelItemFullIdx].IOHndlrAssoc != IOH_None)
   {
      Serial.println("IO Handler set by Teensy Menu");
      IOHandlerInit(TeensyROMMenu[SelItemFullIdx].IOHndlrAssoc); //Could use MenuSource, same thing as TeensyROMMenu here
   }
   else
   {
      IOHandlerInit(IO1[rwRegNextIOHndlr]);
   }
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
      Serial.println("***IOHandler out of range");
      return;
   }
   
   Serial.printf("Loading IO handler: %s\n", IOHandler[NewIOHandler]->Name);
   
   if (IOHandler[NewIOHandler]->InitHndlr != NULL) IOHandler[NewIOHandler]->InitHndlr();
   
   Serial.flush();
   CurrentIOHandler = NewIOHandler;
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
