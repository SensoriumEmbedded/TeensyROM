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
   ASIDAddrReg        = 0xc2,   // Data type and SID Address Register (Read only)
   ASIDDataReg        = 0xc4,   // ASID data, increment queue Tail (Read only)
   ASIDContReg        = 0xc8,   // Control Reg (Write only)

   ASIDContIRQOn      = 0x01,   //enable ASID IRQ
   ASIDContExit       = 0x02,   //Disable IRQ, Send TR to main menu
   
   ASIDAddrType_Skip  = 0x00,   // No data/skip
   ASIDAddrType_Char  = 0x20,   // Character data
   ASIDAddrType_Start = 0x40,   // ASID Start message
   ASIDAddrType_Stop  = 0x60,   // ASID Stop message
   ASIDAddrType_SID1  = 0x80,   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2  = 0xa0,   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3  = 0xc0,   // Lower 5 bits are SID3 reg address
   ASIDAddrType_Error = 0xe0,   // Error from parser
   
   ASIDAddrType_Mask  = 0xe0,   // Mask for Type
   ASIDAddrAddr_Mask  = 0x1f,   // Mask for Address
};

#define ASIDQueueSize   (USB_MIDI_SYSEX_MAX & ~1)  // force to even number; currently 290, defined in cores\teensy4\usb_midi.h
#define ASIDRxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+ASIDQueueSize-RxQueueTail))

#ifdef DbgMsgs_IO  //Debug msgs mode
   #define Printf_dbg_SysExInfo {Serial.printf("\nSysEx: size=%d, data=", size); for(uint16_t Cnt=0; Cnt<size; Cnt++) Serial.printf(" $%02x", data[Cnt]);Serial.println();}
#else //Normal mode
   #define Printf_dbg_SysExInfo {}
#endif

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
  if (ASIDRxQueueUsed >= ASIDQueueSize-2)
  {
     Printf_dbg("-->ASID queue overflow!\n");
     return;
  }
  
  MIDIRxBuf[RxQueueHead] = Addr;
  MIDIRxBuf[RxQueueHead+1] = Data;
  
  if (RxQueueHead == ASIDQueueSize-2) RxQueueHead = 0;
  else RxQueueHead +=2;
}

void SetASIDIRQ()
{
   if(MIDIRxIRQEnabled)
   {
      SetIRQAssert;
   }
   else
   {
      Printf_dbg("ASID IRQ not enabled\n");
      RxQueueHead = RxQueueTail = 0;
   }
}

void AddErrorToASIDRxQueue()
{
   AddToASIDRxQueue(ASIDAddrType_Error, 0);
   SetASIDIRQ();
}

void DecodeSendSIDRegData(uint8_t SID_ID, uint8_t *data, unsigned int size) 
{
   unsigned int NumRegs = 0; //number of regs to write
   
   //Printf_dbg("SID$%02x reg data\n", SID_ID);
   for(uint8_t maskNum = 0; maskNum < 4; maskNum++)
   {
      for(uint8_t bitNum = 0; bitNum < 7; bitNum++)
      {
         if(data[3+maskNum] & (1<<bitNum))
         { //reg is to be written
            uint8_t RegVal = data[11+NumRegs];
            if(data[7+maskNum] & (1<<bitNum)) RegVal |= 0x80;
            AddToASIDRxQueue((SID_ID | ASIDidToReg[maskNum*7+bitNum]), RegVal);
            NumRegs++;
            //Printf_dbg("#%d: reg $%02x = $%02x\n", NumRegs, ASIDidToReg[maskNum*7+bitNum], RegVal);
         }
      }
   }
   if(12+NumRegs > size)
   {
      AddErrorToASIDRxQueue();
      Printf_dbg_SysExInfo;
      Printf_dbg("-->More regs flagged than data available\n");    
   }
   SetASIDIRQ();   
}


//MIDI input handlers for HW Emulation _________________________________________________________________________

