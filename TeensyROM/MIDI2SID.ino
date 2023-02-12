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

void OnNoteOn(byte channel, byte note, byte velocity)
{
   //const char NoteName[12][3] ={" A","A#"," B"," C","C#"," D","D#"," E"," F","F#"," G","G#"};

   int VoiceNum = FindFreeVoice();
   if (VoiceNum<0)
   {
#ifdef DebugMessages
      Serial.println("Out of Voices!");  
#endif
      return;
   }
   
   float Frequency = 440*pow(1.059463094359,note-60+3);  //C->A centered freq conv
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

void OnNoteOff(byte channel, byte note, byte velocity)
{
   
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
   IO1[rRegSIDVoicCont1+VoiceNum*7] &= 0xFE;

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

void OnControlChange(byte channel, byte control, byte value)
{

#ifdef DebugMessages
   Serial.print("MIDI Control Change, ch=");
   Serial.print(channel);
   Serial.print(", control=");
   Serial.print(control);
   Serial.print(", value=");
   Serial.print(value);
   Serial.println();
#endif
}

void myPitchChange(byte channel, int pitch) 
{
  //chan 1 (ignoring), -8192 to 8192, returns to 0 always!
 
#ifdef DebugMessages
   Serial.print("Pitch Change, ch=");
   Serial.print(channel, DEC);
   Serial.print(", pitch=");
   Serial.println(pitch, DEC);
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
