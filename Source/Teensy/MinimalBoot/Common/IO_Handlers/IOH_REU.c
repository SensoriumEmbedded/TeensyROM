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


//#define DbgMsgs_REU        //enable REU debug msgs 
//#define Direct_REU           //use DirectREU Method
#define USE_PSRAM          //use external PSRAM chip instead of SD
#define REU_Size           0x01000000   // 128k (0x00020000) to 16M (0x01000000) on 2^X boundries
#define REU_Temp_FileName  "/temp.reu"


#define REU_Sise_Mask      (REU_Size-1)  // 17 to 24 bit addr bus size
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
//tests:
//   REU Checker 1.0:      General check/integrity
//   CMD 1750/XL REU Test: All protocol checks
//   Sonic:                Speed test
//   NUVIES:               Faster speed test

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
extern "C" uint8_t external_psram_size;
uint8_t *pPSRAM = (uint8_t *)(0x70000000);

#ifdef Direct_REU

#define ModREUAddr       (FixREUAddr ? REU_StartAddr : REU_StartAddr+DMA_Count)

extern bool DMA_RnW, DMA_FixC64Addr;
extern uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
extern bool DMAByte(uint8_t *Data);

void DMAByte_BASkip(uint8_t *Data1)
{
   while (!DMAByte(Data1))
   {  //skip cycle if bus not available
      //while (!GP9_BA(ReadGPIO9));  // bus not available, wait until it is
      ////re-align to clock
      //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
      //while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
      //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
      //while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
      while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
      while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
      StartCycCnt = ARM_DWT_CYCCNT;
   }
}

void DirectREU()
{  //sent here directly from *within the ISR* to incorporate DMA and (PS)RAM REU transfers
   // Either from REU_FF00_W_Check or REUReg_Command_Execute

   uint32_t StartTime = micros();  

   uint8_t Data1, Data2;
   uint32_t REU_StartAddr = REU_Sise_Mask & (REURegs[REUReg_REUStartAddrLo] + 256*REURegs[REUReg_REUStartAddrMed] + 256*256*REURegs[REUReg_REUStartAddrHi]);
   DMA_Length = REURegs[REUReg_TransLengthLo] + 256*REURegs[REUReg_TransLengthHi];
   if (DMA_Length == 0) DMA_Length = 0x10000;  //full 64k if set to zero
   DMA_Count = 0;
   DMA_StartAddr = REURegs[REUReg_C64StartAddrLo] + 256*REURegs[REUReg_C64StartAddrHi];
   
   DMA_FixC64Addr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixC64;
   bool FixREUAddr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixREU;
   
   //Assert DMA in following VIC phase (IO write just occured)
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
   StartCycCnt = ARM_DWT_CYCCNT;
   WaitUntil_nS(nS_DMAAssert); 
   SetDMAAssert;
   
   Printf_dbg_reu("Execute REU x-fer\n");
   Printf_dbg_reu(" Reg start: Stat:$%02x Cmd:$%02x C64:$%02x%02x REU:$%02x%02x%02x Len:$%02x%02x IntM:$%02x AddC:$%02x\n",
      REURegs[REUReg_Status], REURegs[REUReg_Command], REURegs[REUReg_C64StartAddrHi], REURegs[REUReg_C64StartAddrLo], 
      REURegs[REUReg_REUStartAddrHi], REURegs[REUReg_REUStartAddrMed], REURegs[REUReg_REUStartAddrLo], 
      REURegs[REUReg_TransLengthHi], REURegs[REUReg_TransLengthLo], REURegs[REUReg_InterruptMask], REURegs[REUReg_AddressControl]);
   
   //wait for 6510 to give up the bus
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)

   //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)  <move this inside loop?????

   while(DMA_Count < DMA_Length)
   {
      //align to falling edge:
      while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)  <move this out of loop?????
      while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
      StartCycCnt = ARM_DWT_CYCCNT;
   
      switch (REURegs[REUReg_Command] & REUReg_Command_TypeMask)
      {
         case REUReg_Command_TypeC2R:
            //read C64 in x-fer phase, then write to REU in VIC
            DMA_RnW = true; //true=read, false=write
            DMAByte_BASkip(&Data1);
            pPSRAM[ModREUAddr] = Data1;
            break;
         case REUReg_Command_TypeR2C:
            //read from REU in VIC, then write C64 in x-fer phase 
            Data1 = pPSRAM[ModREUAddr];
            DMA_RnW = false; //true=read, false=write
            DMAByte_BASkip(&Data1);
            break;
         case REUReg_Command_TypeSwp:
            //read both and swap
            DMA_RnW = true; //true=read, false=write
            DMAByte_BASkip(&Data2); //read C64 into Data2
            Data1 = pPSRAM[ModREUAddr]; //read REU into Data1
            
            //clock/part 2:
            //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)  <Needed?????
            while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
            StartCycCnt = ARM_DWT_CYCCNT;

            DMA_RnW = false; //true=read, false=write
            DMAByte_BASkip(&Data1);  //write Data1 to C64
            pPSRAM[ModREUAddr] = Data2; //write Data2 to REU
            break;
            
         case REUReg_Command_TypeVer:
            //read both and verify
            DMA_RnW = true; //true=read, false=write
            DMAByte_BASkip(&Data2); //read C64 into Data2
            Data1 = pPSRAM[ModREUAddr]; //read REU into Data1

            if(Data1 != Data2) 
            {
               Printf_dbg_reu(" --Miscompare!-- C64 $%04x=$%02x  REU $%06x=$%02x\n", DMA_StartAddr+DMA_Count, Data2, REU_StartAddr+DMA_Count, Data1);
               REURegs[REUReg_Status] |= REUReg_Status_Fault;
               if ((REURegs[REUReg_InterruptMask] & REUReg_IntMask_Enable) &&
                   (REURegs[REUReg_InterruptMask] & REUReg_IntMask_VerifErr))
               {
                  REURegs[REUReg_Status] |= REUReg_Status_IntPend;
                  SetIRQAssert;
               }
               //todo: update address regs with current/miscompare address(DMA_Count)
               //  small conflict with Process Address Control Register below...
               break; //meant to break out of while loop, but...
            }
            break;
      }
      DMA_Count++;
   }
   
  
