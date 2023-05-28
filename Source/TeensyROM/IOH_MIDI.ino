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


//IO1 Handler for MIDI (6580 ACIA interface) Emulation _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_MIDI(uint8_t Address, bool R_Wn)
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

