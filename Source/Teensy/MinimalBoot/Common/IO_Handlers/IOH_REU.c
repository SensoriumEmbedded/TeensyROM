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

//ref: https://codebase64.net/doku.php?id=base:reu_registers

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
#define REUReg_Command_AutoLoad  0b00100000
#define REUReg_Command_FF00      0b00010000
#define REUReg_Command_TypeMask  0b00000011
#define REUReg_Command_TypeC2R   0b00000000
#define REUReg_Command_TypeR2C   0b00000001
#define REUReg_Command_TypeSwp   0b00000010
#define REUReg_Command_TypeVer   0b00000011

#define REUReg_IntMask_Enable    0b10000000
#define REUReg_IntMask_EndOfBlk  0b01000000
#define REUReg_IntMask_VerifErr  0b00100000

#define REUReg_AddrCont_Mask     0b11000000
#define REUReg_AddrCont_IncBoth  0b00000000
#define REUReg_AddrCont_FixREU   0b01000000
#define REUReg_AddrCont_FixC64   0b10000000
#define REUReg_AddrCont_FixBoth  0b11000000


//______________________________________________________________________________________________

uint8_t REURegs[REUReg_NumRegs];

   
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
   //todo: check for rollover
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
   #ifdef DbgIOTraceLog
      BigBuf[BigBufCount] = Address; //initialize w/ address 
   #endif
   Address &= 0x1f; //only 5 register adress lines, regs are ghosted over $DFxx 8x
   if (R_Wn) //High (IO2 Read)
   {
      if (Address < REUReg_NumRegs) DataPortWriteWaitLog(REURegs[Address]);  
      
      if (Address == REUReg_Status) 
      {
         REURegs[REUReg_Status] &= 0x1f; //clear bits 7:5
         SetIRQDeassert;
      }
   }
   else  // IO2 write
   {
      if (Address < REUReg_NumRegs && Address != REUReg_Status) 
      {
         REURegs[Address] = DataPortWaitRead();  
   #ifdef DbgIOTraceLog
         BigBuf[BigBufCount] |= (REURegs[Address]<<8) | IOTLDataValid;
   #endif
      }
      
      if (Address == REUReg_Command)
      {
         if (REURegs[Address] & REUReg_Command_Execute)
         {
            if (REURegs[Address] & REUReg_Command_FF00) //1=disable FF00 decode
            { //Trigger REU start NOW
               DMA_State = DMA_S_StartActive;      //activate immediately
               fBusSnoop = NULL; //in case it was activated then turned off before trigger
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
   #ifdef DbgIOTraceLog
      if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
      if (BigBufCount < BigBufSize) BigBufCount++;
   #endif
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
   
   if (REULength == 0) REULength = 0x10000;  //full 64k if set to zero
   
   Printf_dbg_reu("Execute REU x-fer\n");
   Printf_dbg_reu(" Reg start: Stat:$%02x Cmd:$%02x C64:$%02x%02x REU:$%02x%02x%02x Len:$%02x%02x IntM:$%02x AddC:$%02x\n",
      REURegs[REUReg_Status], REURegs[REUReg_Command], REURegs[REUReg_C64StartAddrHi], REURegs[REUReg_C64StartAddrLo], 
      REURegs[REUReg_REUStartAddrHi], REURegs[REUReg_REUStartAddrMed], REURegs[REUReg_REUStartAddrLo], 
      REURegs[REUReg_TransLengthHi], REURegs[REUReg_TransLengthLo], REURegs[REUReg_InterruptMask], REURegs[REUReg_AddressControl]);

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
               Printf_dbg_reu(" --Miscompare!-- at $%04x\n", ByteNum);
               REURegs[REUReg_Status] |= REUReg_Status_Fault;
               if ((REURegs[REUReg_InterruptMask] & REUReg_IntMask_Enable) &&
                   (REURegs[REUReg_InterruptMask] & REUReg_IntMask_VerifErr))
               {
                  REURegs[REUReg_Status] |= REUReg_Status_IntPend;
                  SetIRQAssert;
               }
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
   
   Printf_dbg_reu(" Type %d  Len: $%04x ", REURegs[REUReg_Command] & REUReg_Command_TypeMask, REULength);
   Printf_dbg_reu(" C64: $%04x  REU: $%06x  data[0]: $%02x\n", C64Addr, REUAddr, REUBuf[0]);

   free(REUBuf);
      
// Process Interrupt Mask Register
   if ((REURegs[REUReg_InterruptMask] & REUReg_IntMask_Enable) &&
       (REURegs[REUReg_InterruptMask] & REUReg_IntMask_EndOfBlk))
   {
      REURegs[REUReg_Status] |= REUReg_Status_IntPend;
      SetIRQAssert;
   }

// Process Address Control Register
   if ((REURegs[REUReg_Command] & REUReg_Command_AutoLoad) == 0)
   {  //autoload disabled
      //if ((REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixREU) == 0)
      {
         REUAddr += REULength;
         REURegs[REUReg_REUStartAddrLo]  =      REUAddr  & 0xff;
         REURegs[REUReg_REUStartAddrMed] =  (REUAddr>>8) & 0xff;
         REURegs[REUReg_REUStartAddrHi]  = (REUAddr>>16) & 0xff;
      }
      
      //if ((REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixC64) == 0)
      {
         C64Addr += REULength;
         REURegs[REUReg_C64StartAddrLo]  =      C64Addr & 0xff;
         REURegs[REUReg_C64StartAddrHi]  = (C64Addr>>8) & 0xff;
      }
      REURegs[REUReg_TransLengthHi] = 0;
      REURegs[REUReg_TransLengthLo] = 1;
   }
   
   REURegs[REUReg_Command] &= ~REUReg_Command_Execute;  //clear execution bit
   REURegs[REUReg_Status] |= REUReg_Status_Complete;   //flag complete
   
   StartTime = micros() - StartTime;  

   Printf_dbg_reu(" Reg end:   Stat:$%02x Cmd:$%02x C64:$%02x%02x REU:$%02x%02x%02x Len:$%02x%02x IntM:$%02x AddC:$%02x\n",
      REURegs[REUReg_Status], REURegs[REUReg_Command], REURegs[REUReg_C64StartAddrHi], REURegs[REUReg_C64StartAddrLo], 
      REURegs[REUReg_REUStartAddrHi], REURegs[REUReg_REUStartAddrMed], REURegs[REUReg_REUStartAddrLo], 
      REURegs[REUReg_TransLengthHi], REURegs[REUReg_TransLengthLo], REURegs[REUReg_InterruptMask], REURegs[REUReg_AddressControl]);
   Printf_dbg_reu(" REU xfer took %luuS\n", StartTime);

   Serial.flush();
   
   CloseDMA();  //release DMA last so no cycles have passed on the 6510
}

