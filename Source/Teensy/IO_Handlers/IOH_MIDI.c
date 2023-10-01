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

//#define NumMIDIControls   16  //must be power of 2, may want to do this differently?
//uint8_t MIDIControlVals[NumMIDIControls];

volatile uint8_t rIORegMIDIStatus   = 0;
volatile uint8_t MIDIRxIRQEnabled = false;
volatile uint16_t MIDIRxBytesToSend = 0;
volatile uint8_t MIDIRxBuf[USB_MIDI_SYSEX_MAX]; //currently 290, defined in cores\teensy4\usb_midi.h  
volatile uint8_t MIDITxBytesReceived = 0;
volatile uint8_t MIDITxBuf[3];
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
   //did this to accomodate relative mode, but turns out it's not so widely used...
      //if (value==64) return; //sends ref first, always 64 so just assume it
      //control &= (NumMIDIControls-1);
      //
      //int NewVal = MIDIControlVals[control] + value - 64;
      //if (NewVal<0) NewVal=0;
      //if (NewVal>127) NewVal=127;
      //MIDIControlVals[control] = NewVal;
      
   MIDIRxBuf[2] = 0xb0 | channel;  //Bx
   MIDIRxBuf[1] = control;
   MIDIRxBuf[0] = value; //NewVal;
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
   //data already contains starting f0 and ending f7
   //just have to reverse the order to the RxBuf "stack"
   for(uint16_t Cnt=0; Cnt<size; Cnt++) MIDIRxBuf[size-Cnt-1]=data[Cnt];
   MIDIRxBytesToSend = size;
   SetMidiIRQ();
   
   #ifdef DbgMsgs_IO
      if (data[0]!=0xf0 || data[size-1]!=0xf7) Printf_dbg("Bad SysEx: %d %02x %02x\n", size, data[0], data[size-1]);
   #endif
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
   //for (uint8_t ContNum=0; ContNum < NumMIDIControls;) MIDIControlVals[ContNum++]=63;
   
   // MIDI USB Host input handlers
   usbHostMIDI.setHandleNoteOff             (HWEOnNoteOff);             // 8x
   usbHostMIDI.setHandleNoteOn              (HWEOnNoteOn);              // 9x
   usbHostMIDI.setHandleAfterTouchPoly      (HWEOnAfterTouchPoly);      // Ax
   usbHostMIDI.setHandleControlChange       (HWEOnControlChange);       // Bx
   usbHostMIDI.setHandleProgramChange       (HWEOnProgramChange);       // Cx
   usbHostMIDI.setHandleAfterTouch          (HWEOnAfterTouch);          // Dx
   usbHostMIDI.setHandlePitchChange         (HWEOnPitchChange);         // Ex
   usbHostMIDI.setHandleSystemExclusive     (HWEOnSystemExclusive);     // F0
   usbHostMIDI.setHandleTimeCodeQuarterFrame(HWEOnTimeCodeQuarterFrame);// F1
   usbHostMIDI.setHandleSongPosition        (HWEOnSongPosition);        // F2
   usbHostMIDI.setHandleSongSelect          (HWEOnSongSelect);          // F3
   usbHostMIDI.setHandleTuneRequest         (HWEOnTuneRequest);         // F6
   usbHostMIDI.setHandleRealTimeSystem      (HWEOnRealTimeSystem);      // F8-FF (except FD)

   // MIDI USB Device input handlers
   usbDevMIDI.setHandleNoteOff              (HWEOnNoteOff);             // 8x
   usbDevMIDI.setHandleNoteOn               (HWEOnNoteOn);              // 9x
   usbDevMIDI.setHandleAfterTouchPoly       (HWEOnAfterTouchPoly);      // Ax
   usbDevMIDI.setHandleControlChange        (HWEOnControlChange);       // Bx //was disabled as apps like cakewalk write controls to 0 on stop, mess up cynthcart settings 
   usbDevMIDI.setHandleProgramChange        (HWEOnProgramChange);       // Cx //was disabled as apps like cakewalk write programs on start/stop, mess up Sta64 settings 
   usbDevMIDI.setHandleAfterTouch           (HWEOnAfterTouch);          // Dx
   usbDevMIDI.setHandlePitchChange          (HWEOnPitchChange);         // Ex //was disabled as apps like cakewalk write pitch to 0 on stop and crash cynthcart
   usbDevMIDI.setHandleSystemExclusive      (HWEOnSystemExclusive);     // F0
   usbDevMIDI.setHandleTimeCodeQuarterFrame (HWEOnTimeCodeQuarterFrame);// F1
   usbDevMIDI.setHandleSongPosition         (HWEOnSongPosition);        // F2
   usbDevMIDI.setHandleSongSelect           (HWEOnSongSelect);          // F3
   usbDevMIDI.setHandleTuneRequest          (HWEOnTuneRequest);         // F6
   usbDevMIDI.setHandleRealTimeSystem       (HWEOnRealTimeSystem);      // F8-FF (except FD)
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
   if (MIDIRxBytesToSend == 0) //read MIDI-in data in only if ready to send to C64 (buffer empty)
   {
      usbHostMIDI.read(); 
      if (MIDIRxBytesToSend == 0) usbDevMIDI.read(); //dito, giving hosted device priority
   }
   
   if (MIDITxBytesReceived == 3)  //Transmit MIDI-out data if buffer full/ready from C64
   {
      if (MIDITxBuf[0]<0xf0) usbHostMIDI.send(MIDITxBuf[0] & 0xf0, MIDITxBuf[1], MIDITxBuf[2], MIDITxBuf[0] & 0x0f);
      else usbHostMIDI.send(MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2], 0);
      
      Printf_dbg("Mout: %02x %02x %02x\n", MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2]);
      MIDITxBytesReceived = 0;
      rIORegMIDIStatus |= MIDIStatusTxRdy | MIDIStatusIRQReq;
   }
}

