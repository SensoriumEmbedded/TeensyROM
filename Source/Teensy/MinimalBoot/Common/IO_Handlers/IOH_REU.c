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

//REU debug msgs
   #define Printf_dbg_reu Serial.printf
   //__attribute__((always_inline)) inline void Printf_dbg_reu(...) {};


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

extern void PerformDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer, uint32_t Length, bool CloseDMA);
extern void CloseDMA();


enum enumREUregs
{
   // REU registers:
   REUReg_Status = 0,
   REUReg_Command,
   REUReg_C64StartAddrLo,
   REUReg_C64StartAddrHi,
   REUReg_REUStartAddrLo,
   REUReg_REUStartAddrMed,
   REUReg_REUStartAddrHi,
   REUReg_TransLengthLo,
   REUReg_TransLengthHi,
   REUReg_InterruptMask,
   REUReg_AddressControl,

   REUReg_NumRegs,
};

#define REUReg_Status_IntPend    0b10000000
#define REUReg_Status_Complete   0b01000000
#define REUReg_Status_Fault      0b00100000

#define REUReg_Command_Execute   0b10000000
#define REUReg_Command_Load      0b00100000
#define REUReg_Command_FF00      0b00010000
#define REUReg_Command_TypeMask  0b00000011
#define REUReg_Command_TypeC2R   0b00000000
#define REUReg_Command_TypeR2C   0b00000001
#define REUReg_Command_TypeSwp   0b00000010
#define REUReg_Command_TypeVer   0b00000011

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
   
FASTRUN bool REU_FF00_W_Check(uint16_t Address, bool R_Wn)
{
   if (Address == 0xff00 && !R_Wn) 
   { //Trigger REU start NOW
      DMA_State = DMA_S_StartActive;   //activate immediately
      fBusSnoop = NULL;
   }
   return false;  //continue cycle processing to start DMA
}

FLASHMEM void WriteToREU(uint32_t REUAddr, uint8_t *REUBuf, uint16_t REULength)
{
   
   //if(!myFile)
   //{
      //char FullFilePath[MaxNamePathLength];

      uint32_t StartuS = micros();

      //if (PathIsRoot()) sprintf(FullFilePath, "%s%s", DriveDirPath, DriveDirMenu.Name);  // at root
      //else sprintf(FullFilePath, "%s/%s", DriveDirPath, DriveDirMenu.Name);
         
      //Printf_dbg_reu("Loading:\r\n%s", FullFilePath);

      File myFile = SD.open("/temp.reu", FILE_WRITE);
      
      if (!myFile) 
      {
         Printf_dbg_reu(" File Not Found\n");
         return;
      }
      Printf_dbg_reu(" %luuS Open,", micros()-StartuS);
   //}
   
   myFile.seek(REUAddr);
   //for (uint16_t count = 0; count < REULength; count++) REUBuf[count]=myFile.read();
   myFile.write(REUBuf, REULength);
   
   myFile.close();
   Printf_dbg_reu(" %luuS Open+Write+Close\n", micros()-StartuS);

   
}

FLASHMEM void ReadFromREU(uint32_t REUAddr, uint8_t *REUBuf, uint16_t REULength)
{
   uint32_t StartuS = micros();

   File myFile = SD.open("/temp.reu", FILE_READ);
   
   if (!myFile) 
   {
      Printf_dbg_reu(" File Not Found\n");
      return;
   }
   Printf_dbg_reu(" %luuS Open,", micros()-StartuS);
   
   myFile.seek(REUAddr);
   //for (uint16_t count = 0; count < REULength; count++) REUBuf[count]=myFile.read();
   myFile.read(REUBuf, REULength);
   
   myFile.close();
   Printf_dbg_reu(" %luuS Open+Read+Close\n", micros()-StartuS);
}

//______________________________________________________________________________________________

