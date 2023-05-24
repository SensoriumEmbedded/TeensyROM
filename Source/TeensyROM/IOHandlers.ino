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

void IO1HWinitToNext()
{
   IO1HWinit(IO1[rwRegNextIO1Hndlr]);
}

void IO1HWinit(uint8_t NewIO1Handler)
{
   SetMIDIHandlersNULL();
   MIDIRxIRQEnabled = false;
   MIDIRxBytesToSend = 0;
   rIORegMIDIStatus = 0;
   BigBufCount = 0;
   free(BigBuf);
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));

   switch(NewIO1Handler)
   {
      case IOH_SwiftLink:
         Serial.println("SwiftLink IO1 handler ready");
         break;
      case IOH_TeensyROM:  
         //MIDI handlers for MIDI2SID:
         IO1[rwRegNextIO1Hndlr] = EEPROM.read(eepAdNextIO1Hndlr);  //in case it was over-ridden by .crt
         midi1.setHandleNoteOff             (M2SOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (M2SOnNoteOn);              // 9x
         midi1.setHandleControlChange       (M2SOnControlChange);       // Bx
         midi1.setHandlePitchChange         (M2SOnPitchChange);         // Ex
         Serial.println("TeensyROM/MIDI2SID IO1 handler ready");
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
         SetMIDIRegAddrs(NewIO1Handler - IOH_MIDI_Datel);
         NewIO1Handler = IOH_MIDI_Datel; //all 4 are the same after regs are set
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
         Serial.println("Debug IO1 handler ready");
         break;
      default:
         Serial.println("No IO1 handler loaded");
         break;
   }
   
   IO1Handler = NewIO1Handler;
}


void SetMIDIRegAddrs(uint8_t MIDI_ID)
{  
   //these must match enum IOHandlers order/qty
   char sMIDIType[][15] = {
      "Datel/Siel", 
      "Sequential", 
      "Passport/Sent", 
      "Namesoft IRQ",
      };

   uint8_t MidiRegs[][4] = {
      //wControl, rStatus, wTransmit, rReceive $de00+
      4,6,5,7,  // Datel/Siel
      0,2,1,3,  // Sequential
      8,8,9,9,  // Passport/Sent
      0,2,1,3,  // Namesoft IRQ (same as seq, no NMI)
   };

   wIORegAddrMIDIControl  = MidiRegs[MIDI_ID][0];
   rIORegAddrMIDIStatus   = MidiRegs[MIDI_ID][1];
   wIORegAddrMIDITransmit = MidiRegs[MIDI_ID][2];
   rIORegAddrMIDIReceive  = MidiRegs[MIDI_ID][3];

   Serial.printf("%s MIDI IO1 handler ready\n", sMIDIType[MIDI_ID]);
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
