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


//IO Handler for MIDI (6580 ACIA interface) Emulation

void IO1Hndlr_MIDI(uint8_t Address, bool R_Wn);  
void PollingHndlr_MIDI();                           
void InitHndlr_MIDI_Datel();                           
void InitHndlr_MIDI_Sequential();                           
void InitHndlr_MIDI_Passport();                           
void InitHndlr_MIDI_NamesoftIRQ();                           

stcIOHandlers IOHndlr_MIDI_Datel =
{
  "MIDI:Datel/Siel",           //Name of handler
  &InitHndlr_MIDI_Datel,       //Called once at handler startup
  &IO1Hndlr_MIDI,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_MIDI,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};

stcIOHandlers IOHndlr_MIDI_Sequential =
{
  "MIDI:Sequential",           //Name of handler
  &InitHndlr_MIDI_Sequential,  //Called once at handler startup
  &IO1Hndlr_MIDI,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_MIDI,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};

stcIOHandlers IOHndlr_MIDI_Passport =
{
  "MIDI:Passport/Sent",        //Name of handler
  &InitHndlr_MIDI_Passport,    //Called once at handler startup
  &IO1Hndlr_MIDI,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_MIDI,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};

stcIOHandlers IOHndlr_MIDI_NamesoftIRQ =
{
  "MIDI:Namesoft IRQ",         //Name of handler
  &InitHndlr_MIDI_NamesoftIRQ, //Called once at handler startup
  &IO1Hndlr_MIDI,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_MIDI,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};



//see https://codebase64.org/doku.php?id=base:c64_midi_interfaces
// 6580 ACIA interface emulation
//rIORegMIDIStatus:
#define MIDIStatusIRQReq  0x80   // Interrupt Request
#define MIDIStatusDCD     0x04   // Data Carrier Detect (Ready to receive Tx data)
#define MIDIStatusTxRdy   0x02   // Transmit Data Register Empty (Ready to receive Tx data)
#define MIDIStatusRxFull  0x01   // Receive Data Register Full (Rx Data waiting to be read)

#define NumMIDIControls   16  //must be power of 2, may want to do this differently?

volatile uint8_t rIORegMIDIStatus   = 0;
volatile uint8_t MIDIRxIRQEnabled = false;
volatile uint8_t MIDIRxBytesToSend = 0;
volatile uint8_t MIDIRxBuf[3];
volatile uint8_t MIDITxBytesReceived = 0;
volatile uint8_t MIDITxBuf[3];
uint8_t MIDIControlVals[NumMIDIControls];
uint8_t wIORegAddrMIDIControl, rIORegAddrMIDIStatus, wIORegAddrMIDITransmit, rIORegAddrMIDIReceive;



//MIDI input handlers for HW Emulation _________________________________________________________________________
//Only called if MIDIRxBytesToSend==0 (No data waiting)

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
      if ((MIDIRxBuf[0] & 0xf0) != 0xf0) Printf_dbg("IRQ off\n"); //don't print on real-time inputs (there are lots)
   }
}

void HWEOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)  
{
   MIDIRxBuf[2] = 0x80 | channel; //8x
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)  
{
   MIDIRxBuf[2] = 0x90 | channel; //9x
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnAfterTouchPoly(uint8_t channel, uint8_t note, uint8_t velocity)
{
   MIDIRxBuf[2] = 0xa0 | channel; // Ax
   MIDIRxBuf[1] = note;
   MIDIRxBuf[0] = velocity;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
   if (value==64) return; //sends ref first, always 64 so just assume it
   control &= (NumMIDIControls-1);
   
   int NewVal = MIDIControlVals[control] + value - 64;
   if (NewVal<0) NewVal=0;
   if (NewVal>127) NewVal=127;
   MIDIControlVals[control] = NewVal;
      
   MIDIRxBuf[2] = 0xb0 | channel;  //Bx
   MIDIRxBuf[1] = control;
   MIDIRxBuf[0] = NewVal;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnProgramChange(uint8_t channel, uint8_t program)
{
   MIDIRxBuf[1] = 0xc0 | channel; // Cx
   MIDIRxBuf[0] = program;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnAfterTouch(uint8_t channel, uint8_t pressure)
{   
   MIDIRxBuf[1] = 0xd0 | channel;  // Dx
   MIDIRxBuf[0] = pressure;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnPitchChange(uint8_t channel, int pitch)
{
   //-8192 to 8192, returns to 0 always
   pitch+=8192;
   
   MIDIRxBuf[2] = 0xe0 | channel;  //Ex
   MIDIRxBuf[1] = pitch & 0x7f;
   MIDIRxBuf[0] = (pitch>>7) & 0x7f;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

// F0 SysEx single call, message larger than buffer is truncated
void HWEOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   //need a bigger buffer and lots of time, not forwarding for now
}

void HWEOnTimeCodeQuarterFrame(uint8_t data) 
{
   MIDIRxBuf[1] = 0xf1; // F1
   MIDIRxBuf[0] = data;  //won't have bit 7 set
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnSongPosition(uint16_t beats)       
{
   MIDIRxBuf[2] = 0xf2; // F2
   MIDIRxBuf[1] = beats & 0x7f; //not sure if this is correct format?
   MIDIRxBuf[0] = (beats >> 7) & 0x7f;
   MIDIRxBytesToSend = 3;
   SetMidiIRQ();
}

void HWEOnSongSelect(uint8_t songnumber)     
{
   MIDIRxBuf[1] = 0xf3; // F3
   MIDIRxBuf[0] = songnumber;
   MIDIRxBytesToSend = 2;
   SetMidiIRQ();
}

void HWEOnTuneRequest()                  
{
   MIDIRxBuf[0] = 0xf6; // F6
   MIDIRxBytesToSend = 1;
   SetMidiIRQ();
}

// F8-FF (except FD)
void HWEOnRealTimeSystem(uint8_t realtimebyte)     
{
   MIDIRxBuf[0] = realtimebyte;
   MIDIRxBytesToSend = 1;
   SetMidiIRQ();
}

//____________________________________________________________________________________________________

void MIDIinHndlrInit()
{
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
}   
   
void InitHndlr_MIDI_Datel()  
{
   wIORegAddrMIDIControl  = 4;
   rIORegAddrMIDIStatus   = 6;
   wIORegAddrMIDITransmit = 5;
   rIORegAddrMIDIReceive  = 7;
   MIDIinHndlrInit();
}
   
void InitHndlr_MIDI_Sequential()                         
{
   wIORegAddrMIDIControl  = 0;
   rIORegAddrMIDIStatus   = 2;
   wIORegAddrMIDITransmit = 1;
   rIORegAddrMIDIReceive  = 3;
   MIDIinHndlrInit();
}
   
void InitHndlr_MIDI_Passport()                         
{
   wIORegAddrMIDIControl  = 8;
   rIORegAddrMIDIStatus   = 8;
   wIORegAddrMIDITransmit = 9;
   rIORegAddrMIDIReceive  = 9;
   MIDIinHndlrInit();
}
   
void InitHndlr_MIDI_NamesoftIRQ()                          
{
   // same as seq, no NMI
   wIORegAddrMIDIControl  = 0;
   rIORegAddrMIDIStatus   = 2;
   wIORegAddrMIDITransmit = 1;
   rIORegAddrMIDIReceive  = 3;
   MIDIinHndlrInit();
}

void IO1Hndlr_MIDI(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      if      (Address == rIORegAddrMIDIStatus)
      {
         DataPortWriteWaitLog(rIORegMIDIStatus);  
      }
      else if (Address == rIORegAddrMIDIReceive) //MIDI-in from USB kbd (interrupt driven)
      {
         if(MIDIRxBytesToSend)
         {
            DataPortWriteWaitLog(MIDIRxBuf[--MIDIRxBytesToSend]);  
         }
         else
         {
            DataPortWriteWaitLog(0); 
            Printf_dbg("unreq\n"); //unrequested read from Rx reg.
         }
         if (MIDIRxBytesToSend == 0) //if we're done/empty, remove the interrupt
         {
            rIORegMIDIStatus &= ~(MIDIStatusRxFull | MIDIStatusIRQReq);
            SetIRQDeassert;
         }
      } else
      {
         DataPortWriteWaitLog(0); //read 0s from all other regs in IO1
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      Data = DataPortWaitRead(); 
      if (Address == wIORegAddrMIDIControl)
      {
         if (Data == 0x03) //Master Reset
         {
            rIORegMIDIStatus   = 0;
            MIDIRxBytesToSend  = 0;
            //MIDIRxIRQEnabled = false;
            SetIRQDeassert;
         }
         MIDIRxIRQEnabled = (Data & 0x80) == 0x80;
         if (MIDIRxIRQEnabled) //Receive Interrupt Enable set
         {
            rIORegMIDIStatus |= MIDIStatusDCD;
            MIDIRxBytesToSend = 0; 
            Printf_dbg("RIE:%02x\n", Data);
         }
         if ((Data & 0x1C) == 0x14) // xxx101xx Word Select == 8 Bits + No Parity + 1 Stop Bit
         {
            rIORegMIDIStatus |= MIDIStatusTxRdy;              
         }
      }
      else if (Address == wIORegAddrMIDITransmit) //Tx MIDI out to USB instrument
      {
         if (MIDITxBytesReceived < 3) //make sure there's not a full packet already in progress
         {
            if ((Data & 0x80) == 0x80) //header byte, start new packet
            {
               if(MIDITxBytesReceived) Printf_dbg("drop %d\n", MIDITxBytesReceived); //had another in progress
               MIDITxBytesReceived = 0;
               switch(Data)
               {
                  case 0x00 ... 0xef: 
                  case 0xf1: 
                  case 0xf2: 
                  case 0xf3: 
                     //2-3 byte messages
                     MIDITxBuf[MIDITxBytesReceived++] = Data;
                     break;
                  case 0xf6:
                  case 0xf8 ... 0xff:
                     //1 byte messages, send now
                     MIDITxBuf[0] = Data;
                     MIDITxBuf[1] = 0;
                     MIDITxBuf[2] = 0;
                     MIDITxBytesReceived = 3;
                     break;
                  default:
                     Printf_dbg("igh: %02x\n", Data);
                     break;
               }
            }
            else  //adding data to existing
            {
               if(MIDITxBytesReceived > 0) //make sure we accepted a valid header byte previously
               {
                  MIDITxBuf[MIDITxBytesReceived++] = Data;
                  if(MIDITxBytesReceived == 2 && ((MIDITxBuf[0] & 0xf0) == 0xc0 || (MIDITxBuf[0] & 0xf0) == 0xd0 || MIDITxBuf[0] == 0xf1 || MIDITxBuf[0] == 0xf3))
                  { //single extra byte commands, send now
                     MIDITxBuf[2] = 0;
                     MIDITxBytesReceived = 3;
                  }
               }
               else Printf_dbg("igd: %02x\n", Data);
            }
            rIORegMIDIStatus &= ~MIDIStatusIRQReq;
            if(MIDITxBytesReceived == 3) rIORegMIDIStatus &= ~MIDIStatusTxRdy; //not ready, waiting for USB transmit
         }
         else Printf_dbg("Miss!\n");
      }
      TraceLogAddValidData(Data);
   }
}

void PollingHndlr_MIDI()
{
   if (MIDIRxBytesToSend == 0) midi1.read(); //read MIDI-in data in only if ready to send to C64 (buffer empty)
      
   if (MIDITxBytesReceived == 3)  //Transmit MIDI-out data if buffer full/ready from C64
   {
      if (MIDITxBuf[0]<0xf0) midi1.send(MIDITxBuf[0] & 0xf0, MIDITxBuf[1], MIDITxBuf[2], MIDITxBuf[0] & 0x0f);
      else midi1.send(MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2], 0);
      
      Printf_dbg("Mout: %02x %02x %02x\n", MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2]);
      MIDITxBytesReceived = 0;
      rIORegMIDIStatus |= MIDIStatusTxRdy | MIDIStatusIRQReq;
   }
}