// F0 SysEx single call, message larger than buffer is truncated
void ASIDOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   //data already contains starting f0 and ending f7
   //Printf_dbg_SysExInfo;
   
   // ASID decode based on:   http://paulus.kapsi.fi/asid_protocol.txt
   // originally by Elektron SIDStation
      
   if(data[0] != 0xf0 || data[1] != 0x2d || data[size-1] != 0xf7)
   {
      AddErrorToASIDRxQueue();
      Printf_dbg_SysExInfo;
      Printf_dbg("-->Invalid ASID/SysEx format\n");
      return;
   }
   switch(data[2])
   {
      case 0x4c: //start playing message
         AddToASIDRxQueue(ASIDAddrType_Start, 0);
         //Printf_dbg("Start playing\n");
         SetASIDIRQ();
         break;
      case 0x4d: //stop playback message
         AddToASIDRxQueue(ASIDAddrType_Stop, 0);
         //Printf_dbg("Stop playback\n");
         SetASIDIRQ();
         break;
      case 0x4f: //Display Characters
         //display characters
         //data[size-1] = 0; //replace 0xf7 with term
         //Printf_dbg("Display chars: \"%s\"\n", data+3);
         for(uint8_t CharNum=3; CharNum < size-1 ; CharNum++)
         {
            AddToASIDRxQueue(ASIDAddrType_Char, ToPETSCII(data[CharNum]));
         }
         AddToASIDRxQueue(ASIDAddrType_Char, 13);
         SetASIDIRQ();
         break;
      case 0x4e:  //SID1 reg data (primary)
         DecodeSendSIDRegData(ASIDAddrType_SID1, data, size);
         break;
      case 0x50:  //SID2 reg data
         DecodeSendSIDRegData(ASIDAddrType_SID2, data, size);
         break;
      case 0x51:  //SID3 reg data
         DecodeSendSIDRegData(ASIDAddrType_SID3, data, size);
         break;
      //case 0x52:  //SID4 reg data.  ** Haven't seen this value used
      //   DecodeSendSIDRegData(ASIDAddrType_SID4, data, size);
      //   break;
      default:
         AddErrorToASIDRxQueue();
         Printf_dbg_SysExInfo;
         Printf_dbg("-->Unexpected ASID msg type: $%02x\n", data[2]);
   }
}


//____________________________________________________________________________________________________

void InitHndlr_ASID()  
{
   RxQueueHead = RxQueueTail = 0;

   Printf_dbg("ASID Queue Size: %d\n", ASIDQueueSize);
   
   // SetMIDIHandlersNULL(); is called prior to this, 
   //    all other MIDI messages ignored.
   
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
            else  //no data to send, send skip message (should not happen)
            {
               DataPortWriteWaitLog(ASIDAddrType_Skip);
            }
            break;
         case ASIDDataReg:
            if(ASIDRxQueueUsed)
            {
               DataPortWriteWaitLog(MIDIRxBuf[RxQueueTail+1]);  
               RxQueueTail+=2;  //inc on data read, must happen after address read
               if (RxQueueTail == ASIDQueueSize) RxQueueTail = 0;
            }
            else  //no data to send, send 0  (should not happen)
            {
               DataPortWriteWaitLog(0);
            }
            if(ASIDRxQueueUsed == 0) SetIRQDeassert;  //remove IRQ if queue empty        
            break;
         //default:
            //leave other locations available for potential SID in IO1
            //DataPortWriteWaitLog(0); 
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      uint8_t Data = DataPortWaitRead(); 
      if (Address == ASIDContReg)
      {
         switch(Data)
         {
            case ASIDContIRQOn:
               MIDIRxIRQEnabled = true;
               RxQueueHead = RxQueueTail = 0;
               Printf_dbg("ASIDContIRQOn\n");
               break;
            case ASIDContExit:
               MIDIRxIRQEnabled = false;
               BtnPressed = true;   //main menu
               Printf_dbg("ASIDContExit\n");
               break;
         }
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