// REU Execution complete
   Printf_dbg_reu(" Type %d  Len: $%04x ", REURegs[REUReg_Command] & REUReg_Command_TypeMask, DMA_Length);
   Printf_dbg_reu(" C64: $%04x  REU: $%06x  data1: $%02x\n", DMA_StartAddr, REU_StartAddr, Data1);
      
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
         REU_StartAddr += DMA_Length;
         REURegs[REUReg_REUStartAddrLo]  =      REU_StartAddr  & 0xff;
         REURegs[REUReg_REUStartAddrMed] =  (REU_StartAddr>>8) & 0xff;
         REURegs[REUReg_REUStartAddrHi]  = (REU_StartAddr>>16) & 0xff;
      }
      
      if (!DMA_FixC64Addr)
      {  //not fixed address, show final count
         DMA_StartAddr += DMA_Length;
         REURegs[REUReg_C64StartAddrLo]  =      DMA_StartAddr & 0xff;
         REURegs[REUReg_C64StartAddrHi]  = (DMA_StartAddr>>8) & 0xff;
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
   Printf_dbg_reu(" Type %d REU xfer took %luuS\n", REURegs[REUReg_Command] & REUReg_Command_TypeMask, StartTime);
   Serial.flush();
   
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
   //StartCycCnt = ARM_DWT_CYCCNT;
   SetDMADeassert;
}
#endif

   
FASTRUN bool REU_FF00_W_Check(uint16_t Address, bool R_Wn)
{
   if (Address == 0xff00 && !R_Wn) 
   { //Trigger REU start NOW
      fBusSnoop = NULL;
#ifdef Direct_REU
      DirectREU();
      return true; //skip the rest of the cycle, DMA occured
#else
      DMA_State = DMA_S_StartActive;   //activate immediately
#endif
   }
   return false;  //continue cycle
}

