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


//IO1 Handler Init _________________________________________________________________________________________

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
   CycleCountdown = 0;
   free(BigBuf);
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));
   
   if (NewIOHandler>=IOH_Num_Handlers)
   {
      Serial.println("***No IO handler loaded");
      return;
   }
   
   Serial.printf("Loading IO handler: %s\n", IOHandlerName[NewIOHandler]);
   
   switch(NewIOHandler)
   {
      case IOH_SwiftLink:

         break;
      case IOH_EpyxFastLoad:   
         //EpyxFastLoadCycleReset;
         CycleCountdown=100000; //give extra time at start
         SetExROMAssert;
         break;
      case IOH_TeensyROM:  
         IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
         //MIDI handlers for MIDI2SID:
         midi1.setHandleNoteOff             (M2SOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (M2SOnNoteOn);              // 9x
         midi1.setHandleControlChange       (M2SOnControlChange);       // Bx
         midi1.setHandlePitchChange         (M2SOnPitchChange);         // Ex
         break;
      case IOH_MIDI_Datel:
      case IOH_MIDI_Sequential:
      case IOH_MIDI_Passport:
      case IOH_MIDI_NamesoftIRQ:
         for (uint8_t ContNum=0; ContNum < NumMIDIControls;) MIDIControlVals[ContNum++]=63;
         midi1.setHandleNoteOff             (HWEOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (HWEOnNoteOn);              // 9x
         midi1.setHandleAfterTouchPoly      (HWEOnAfterTouchPoly);      // Ax
         midi1.setHandleControlChange       (HWEOnControlChange);       // Bx
         midi1.setHandleProgramChange       (HWEOnProgramChange);       // Cx
         midi1.setHandleAfterTouch          (HWEOnAfterTouch);          // Dx
         midi1.setHandlePitchChange         (HWEOnPitchChange);         // Ex
         midi1.setHandleSystemExclusive     (HWEOnSystemExclusive);     // F0 *not implemented
         midi1.setHandleTimeCodeQuarterFrame(HWEOnTimeCodeQuarterFrame);// F1
         midi1.setHandleSongPosition        (HWEOnSongPosition);        // F2
         midi1.setHandleSongSelect          (HWEOnSongSelect);          // F3
         midi1.setHandleTuneRequest         (HWEOnTuneRequest);         // F6
         midi1.setHandleRealTimeSystem      (HWEOnRealTimeSystem);      // F8-FF (except FD)
         // not catching F0, F4, F5, F7 (end of SysEx), and FD         
         wIORegAddrMIDIControl  = MidiRegs[NewIOHandler-IOH_MIDI_Datel][0];
         rIORegAddrMIDIStatus   = MidiRegs[NewIOHandler-IOH_MIDI_Datel][1];
         wIORegAddrMIDITransmit = MidiRegs[NewIOHandler-IOH_MIDI_Datel][2];
         rIORegAddrMIDIReceive  = MidiRegs[NewIOHandler-IOH_MIDI_Datel][3];
         NewIOHandler = IOH_MIDI_Datel; //all 4 are the same after regs are set
         break;
      case IOH_Debug:
         midi1.setHandleNoteOff             (DbgOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (DbgOnNoteOn);              // 9x
         midi1.setHandleAfterTouchPoly      (DbgOnAfterTouchPoly);      // Ax
         midi1.setHandleControlChange       (DbgOnControlChange);       // Bx
         midi1.setHandleProgramChange       (DbgOnProgramChange);       // Cx
         midi1.setHandleAfterTouch          (DbgOnAfterTouch);          // Dx
         midi1.setHandlePitchChange         (DbgOnPitchChange);         // Ex
         midi1.setHandleSystemExclusive     (DbgOnSystemExclusive);     // F0   
         midi1.setHandleTimeCodeQuarterFrame(DbgOnTimeCodeQuarterFrame);// F1
         midi1.setHandleSongPosition        (DbgOnSongPosition);        // F2
         midi1.setHandleSongSelect          (DbgOnSongSelect);          // F3
         midi1.setHandleTuneRequest         (DbgOnTuneRequest);         // F6
         midi1.setHandleRealTimeSystem      (DbgOnRealTimeSystem);      // F8-FF (except FD)
         // not catching F4, F5, F7 (end of SysEx), and FD                  
         break;
   }
   
   IOHandler = NewIOHandler;
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
