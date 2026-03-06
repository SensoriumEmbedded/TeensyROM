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


//#define DbgMsgs_REU    //enable REU debug msgs
#define REU_Size       0x01000000   // 128k (0x00020000) to 16M (0x01000000) on 2^X boundries


#define REU_Sise_Mask  (REU_Size-1)  // 17 to 24 bit addr bus size
#ifdef DbgMsgs_REU
   #define Printf_dbg_reu Serial.printf
#else
   __attribute__((always_inline)) inline void Printf_dbg_reu(...) {};
#endif

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

extern void PerformDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer, uint32_t Length, bool FixC64Addr);
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
File REUFile = NULL;

   
FASTRUN bool REU_FF00_W_Check(uint16_t Address, bool R_Wn)
{
   if (Address == 0xff00 && !R_Wn) 
   { //Trigger REU start NOW
      DMA_State = DMA_S_StartActive;   //activate immediately
      fBusSnoop = NULL;
   }
   return false;  //continue cycle processing to start DMA
}

FLASHMEM void ReadWriteREU(bool RnW, uint32_t REUAddr, uint8_t *REUBuf, uint16_t REULength, bool FixREUAddr)
{
   
   uint32_t StartuS = micros();

   //REUFile = SD.open("/temp.reu", FILE_WRITE);  //already open
   if (!REUFile)
   {
      Printf_dbg_reu("Couldn't access REU temp file\n");
      return;
   }
   REUFile.flush();  //flush through any writes from previous access

   Printf_dbg_reu(" %luuS Check,", micros()-StartuS);
   
   REUFile.seek(REUAddr);
   
   if (RnW)
   {  //read file
      if (FixREUAddr) 
      {
         REUFile.read(REUBuf, 1);  //just read from first location
         for (uint16_t count = 1; count < REULength; count++) REUBuf[count]=REUBuf[0]; //and copy to the rest
      }
      else REUFile.read(REUBuf, REULength);
   }
   else
   {  //write file
      if (FixREUAddr) REUFile.write(REUBuf+REULength-1, 1);  //just write last byte to first buffer location
      else REUFile.write(REUBuf, REULength);
   }
   
   //REUFile.flush();  //flush through any writes.  Don't wait here for this, flush at start of next cycle
   //REUFile.close();  //leave open 
   Printf_dbg_reu(" %luuS Check+R/W\n", micros()-StartuS);
}


//______________________________________________________________________________________________

FLASHMEM void InitHndlr_REU()
{
   Printf_dbg_reu("Hello from REU!\n");

   fBusSnoop = NULL;
   //set reg defaults:
   uint8_t REURegsInit[REUReg_NumRegs]={0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f, 0x3f};
   memcpy(REURegs, REURegsInit, REUReg_NumRegs);
   
   uint32_t StartmS = millis();
   REUFile = SD.open("/temp.reu", FILE_WRITE);
   if (!REUFile)
   {
      Printf_dbg_reu("Couldn't open/create REU temp file\n");
   }
   else
   {
      //check for temp file size, make it REU_Size if less
      uint32_t REUFileSize = REUFile.size();
      Printf_dbg_reu("REU file size: $%08x", REUFileSize);
      while (REUFileSize < REU_Size) 
      {
         REUFile.write(0); //fill with zeros to REU size
         REUFileSize++;
      }
      REUFile.flush();  //flush through writes
      //REUFile.close();  //leave open 
      Printf_dbg_reu("  increased to $%08x in %lumS\n", REUFileSize, millis()-StartmS);
   }
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
   if (REULength == 0) REULength = 0x10000;  //full 64k if set to zero
   uint8_t *REUBuf = (uint8_t*)malloc(REULength); //allocate space
   uint32_t REUAddr = REU_Sise_Mask & (REURegs[REUReg_REUStartAddrLo] + 256*REURegs[REUReg_REUStartAddrMed] + 256*256*REURegs[REUReg_REUStartAddrHi]);
   uint32_t C64Addr = REURegs[REUReg_C64StartAddrLo] + 256*REURegs[REUReg_C64StartAddrHi];
   
   bool FixC64Addr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixC64;
   bool FixREUAddr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixREU;
   
   Printf_dbg_reu("Execute REU x-fer\n");
   Printf_dbg_reu(" Reg start: Stat:$%02x Cmd:$%02x C64:$%02x%02x REU:$%02x%02x%02x Len:$%02x%02x IntM:$%02x AddC:$%02x\n",
      REURegs[REUReg_Status], REURegs[REUReg_Command], REURegs[REUReg_C64StartAddrHi], REURegs[REUReg_C64StartAddrLo], 
      REURegs[REUReg_REUStartAddrHi], REURegs[REUReg_REUStartAddrMed], REURegs[REUReg_REUStartAddrLo], 
      REURegs[REUReg_TransLengthHi], REURegs[REUReg_TransLengthLo], REURegs[REUReg_InterruptMask], REURegs[REUReg_AddressControl]);

   switch (REURegs[REUReg_Command] & REUReg_Command_TypeMask)
   {
      case REUReg_Command_TypeC2R:
         PerformDMA(true, C64Addr, REUBuf, REULength, FixC64Addr); //read C64 into buffer
         ReadWriteREU(false, REUAddr, REUBuf, REULength, FixREUAddr);       //Write to REU
         break;
      case REUReg_Command_TypeR2C:
         ReadWriteREU(true, REUAddr, REUBuf, REULength, FixREUAddr);      //read REU into buffer
         PerformDMA(false, C64Addr, REUBuf, REULength, FixC64Addr); //write to C64
         break;
      case REUReg_Command_TypeSwp:
      {  //read both and swap
         uint8_t *C64Buf = (uint8_t*)malloc(REULength); //allocate space
         PerformDMA(true, C64Addr, C64Buf, REULength, FixC64Addr); //read C64 into C64Buf 
         ReadWriteREU(true, REUAddr, REUBuf, REULength, FixREUAddr); //read REU into REUBuf 
         
         ReadWriteREU(false, REUAddr, C64Buf, REULength, FixREUAddr); //write C64Buf into REU
         PerformDMA(false, C64Addr, REUBuf, REULength, FixC64Addr); //write REUBuf into C64

         free(C64Buf);
      }
         break;
         
      case REUReg_Command_TypeVer:
      {  //read both and verify
         uint8_t *C64Buf = (uint8_t*)malloc(REULength); //allocate space
         uint32_t ByteNum = 0;
         PerformDMA(true, C64Addr, C64Buf, REULength, FixC64Addr); //read C64 into C64Buf
         ReadWriteREU(true, REUAddr, REUBuf, REULength, FixREUAddr); //read REU into REUBuf
         
         while (ByteNum < REULength)
         {  //Compare the two buffers
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
               //todo: update address regs with current/miscompare address(ByteNum)
               //  small conflict with Process Address Control Register below...
               break;
            }
            ByteNum++;
         }
         free(C64Buf);
      }
         break;
   }
   
// REU Execution complete
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
      if (!FixREUAddr)
      {  //not fixed address, show final count
         REUAddr += REULength;
         REURegs[REUReg_REUStartAddrLo]  =      REUAddr  & 0xff;
         REURegs[REUReg_REUStartAddrMed] =  (REUAddr>>8) & 0xff;
         REURegs[REUReg_REUStartAddrHi]  = (REUAddr>>16) & 0xff;
      }
      
      if (!FixC64Addr)
      {  //not fixed address, show final count
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

