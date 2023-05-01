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

void M2SOnNoteOn(byte channel, byte note, byte velocity)
{   
   note+=3; //offset to A centered from C
   int VoiceNum = FindFreeVoice();
   if (VoiceNum<0)
   {
      IO1[rRegSIDOutOfVoices]='x';
      #ifdef DebugMessages
       Serial.println("Out of Voices!");  
      #endif
      return;
   }
   
   float Frequency = 440*pow(1.059463094359,note-60);  
   uint32_t RegVal = Frequency*16777216/NTSCBusFreq;
   
   if (RegVal > 0xffff) 
   {
      #ifdef DebugMessages
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

   #ifdef DebugMessages
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

void M2SOnNoteOff(byte channel, byte note, byte velocity)
{
   note+=3; //offset to A centered from C
   IO1[rRegSIDOutOfVoices]=' ';
   int VoiceNum = FindVoiceUsingNote(note);
   
   if (VoiceNum<0)
   {
      #ifdef DebugMessages
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

   #ifdef DebugMessages
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

void M2SOnControlChange(byte channel, byte control, byte value)
{
   
   #ifdef DebugMessages
    Serial.print("MIDI Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", NewVal=");
    Serial.print(NewVal);
    Serial.println();
   #endif
}

void M2SOnPitchChange(byte channel, int pitch) 
{

   #ifdef DebugMessages
    Serial.print("Pitch Change, ch=");
    Serial.print(channel, DEC);
    Serial.print(", pitch=");
    Serial.println(pitch, DEC);
    Serial.printf("     0-6= %02x, 7-13=%02x\n", pitch & 0x7f, (pitch>>7) & 0x7f);
   #endif
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

void HWEOnNoteOn(byte channel, byte note, byte velocity)
{
   rIORegMIDIReceiveBuf[2] = 0x90 | channel;
   rIORegMIDIReceiveBuf[1] = note;
   rIORegMIDIReceiveBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   rIORegMIDIStatus = 0x81; //Interrupt Request + Receive Data Register Full
   SetIRQAssert;
}

void HWEOnNoteOff(byte channel, byte note, byte velocity)
{
   rIORegMIDIReceiveBuf[2] = 0x80 | channel;
   rIORegMIDIReceiveBuf[1] = note;
   rIORegMIDIReceiveBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   rIORegMIDIStatus = 0x81; //Interrupt Request + Receive Data Register Full
   SetIRQAssert;
}

void HWEOnControlChange(byte channel, byte control, byte value)
{
   if (value==64) return; //sends ref first, always 64 so just assume it
   control &= (NumMIDIControls-1);
   
   int NewVal = MIDIControlVals[control] + value - 64;
   if (NewVal<0) NewVal=0;
   if (NewVal>127) NewVal=127;
   MIDIControlVals[control] = NewVal;
      
   rIORegMIDIReceiveBuf[2] = 0xb0 | channel;
   rIORegMIDIReceiveBuf[1] = control;
   rIORegMIDIReceiveBuf[0] = NewVal;
   MIDIRxBytesToSend = 3;
   rIORegMIDIStatus = 0x81; //Interrupt Request + Receive Data Register Full
   SetIRQAssert;
}

void HWEOnPitchChange(byte channel, int pitch) 
{
   //-8192 to 8192, returns to 0 always
   pitch+=8192;
   
   rIORegMIDIReceiveBuf[2] = 0xe0 | channel;
   rIORegMIDIReceiveBuf[1] = pitch & 0x7f;
   rIORegMIDIReceiveBuf[0] = (pitch>>7) & 0x7f;
   MIDIRxBytesToSend = 3;
   rIORegMIDIStatus = 0x81; //Interrupt Request + Receive Data Register Full
   SetIRQAssert;
}

//MIDI input handlers for Debug _________________________________________________________________________

void DbgOnNoteOn(byte channel, byte note, byte velocity)
{   
    Serial.print("Note On, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.println(velocity);
}

void DbgOnNoteOff(byte channel, byte note, byte velocity)
{
    Serial.print("Note Off, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.println(velocity);
}

void DbgOnControlChange(byte channel, byte control, byte value)
{
    Serial.print("Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", NewVal=");
    Serial.println(value);
}

void DbgOnPitchChange(byte channel, int pitch) 
{
    Serial.print("Pitch Change, ch=");
    Serial.print(channel);
    Serial.print(", pitch=");
    Serial.println(pitch);
}