// MIT License
// 
// Copyright (c) 2024 Travis Smith
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


//IO Handler for MIDI ASID SysEx streams

void IO1Hndlr_ASID(uint8_t Address, bool R_Wn);  
void PollingHndlr_ASID();                           
void InitHndlr_ASID();                           

stcIOHandlers IOHndlr_ASID =
{
  "ASID Player",           //Name of handler
  &InitHndlr_ASID,       //Called once at handler startup
  &IO1Hndlr_ASID,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_ASID,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};

enum ASIDregsMatching  //synch with ASIDPlayer.asm
{
   ASIDAddrReg        = 0x02,   // Data type and SID Address Register
   ASIDDataReg        = 0x04,   // ASID data
   ASIDCompReg        = 0x08,   // Read Complete/good
                         
   ASIDAddrType_Skip  = 0x00,   // No data/skip
   ASIDAddrType_Char  = 0x20,   // Character data
   ASIDAddrType_Start = 0x40,   // 
   ASIDAddrType_Stop  = 0x60,   // 
   ASIDAddrType_SID1  = 0x80,   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2  = 0xa0,   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3  = 0xc0,   // Lower 5 bits are SID3 reg address
   ASIDAddrType_SID4  = 0xe0,   // Lower 5 bits are SID4 reg address
   ASIDAddrType_Mask  = 0xe0,   // 
   ASIDAddrAddr_Mask  = 0x1f,   // 
};

#define ASIDRxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+USB_MIDI_SYSEX_MAX-RxQueueTail))

uint8_t ASIDidToReg[] = 
{
 //reg#,// bit/id
    0,  // 00
    1,  // 01
    2,  // 02
    3,  // 03
    5,  // 04
    6,  // 05
    7,  // 06
           
    8,  // 07
    9,  // 08
   10,  // 09
   12,  // 10
   13,  // 11
   14,  // 12
   15,  // 13

   16,  // 14
   17,  // 15
   19,  // 16
   20,  // 17
   21,  // 18
   22,  // 19
   23,  // 20

   24,  // 21
    4,  // 22
   11,  // 23
   18,  // 24
    4,  // 25 <= secondary for reg 04
   11,  // 26 <= secondary for reg 11
   18,  // 27 <= secondary for reg 18
};

void AddToASIDRxQueue(uint8_t Addr, uint8_t Data)
{
  if (ASIDRxQueueUsed >= USB_MIDI_SYSEX_MAX-2)
  {
     Printf_dbg("-->ASID queue overflow!\n");
     return;
  }
  
  MIDIRxBuf[RxQueueHead] = Addr;
  MIDIRxBuf[RxQueueHead+1] = Data;
  
  if (RxQueueHead == USB_MIDI_SYSEX_MAX-2) RxQueueHead = 0;
  else RxQueueHead +=2;
  //assumes USB_MIDI_SYSEX_MAX is an even number
  //  currently 290, defined in cores\teensy4\usb_midi.h
}

//MIDI input handlers for HW Emulation _________________________________________________________________________

