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

#define DEBUG_MEMLOC   FLASHMEM

void IO1Hndlr_Debug(uint8_t Address, bool R_Wn);  
void PollingHndlr_Debug();                           
void InitHndlr_Debug();                           

stcIOHandlers IOHndlr_Debug =
{
  "Debug",             //Name of handler
  &InitHndlr_Debug,    //Called once at handler startup
  &IO1Hndlr_Debug,     //IO1 R/W handler
  NULL,                //IO2 R/W handler
  NULL,                //ROML Read handler, in addition to any ROM data sent
  NULL,                //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_Debug, //Polled in main routine
  NULL,                //called at the end of EVERY c64 cycle
};

extern uint8_t ASIDidToReg[];

//MIDI input handlers for Debug _________________________________________________________________________

DEBUG_MEMLOC void DbgOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
   Serial.printf("8x Note Off, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

DEBUG_MEMLOC void DbgOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{   
   Serial.printf("9x Note On, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

DEBUG_MEMLOC void DbgOnAfterTouchPoly(uint8_t channel, uint8_t note, uint8_t velocity)
{
   Serial.printf("Ax After Touch Poly, ch=%d, note=%d, velocity=%d\n", channel, note, velocity);
}

DEBUG_MEMLOC void DbgOnControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
   Serial.printf("Bx Control Change, ch=%d, control=%d, value=%d\n", channel, control, value);
}

DEBUG_MEMLOC void DbgOnProgramChange(uint8_t channel, uint8_t program)
{   
   Serial.printf("Cx Program Change, ch=%d, program=%d\n", channel, program);
}

DEBUG_MEMLOC void DbgOnAfterTouch(uint8_t channel, uint8_t pressure)
{   
   Serial.printf("Dx After Touch, ch=%d, pressure=%d\n", channel, pressure);
}

DEBUG_MEMLOC void DbgOnPitchChange(uint8_t channel, int pitch) 
{
   Serial.printf("Ex Pitch Change, ch=%d, (int)pitch=%d\n", channel, pitch);
}

// F0 SysEx single call, message larger than buffer is truncated
DEBUG_MEMLOC void DbgOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   Serial.printf("\nSysEx: size=%d, data=", size);
   for(uint16_t Cnt=0; Cnt<size; Cnt++) Serial.printf(" $%02x", data[Cnt]);
   Serial.println();
   
   // Implemented based on:   http://paulus.kapsi.fi/asid_protocol.txt
   
   unsigned int NumRegs = 0; //number of regs to write
   
   if(data[0] != 0xf0 || data[1] != 0x2d || data[size-1] != 0xf7)
   {
      Serial.println("-->Invalid ASID/SysEx format");
      return;
   }
   switch(data[2])
   {
      case 0x4c:
         Serial.println("Start playing");
         break;
      case 0x4d:
         Serial.println("Stop playback");
         break;
      case 0x4f:
         //display characters
         data[size-1] = 0; //replace 0xf7 with term
         Serial.printf("Display chars: \"%s\"\n", data+3);
         break;
      case 0x4e:
         //SID1 reg data
         Serial.println("SID1 reg data");
         for(uint8_t maskNum = 0; maskNum < 4; maskNum++)
         {
            for(uint8_t bitNum = 0; bitNum < 7; bitNum++)
            {
               if(data[3+maskNum] & (1<<bitNum))
               { //reg is to be written
                  uint8_t RegVal = data[11+NumRegs];
                  if(data[7+maskNum] & (1<<bitNum)) RegVal |= 0x80;
                  NumRegs++;
                  Serial.printf("#%d: reg $%02x = $%02x\n", NumRegs, ASIDidToReg[maskNum*7+bitNum], RegVal);
               }
            }
         }
         if(12+NumRegs > size)
         {
            Serial.println("-->More regs flagged than data available");    
         }
         break;
      default:
         Serial.printf("-->Unexpected msg type: $%02x\n", data[2]);
   }
}

DEBUG_MEMLOC void DbgOnTimeCodeQuarterFrame(uint8_t data)
{
   Serial.printf("F1 TimeCodeQuarterFrame, data=%d\n", data);
   //could decode this, see example
}

DEBUG_MEMLOC void DbgOnSongPosition(uint16_t beats)       
{
   Serial.printf("F2 Song Position, (uint)beats=%d\n", beats);
}

DEBUG_MEMLOC void DbgOnSongSelect(uint8_t songnumber)     
{
   Serial.printf("F3 Song Select, songnumber=%d\n", songnumber);
}

DEBUG_MEMLOC void DbgOnTuneRequest(void)
{
   Serial.printf("F6 TuneRequest\n");
}

// F8-FF (except FD)
DEBUG_MEMLOC void DbgOnRealTimeSystem(uint8_t realtimebyte)     
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

//______________________________________________________________________________________________

DEBUG_MEMLOC void InitHndlr_Debug()
{
   usbHostMIDI.setHandleNoteOff             (DbgOnNoteOff);             // 8x
   usbHostMIDI.setHandleNoteOn              (DbgOnNoteOn);              // 9x
   usbHostMIDI.setHandleAfterTouchPoly      (DbgOnAfterTouchPoly);      // Ax
   usbHostMIDI.setHandleControlChange       (DbgOnControlChange);       // Bx
   usbHostMIDI.setHandleProgramChange       (DbgOnProgramChange);       // Cx
   usbHostMIDI.setHandleAfterTouch          (DbgOnAfterTouch);          // Dx
   usbHostMIDI.setHandlePitchChange         (DbgOnPitchChange);         // Ex
   usbHostMIDI.setHandleSystemExclusive     (DbgOnSystemExclusive);     // F0   
   usbHostMIDI.setHandleTimeCodeQuarterFrame(DbgOnTimeCodeQuarterFrame);// F1
   usbHostMIDI.setHandleSongPosition        (DbgOnSongPosition);        // F2
   usbHostMIDI.setHandleSongSelect          (DbgOnSongSelect);          // F3
   usbHostMIDI.setHandleTuneRequest         (DbgOnTuneRequest);         // F6
   usbHostMIDI.setHandleRealTimeSystem      (DbgOnRealTimeSystem);      // F8-FF (except FD)
   // not catching F4, F5, F7 (end of SysEx), and FD                  

   usbDevMIDI.setHandleNoteOff             (DbgOnNoteOff);             // 8x
   usbDevMIDI.setHandleNoteOn              (DbgOnNoteOn);              // 9x
   usbDevMIDI.setHandleAfterTouchPoly      (DbgOnAfterTouchPoly);      // Ax
   usbDevMIDI.setHandleControlChange       (DbgOnControlChange);       // Bx
   usbDevMIDI.setHandleProgramChange       (DbgOnProgramChange);       // Cx
   usbDevMIDI.setHandleAfterTouch          (DbgOnAfterTouch);          // Dx
   usbDevMIDI.setHandlePitchChange         (DbgOnPitchChange);         // Ex
   usbDevMIDI.setHandleSystemExclusive     (DbgOnSystemExclusive);     // F0   
   usbDevMIDI.setHandleTimeCodeQuarterFrame(DbgOnTimeCodeQuarterFrame);// F1
   usbDevMIDI.setHandleSongPosition        (DbgOnSongPosition);        // F2
   usbDevMIDI.setHandleSongSelect          (DbgOnSongSelect);          // F3
   usbDevMIDI.setHandleTuneRequest         (DbgOnTuneRequest);         // F6
   usbDevMIDI.setHandleRealTimeSystem      (DbgOnRealTimeSystem);      // F8-FF (except FD)
}

void IO1Hndlr_Debug(uint8_t Address, bool R_Wn)
{
   #ifndef DbgIOTraceLog
      BigBuf[BigBufCount] = Address; //initialize w/ address 
   #endif
   if (R_Wn) //High (IO1 Read)
   {
      //DataPortWriteWaitLog(0); //respond to all reads
      //BigBuf[BigBufCount] |= (0<<8) | IOTLDataValid;
      //Serial.printf("Rd $de%02x\n", Address);
   }
   else  // IO1 write
   {
      BigBuf[BigBufCount] |= (DataPortWaitRead()<<8) | IOTLDataValid;
      //Serial.printf("wr $de%02x:$%02x\n", Address, Data);
   }
   #ifndef DbgIOTraceLog
      if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
      if (BigBufCount < BigBufSize) BigBufCount++;
   #endif
}

void PollingHndlr_Debug()
{
   usbHostMIDI.read();
   usbDevMIDI.read();
}

