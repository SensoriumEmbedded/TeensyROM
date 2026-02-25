// MIT License
// 
// Copyright (c) 2026 Travis Smith
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

void IO2Hndlr_REU(uint8_t Address, bool R_Wn);  
void PollingHndlr_REU();                           
void InitHndlr_REU();                           

stcIOHandlers IOHndlr_REU =
{
  "REU",               //Name of handler
  &InitHndlr_REU,      //Called once at handler startup
  NULL,                //IO1 R/W handler
  &IO2Hndlr_REU,       //IO2 R/W handler
  NULL,                //ROML Read handler, in addition to any ROM data sent
  NULL,                //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_REU,   //Polled in main routine
  NULL,                //called at the end of EVERY c64 cycle
};

enum enumREUregs
{
   // REU registers:
   REUReg_Status = 0,
   REUReg_Command,
   REUReg_C64StartAddrLSB,
   REUReg_C64StartAddrMSB,
   REUReg_REUStartAddrLSB,
   REUReg_REUStartAddrMoreSB,
   REUReg_REUStartAddrMostSB,
   REUReg_TransLengthLSB,
   REUReg_TransLengthMSB,
   REUReg_InterruptMask,
   REUReg_AddressControl,

   REUReg_NumRegs,
};

//______________________________________________________________________________________________

uint8_t REURegs[REUReg_NumRegs];

//From: https://codebase64.net/doku.php?id=base:reu_registers
//  Adr Bit Function
//   0     Status register - read only
//      7     Interrupt Pending (1=interrupt waiting to be serviced)
//      6     End of Block (1=transfer complete)
//      5     Fault (1=block verify error)
//      4     Size (tells if a jumper is cut in the REU)
//      3-0   Version number (0 on the REU I tested)
//      Note: Bits 7-5 are cleared when this register is read.
//   1     Command Register
//      7     Execute (1=initiate transfer per current config)
//      6     reserved (returns 0 upon reading)
//      5     Load (1=enable AUTOLOAD option)
//      4     FF00 (1=disable FF00 decode)
//      3-2   reserved (0 upon reading)
//      1-0   Transfer type: 00=C64-&gt;REU
//               01=REU-&gt;C64
//               10=swap
//               11=verify
//   2  7-0   C64 start address (LSB)
//   3  7-0   C64 start address (MSB)
//   4  7-0   REU start address (LSB)
//   5  7-0   REU start address (More SB)
//   6  2-0   REU start address (most significant bits)
//   7  7-0   Transfer length (LSB) ($0000=64 kB)
//   8  7-0   Transfer length (MSB)
//   9     Interrupt Mask Register
//      7  Interrupt enable (1=interrupts enabled)
//      6  End of Block mask (1=interrupt on end of block)
//      5  Verify error (1=interrupt on verify error)
//      4-0   unused (1 upon reading)
//   A  7-6   Address Control Register
//         00=increment both addresses
//         01=fix expansion address
//         10=fix C64 address
//         11=fix both addresses
//      5-0   unused (1 upon reading)
   


//______________________________________________________________________________________________

FLASHMEM void InitHndlr_REU()
{
   Serial.println("Hello from REU!");

   //set reg defaults:
   uint8_t REURegsInit[REUReg_NumRegs]={0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f, 0x3f};
   memcpy(REURegs, REURegsInit, REUReg_NumRegs);
}

void IO2Hndlr_REU(uint8_t Address, bool R_Wn)
{
//   #ifndef DbgIOTraceLog
//      BigBuf[BigBufCount] = Address; //initialize w/ address 
//   #endif
   Address &= 0x1f; //only 5 register adress lines, regs are ghosted over $DFxx 8x
   if (R_Wn) //High (IO1 Read)
   {
      if (Address<REUReg_NumRegs) DataPortWriteWaitLog(REURegs[Address]);  
      if (Address == REUReg_Status) REURegs[REUReg_Status] &= 0x1f; //clear bits 7:5
         
      //DataPortWriteWaitLog(0); //respond to all reads
      //BigBuf[BigBufCount] |= (0<<8) | IOTLDataValid;
      //Serial.printf("Rd $de%02x\n", Address);
   }
   else  // IO1 write
   {
      if (Address<REUReg_NumRegs) REURegs[Address] = DataPortWaitRead();  



      //BigBuf[BigBufCount] |= (DataPortWaitRead()<<8) | IOTLDataValid;
      //Serial.printf("wr $de%02x:$%02x\n", Address, Data);
   }
//   #ifndef DbgIOTraceLog
//      if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
//      if (BigBufCount < BigBufSize) BigBufCount++;
//   #endif
}

void PollingHndlr_REU()
{

}