// F0 SysEx single call, message larger than buffer is truncated
void ASIDOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   //data already contains starting f0 and ending f7
   //Serial.printf("\nSysEx: size=%d, data=", size);
   //for(uint16_t Cnt=0; Cnt<size; Cnt++) Serial.printf(" $%02x", data[Cnt]);
   //Serial.println();
   
   // Implemented based on:   http://paulus.kapsi.fi/asid_protocol.txt
   
   unsigned int NumRegs = 0; //number of regs to write
   
   if(data[0] != 0xf0 || data[1] != 0x2d || data[size-1] != 0xf7)
   {
      Printf_dbg("-->Invalid ASID/SysEx format\n");
      return;
   }
   switch(data[2])
   {
      case 0x4c:
         AddToASIDRxQueue(ASIDAddrType_Start, 0);
         //Printf_dbg("Start playing\n");
         SetIRQAssert;
         break;
      case 0x4d:
         AddToASIDRxQueue(ASIDAddrType_Stop, 0);
         //Printf_dbg("Stop playback\n");
         SetIRQAssert;
         break;
      case 0x4f:
         //display characters
         //data[size-1] = 0; //replace 0xf7 with term
         //Printf_dbg("Display chars: \"%s\"\n", data+3);
         for(uint8_t CharNum=3; CharNum < size-1 ; CharNum++)
         {
            AddToASIDRxQueue(ASIDAddrType_Char, ToPETSCII(data[CharNum]));
         }
         AddToASIDRxQueue(ASIDAddrType_Char, 13);
         SetIRQAssert;
         break;
      case 0x4e:
         //SID1 reg data
         //Printf_dbg("SID1 reg data\n");
         for(uint8_t maskNum = 0; maskNum < 4; maskNum++)
         {
            for(uint8_t bitNum = 0; bitNum < 7; bitNum++)
            {
               if(data[3+maskNum] & (1<<bitNum))
               { //reg is to be written
                  uint8_t RegVal = data[11+NumRegs];
                  if(data[7+maskNum] & (1<<bitNum)) RegVal |= 0x80;
                  AddToASIDRxQueue((ASIDAddrType_SID1 | ASIDidToReg[maskNum*7+bitNum]), RegVal);
                  NumRegs++;
                  //Printf_dbg("#%d: reg $%02x = $%02x\n", NumRegs, ASIDidToReg[maskNum*7+bitNum], RegVal);
               }
            }
         }
         if(12+NumRegs > size)
         {
            Printf_dbg("-->More regs flagged than data available\n");    
         }
         SetIRQAssert;
         break;
      default:
         Printf_dbg("-->Unexpected ASID msg type: $%02x\n", data[2]);
   }
}


//____________________________________________________________________________________________________

void InitHndlr_ASID()  
{
   RxQueueHead = RxQueueTail = 0;
   
   //SetMIDIHandlersNULL(); is called prior to this, all other MIDI messages ignored.
   // MIDI USB Host input handlers
   usbHostMIDI.setHandleSystemExclusive     (ASIDOnSystemExclusive);     // F0

   // MIDI USB Device input handlers
   usbDevMIDI.setHandleSystemExclusive      (ASIDOnSystemExclusive);     // F0
}

void IO1Hndlr_ASID(uint8_t Address, bool R_Wn)
{
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      switch(Address)
      {
         case ASIDAddrReg:
            if(ASIDRxQueueUsed)
            {
               DataPortWriteWaitLog(MIDIRxBuf[RxQueueTail]); 
            }
            else  //no data to send, send skip message
            { //should no longer happen...
               DataPortWriteWaitLog(ASIDAddrType_Skip);
            }
            break;
         case ASIDDataReg:
            if(ASIDRxQueueUsed)
            {
               DataPortWriteWaitLog(MIDIRxBuf[RxQueueTail+1]);  
            }
            else  //no data to send, send 0
            { //should no longer happen...
               DataPortWriteWaitLog(0);
            }
            break;
         case ASIDCompReg:
            DataPortWriteWaitLog(0);
            if(ASIDRxQueueUsed)
            {
               RxQueueTail+=2;  //inc on data read, must happen after address/data reads
               if (RxQueueTail == USB_MIDI_SYSEX_MAX) RxQueueTail = 0;
            }    
            if(ASIDRxQueueUsed == 0) SetIRQDeassert;          
         default:
            DataPortWriteWaitLog(0); //read 0s from all other regs in IO1
      }
   }

}

void PollingHndlr_ASID()
{
   if(ASIDRxQueueUsed == 0) //read MIDI-in data in only if ready to send to C64 (buffer empty)
   {
      usbDevMIDI.read();
      if(ASIDRxQueueUsed == 0) usbHostMIDI.read();   //dito, giving USB device priority 
   }
}