#ifdef USE_PSRAM
FLASHMEM void ReadWriteREU(bool RnW, uint32_t REUAddr, uint8_t *REUBuf, uint16_t REULength, bool FixREUAddr)
{
   uint32_t StartuS = micros();
      
   //arm_dcache_flush((void *)pPSRAM, REU_Size);
   if (RnW)
   {  //read PSRAM
      if (FixREUAddr) 
      {
         REUBuf[0] = pPSRAM[REUAddr]; // read first location...
         for (uint16_t count = 1; count < REULength; count++) REUBuf[count]=REUBuf[0]; // and copy to the rest
      }
      else for(uint32_t cnt = 0; cnt<REULength; cnt++) REUBuf[cnt]=pPSRAM[REUAddr+cnt];
   }
   else
   {  //write PSRAM
      if (FixREUAddr) pPSRAM[REUAddr]=REUBuf[REULength-1];  //memcpy(pPSRAM, REUBuf+REULength-1, 1); //just write last byte to first buffer location
      else for(uint32_t cnt = 0; cnt<REULength; cnt++) pPSRAM[REUAddr+cnt]=REUBuf[cnt];
   }
   
   Printf_dbg_reu(" %luuS PSRAM R/W\n", micros()-StartuS);
}
#else

File REUFile = NULL;

FLASHMEM void ReadWriteREU(bool RnW, uint32_t REUAddr, uint8_t *REUBuf, uint16_t REULength, bool FixREUAddr)
{
   uint32_t StartuS = micros();

   //REUFile = SD.open(REU_Temp_FileName, FILE_WRITE);  //already open
   if (!REUFile)
   {
      Printf_dbg_reu("Couldn't access REU temp file\n");
      return;
   }
   REUFile.flush();  //flush through any writes from previous access

   Printf_dbg_reu(" %luuS Check/Flush,", micros()-StartuS);
   
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
#endif


//______________________________________________________________________________________________

FLASHMEM void InitHndlr_REU()
{
   Printf_dbg_reu("Hello from REU!\n");

   fBusSnoop = NULL;
   //set reg defaults:
   uint8_t REURegsInit[REUReg_NumRegs]={0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f, 0x3f};
   memcpy(REURegs, REURegsInit, REUReg_NumRegs);
   
#ifdef USE_PSRAM
   uint8_t size = external_psram_size;
   Serial.printf("PSRAM Memory: %dMB\n", size);
   if (size == 0) return;
   const float clocks[4] = {396.0f, 720.0f, 664.62f, 528.0f};
   const float frequency = clocks[(CCM_CBCMR >> 8) & 3] / (float)(((CCM_CBCMR >> 29) & 7) + 1);
   Serial.printf(" CCM_CBCMR=%08X (%.1f MHz)\n", CCM_CBCMR, frequency);
   //memory_end = (uint32_t *)(0x70000000 + size * 1048576);
   //pPSRAM = (uint8_t *)(0x70000000);
   //if (!pPSRAM) pPSRAM = (uint8_t*)extmem_malloc(REU_Size);
   if (!pPSRAM) Serial.println(" PSRAM not allocated!");
   else Serial.printf(" PSRAM loc: $%08x\n", pPSRAM);
#else
   uint32_t StartmS = millis();
   REUFile = SD.open(REU_Temp_FileName, FILE_WRITE);
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
#endif
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
               fBusSnoop = NULL; //in case it was activated then turned off before trigger
#ifdef Direct_REU
               DirectREU();
#else
               DMA_State = DMA_S_StartActive;   //activate immediately
#endif
            }
            else
            { //delay trigger for FF00 Write
               fBusSnoop = &REU_FF00_W_Check; 
            }
         }
      }

      //BigBuf[BigBufCount] |= (DataPortWaitRead()<<8) | IOTLDataValid;
      //Printf_dbg_reu("reuwr $de%02x:$%02x\n", Address, REURegs[Address]);
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