FLASHMEM void InitHndlr_REU()
{
   Printf_dbg_reu("Hello from REU!\n");

   fBusSnoop = NULL;
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
   if (R_Wn) //High (IO2 Read)
   {
      if (Address < REUReg_NumRegs) DataPortWriteWait(REURegs[Address]);  
      
      if (Address == REUReg_Status) REURegs[REUReg_Status] &= 0x1f; //clear bits 7:5

      //DataPortWriteWaitLog(0); //respond to all reads
      //BigBuf[BigBufCount] |= (0<<8) | IOTLDataValid;
      //Printf_dbg_reu("Rd $de%02x\n", Address);
   }
   else  // IO2 write
   {
      if (Address < REUReg_NumRegs) REURegs[Address] = DataPortWaitRead();  

      if (Address == REUReg_Command)
      {
         if (REURegs[Address] & REUReg_Command_Execute)
         {
            if (REURegs[Address] & REUReg_Command_FF00)
            { //Trigger REU start NOW
               DMA_State = DMA_S_StartActive;      //activate immediately
            }
            else
            { //delay trigger for FF00 Write
               fBusSnoop = &REU_FF00_W_Check; 
            }
         }
      }

      //BigBuf[BigBufCount] |= (DataPortWaitRead()<<8) | IOTLDataValid;
      //SPrintf_dbg_reu("wr $de%02x:$%02x\n", Address, Data);
   }
//   #ifndef DbgIOTraceLog
//      if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
//      if (BigBufCount < BigBufSize) BigBufCount++;
//   #endif
}

FLASHMEM void PollingHndlr_REU()
{
   if (DMA_State != DMA_S_ActiveReady) return;

   //DMA asserted, Do REU transfer
   uint32_t StartTime = micros();  
   uint32_t REULength = REURegs[REUReg_TransLengthLo] + 256*REURegs[REUReg_TransLengthHi];
   uint32_t REUAddr = REURegs[REUReg_REUStartAddrLo] + 256*REURegs[REUReg_REUStartAddrMed] + 256*256*REURegs[REUReg_REUStartAddrHi];
   uint32_t C64Addr = REURegs[REUReg_C64StartAddrLo] + 256*REURegs[REUReg_C64StartAddrHi];
   uint8_t *REUBuf = (uint8_t*)malloc(REULength); //allocate space
   
   Printf_dbg_reu("Execute REU x-fer type %d for %d bytes\n", REURegs[REUReg_Command] & REUReg_Command_TypeMask, REULength);
   Printf_dbg_reu(" addr C64: $%04x  REU: $%06x\n", C64Addr, REUAddr);

   switch (REURegs[REUReg_Command] & REUReg_Command_TypeMask)
   {
      case REUReg_Command_TypeC2R:
         PerformDMA(true, C64Addr, REUBuf, REULength, false); //read into buffer
         WriteToREU(REUAddr, REUBuf, REULength);
         break;
      case REUReg_Command_TypeR2C:
         ReadFromREU(REUAddr, REUBuf, REULength);
         PerformDMA(false, C64Addr, REUBuf, REULength, false); //write to C64
         break;
      case REUReg_Command_TypeSwp:
         //read both and swap
         
         break;
      case REUReg_Command_TypeVer:
      {  //read both and verify
         uint8_t *C64Buf = (uint8_t*)malloc(REULength); //allocate space
         uint32_t ByteNum = 0;
         PerformDMA(true, C64Addr, C64Buf, REULength, false); //read C64 into buffer
         ReadFromREU(REUAddr, REUBuf, REULength); //read REU into buffer
         
         while (ByteNum < REULength)
         {
            if(REUBuf[ByteNum] != C64Buf[ByteNum]) 
            {
               REURegs[REUReg_Status] |= REUReg_Status_Fault;
               Printf_dbg_reu(" --Miscompare!-- at $%04x\n", ByteNum);
               //todo: update address regs with current/miscompare address(?)
               break;
            }
            ByteNum++;
         }
         //if (ByteNum == REULength) Printf_dbg_reu(" Pass\n");
         //else Printf_dbg_reu(" Fail\n");
         free(C64Buf);
      }
         break;
   }
   
   free(REUBuf);
   
   
   
   
// Process Interrupt Mask Register



// Process Address Control Register
   
   
   
   
   REURegs[REUReg_Status] |= REUReg_Status_Complete;   
   
   StartTime = micros() - StartTime;  
   Printf_dbg_reu(" REU xfer took %luuS\n", StartTime);
   Serial.flush();
   
   CloseDMA();  //release DMA last so no cycles have passed on the 6510
}

