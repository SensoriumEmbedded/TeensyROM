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
{
   IOHandlerInit(IO1[rwRegNextIOHndlr]);
}

void IOHandlerInit(uint8_t NewIOHandler)
{
   SetMIDIHandlersNULL();
   MIDIRxIRQEnabled = false;
   MIDIRxBytesToSend = 0;
   rIORegMIDIStatus = 0;
   BigBufCount = 0;
   free(BigBuf);
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));
   
   if (NewIOHandler>=IOH_Num_Handlers)
   {
      Serial.println("***No IO handler loaded");
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
   midi1.setHandleNoteOff             (NULL); // 8x
   midi1.setHandleNoteOn              (NULL); // 9x
   midi1.setHandleAfterTouchPoly      (NULL); // Ax
   midi1.setHandleControlChange       (NULL); // Bx
   midi1.setHandleProgramChange       (NULL); // Cx
   midi1.setHandleAfterTouch          (NULL); // Dx
   midi1.setHandlePitchChange         (NULL); // Ex
   midi1.setHandleSystemExclusive     (NothingOnSystemExclusive); // F0   
   midi1.setHandleTimeCodeQuarterFrame(NULL); // F1
   midi1.setHandleSongPosition        (NULL); // F2
   midi1.setHandleSongSelect          (NULL); // F3
   midi1.setHandleTuneRequest         (NULL); // F6
   midi1.setHandleRealTimeSystem      (NULL); // F8-FF (except FD)
}
