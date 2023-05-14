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

#define NUM_VOICES 3
const char NoteName[12][3] ={" a","a#"," b"," c","c#"," d","d#"," e"," f","f#"," g","g#"};

struct stcVoiceInfo
{
  bool Available;
  uint16_t  NoteNumUsing;
};

stcVoiceInfo Voice[NUM_VOICES]=
{  //voice table for poly synth
   true, 0,
   true, 0,
   true, 0,
};

//MIDI input handlers for MIDI2SID _________________________________________________________________________

void M2SOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{   
   note+=3; //offset to A centered from C
   int VoiceNum = FindFreeVoice();
   if (VoiceNum<0)
   {
      IO1[rRegSIDOutOfVoices]='x';
      #ifdef DbgMsgs_M2S
       Serial.println("Out of Voices!");  
      #endif
      return;
   }
   
   float Frequency = 440*pow(1.059463094359,note-60);  
   uint32_t RegVal = Frequency*16777216/NTSCBusFreq;
   
   if (RegVal > 0xffff) 
   {
      #ifdef DbgMsgs_M2S
       Serial.println("Too high!");
      #endif
      return;
   }
   
   Voice[VoiceNum].Available = false;
   Voice[VoiceNum].NoteNumUsing = note;
   IO1[rRegSIDFreqLo1+VoiceNum*7] = RegVal;  //7 regs per voice
   IO1[rRegSIDFreqHi1+VoiceNum*7] = (RegVal>>8);
   IO1[rRegSIDVoicCont1+VoiceNum*7] |= 0x01; //start ADSR
   IO1[rRegSIDStrStart+VoiceNum*4+0]=NoteName[note%12][0];
   IO1[rRegSIDStrStart+VoiceNum*4+1]=NoteName[note%12][1];
   IO1[rRegSIDStrStart+VoiceNum*4+2]='0'+note/12;

   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Note On, ch=");
    Serial.print(channel);
    Serial.print(", voice=");
    Serial.print(VoiceNum);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.print(", reg ");
    Serial.print(IO1[rRegSIDFreqHi1  ]);
    Serial.print(":");
    Serial.print(IO1[rRegSIDFreqLo1  ]);
    Serial.println();
   #endif
}

void M2SOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
   note+=3; //offset to A centered from C
   IO1[rRegSIDOutOfVoices]=' ';
   int VoiceNum = FindVoiceUsingNote(note);
   
   if (VoiceNum<0)
   {
      #ifdef DbgMsgs_M2S
       Serial.print("No voice using note ");  
       Serial.println(note);  
      #endif
      return;
   }
   Voice[VoiceNum].Available = true;
   IO1[rRegSIDVoicCont1+VoiceNum*7] &= 0xFE; //stop note
   IO1[rRegSIDStrStart+VoiceNum*4+0]='-';
   IO1[rRegSIDStrStart+VoiceNum*4+1]='-';
   IO1[rRegSIDStrStart+VoiceNum*4+2]=' ';

   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Note Off, ch=");
    Serial.print(channel);
    Serial.print(", voice=");
    Serial.print(VoiceNum);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.println();
   #endif
}

void M2SOnControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
   
   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", NewVal=");
    Serial.print(NewVal);
    Serial.println();
   #endif
}

void M2SOnPitchChange(uint8_t channel, int pitch) 
{

   #ifdef DbgMsgs_M2S
    Serial.print("Pitch Change, ch=");
    Serial.print(channel, DEC);
    Serial.print(", pitch=");
    Serial.println(pitch, DEC);
    Serial.printf("     0-6= %02x, 7-13=%02x\n", pitch & 0x7f, (pitch>>7) & 0x7f);
   #endif
}

void NothingOnSystemExclusive(uint8_t *data, unsigned int size) // F0 SysEx single call, message larger than buffer is truncated
{
   //Setting handler to NULL creates ambiguous error
}

int FindVoiceUsingNote(int NoteNum)
{
  for (int VoiceNum=0; VoiceNum<NUM_VOICES; VoiceNum++)
  {
    if(Voice[VoiceNum].NoteNumUsing == NoteNum && !Voice[VoiceNum].Available) return (VoiceNum);  
  }
  return (-1);
}

int FindFreeVoice()
{
  for (int VoiceNum=0; VoiceNum<NUM_VOICES; VoiceNum++)
  {
    if(Voice[VoiceNum].Available) return (VoiceNum);  
  }
  return (-1);
}



//MIDI input handlers for HW Emulation _________________________________________________________________________
//Only called if MIDIRxBytesToSend==0 (No data waiting)

void HWEOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)  //8x
{
   MIDIRxBuf[2] = 0x80 | channel;
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)   //9x
{
   MIDIRxBuf[2] = 0x90 | channel;
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnAfterTouchPoly(uint8_t channel, uint8_t note, uint8_t velocity) // Ax
{
   MIDIRxBuf[2] = 0xa0 | channel;
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnControlChange(uint8_t channel, uint8_t control, uint8_t value)  //Bx
{
   if (value==64) return; //sends ref first, always 64 so just assume it
   control &= (NumMIDIControls-1);
   
   int NewVal = MIDIControlVals[control] + value - 64;
   if (NewVal<0) NewVal=0;
   if (NewVal>127) NewVal=127;
   MIDIControlVals[control] = NewVal;
      
   MIDIRxBuf[2] = 0xb0 | channel;
   MIDIRxBuf[1] = control;
   MIDIRxBuf[0] = NewVal;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnProgramChange(uint8_t channel, uint8_t program) // Cx
{
   MIDIRxBuf[1] = 0xc0 | channel;
   MIDIRxBuf[0] = program;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnAfterTouch(uint8_t channel, uint8_t pressure)  // Dx
{   
   MIDIRxBuf[1] = 0xd0 | channel;
   MIDIRxBuf[0] = pressure;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnPitchChange(uint8_t channel, int pitch)  //Ex
{
   //-8192 to 8192, returns to 0 always
   pitch+=8192;
   
   MIDIRxBuf[2] = 0xe0 | channel;
   MIDIRxBuf[1] = pitch & 0x7f;
   MIDIRxBuf[0] = (pitch>>7) & 0x7f;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnSystemExclusive(uint8_t *data, unsigned int size) // F0 SysEx single call, message larger than buffer is truncated
{
   //need a bigger buffer and lots of time, not forwarding for now
}

void HWEOnTimeCodeQuarterFrame(uint8_t data)  // F1
{
   MIDIRxBuf[1] = 0xf1;
   MIDIRxBuf[0] = data;  //won't have bit 7 set
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnSongPosition(uint16_t beats)        // F2
{
   MIDIRxBuf[2] = 0xf2;
   MIDIRxBuf[1] = beats & 0x7f; //not sure if this is correct format?
   MIDIRxBuf[0] = (beats >> 7) & 0x7f;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnSongSelect(uint8_t songnumber)      // F3
{
   MIDIRxBuf[1] = 0xf3;
   MIDIRxBuf[0] = songnumber;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnTuneRequest(void)                   // F6
{
   MIDIRxBuf[0] = 0xf6;
   MIDIRxBytesToSend = 1;
   SetMidiIRQ();
}

void HWEOnRealTimeSystem(uint8_t realtimebyte)     // F8-FF (except FD)
{
   MIDIRxBuf[0] = realtimebyte;
   MIDIRxBytesToSend = 1;
   SetMidiIRQ();
}

void SetMidiIRQ()
{
   if(MIDIRxIRQEnabled)
   {
      rIORegMIDIStatus |= MIDIStatusRxFull | MIDIStatusIRQReq; 
      SetIRQAssert;
   }
   else
   {
      MIDIRxBytesToSend = 0;
      if ((MIDIRxBuf[0] & 0xf0) != 0xf0) Printf_dbgMIDI("IRQ off\n"); //don't print on real-time inputs (there are lots)
   }
}


//MIDI input handlers for Debug _________________________________________________________________________

void DbgOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
   Serial.printf("8x Note Off, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

void DbgOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{   
   Serial.printf("9x Note On, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

void DbgOnAfterTouchPoly(uint8_t channel, uint8_t note, uint8_t velocity) // Ax
{
   Serial.printf("Ax After Touch Poly, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

void DbgOnControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
   Serial.printf("Bx Control Change, ch=%d, control=%d, NewVal=%d\n", channel, control, value);
}

void DbgOnProgramChange(uint8_t channel, uint8_t program) // Cx
{   
   Serial.printf("Cx Program Change, ch=%d, program=%d\n", channel, program);
}

void DbgOnAfterTouch(uint8_t channel, uint8_t pressure)  // Dx
{   
   Serial.printf("Dx After Touch, ch=%d, pressure=%d\n", channel, pressure);
}

void DbgOnPitchChange(uint8_t channel, int pitch) 
{
   Serial.printf("Ex Pitch Change, ch=%d, (int)pitch=%d\n", channel, pitch);
}

void DbgOnSystemExclusive(uint8_t *data, unsigned int size) // F0 SysEx single call, message larger than buffer is truncated
{
   Serial.printf("F0 SysEx, (int)size=%d, (hex)data=", size);
   for(uint16_t Cnt=0; Cnt<size; Cnt++) Serial.printf(" %02x", data[Cnt]);
   Serial.println();
}

void DbgOnTimeCodeQuarterFrame(uint8_t data)  // F1
{
   Serial.printf("F1 TimeCodeQuarterFrame, data=%d\n", data);
   //could decode this, see example
}

void DbgOnSongPosition(uint16_t beats)        // F2
{
   Serial.printf("F2 Song Position, (uint)beats=%d\n", beats);
}

void DbgOnSongSelect(uint8_t songnumber)      // F3
{
   Serial.printf("F3 Song Select, songnumber=%d\n", songnumber);
}

void DbgOnTuneRequest(void)                   // F6
{
   Serial.printf("F6 TuneRequest\n");
}

void DbgOnRealTimeSystem(uint8_t realtimebyte)     // F8-FF (except FD)
{
   Serial.printf("%02x Real Time: ", realtimebyte);
   switch(realtimebyte)
   {
      case 0xF8:
         Serial.print("Timing Clock");
         break;
      case 0xF9:
         Serial.print("Measure End");
         break;
      case 0xFA:
         Serial.print("Start");
         break;
      case 0xFB:
         Serial.print("Continue");
         break;
      case 0xFC:
         Serial.print("Stop");
         break;
      case 0xFE:
         Serial.print("Active Sensing");
         break;
      case 0xFF:
         Serial.print("Reset");
         break;
      default:
         Serial.print("Unknown");
         break;
   }         
   Serial.println();
}

