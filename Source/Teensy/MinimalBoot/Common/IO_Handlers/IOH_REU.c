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
#define Direct_REU         //use DirectREU Method instead of buffer

//choose one for REU swap buffer;  direct: PSRAM/RAM12 only, Indirect: SD/PSRAM only
#define USE_RAM12          //use RAM1/2 space
//#define USE_PSRAM          //use external PSRAM chip(s)
//#define USE_SD             //use SD card`


#ifdef USE_RAM12
   #define REU_Size           0x00080000   // 512k  Range: 128k (0x00020000) to 16M (0x01000000) on 2^X boundaries
   #define REU_RAM_Bank_Size   0x2000
   #define REU_RAM_READ(a,d)   d=CrtChips[a/REU_RAM_Bank_Size].ChipROM[a%REU_RAM_Bank_Size]
   #define REU_RAM_WRITE(a,d)  CrtChips[a/REU_RAM_Bank_Size].ChipROM[a%REU_RAM_Bank_Size]=d
#elif defined(USE_PSRAM)
   #define REU_Size           0x01000000   // 16M   Range: 128k (0x00020000) to 16M (0x01000000) on 2^X boundaries
   uint8_t *pPSRAM = (uint8_t *)(0x70000000);
   extern "C" uint8_t external_psram_size;
   #define REU_RAM_READ(a,d)   d=pPSRAM[a]
   #define REU_RAM_WRITE(a,d)  pPSRAM[a]=d
#elif defined(USE_SD)
   #define REU_Size           0x01000000   // 16M   Range: 128k (0x00020000) to 16M (0x01000000) on 2^X boundaries
   #define REU_Temp_FileName  "/temp.reu"
#endif

#define REU_Size_Mask      (REU_Size-1)  // 17 to 24 bit addr bus size
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
  "512KB REU",         //Name of handler
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
extern void (*fSpecialBtnChange)(bool Up_nDn);  //Pointer to function called when Special Button Changes
extern void EEPreadStr(uint16_t addr, char* buf);
extern RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName);
extern bool SDFullInit();
extern bool USBFileSystemWait();
extern FS *FSfromSourceID(RegMenuTypes SourceID);
//extern uint8_t RAM2blocks();

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

#ifdef Direct_REU

extern bool DMA_RnW, DMA_FixC64Addr;
extern uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
extern bool DMAByte(uint8_t *Data);

extern void FreeCrtChips();
extern void FreeDriveDirMenu();
extern uint8_t RAM_Image[];
extern StructCrtChip CrtChips[];
extern uint8_t NumCrtChips;
extern StructMenuItem *DriveDirMenu;

void DMAByte_BASkip(uint8_t *Data1)
{
   while (!DMAByte(Data1))
   {  //skip cycle if bus not available
      //re-align to clock
      //while (!GP9_BA(ReadGPIO9)); // bus not available, wait until it is
      while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
      while(GP6_Phi2(ReadGPIO6));   //Find phi2 falling (start VIC phase)
      StartCycCnt = ARM_DWT_CYCCNT;
   }
}

void DirectREU()
{  //sent here directly from *within the ISR* to incorporate DMA and (PS)RAM REU transfers
   // Either from REU_FF00_W_Check or REUReg_Command_Execute

   uint32_t StartTime = micros();  

   uint8_t Data1, Data2;
   uint32_t REU_StartAddr = REU_Size_Mask & (REURegs[REUReg_REUStartAddrLo] + 256*REURegs[REUReg_REUStartAddrMed] + 256*256*REURegs[REUReg_REUStartAddrHi]);
   DMA_Length = REURegs[REUReg_TransLengthLo] + 256*REURegs[REUReg_TransLengthHi];
   if (DMA_Length == 0) DMA_Length = 0x10000;  //full 64k if set to zero
   DMA_Count = 0;
   DMA_StartAddr = REURegs[REUReg_C64StartAddrLo] + 256*REURegs[REUReg_C64StartAddrHi];
   
   DMA_FixC64Addr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixC64;
   bool FixREUAddr = REURegs[REUReg_AddressControl] & REUReg_AddrCont_FixREU;
   bool ErrOut = false;
   //uint32_t MisCount = 0;
   
   //Assert DMA in following VIC phase (IO write just occurred)
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

   while(DMA_Count < DMA_Length && !ErrOut)
   {
      uint32_t ModREUAddr = (FixREUAddr ? REU_StartAddr : REU_StartAddr+DMA_Count);

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
            REU_RAM_WRITE(ModREUAddr, Data1);
            break;
            
         case REUReg_Command_TypeR2C:
            //read from REU in VIC, then write C64 in x-fer phase 
            REU_RAM_READ(ModREUAddr, Data1); //read REU into Data1
#ifdef USE_PSRAM       
            //Look for slow PSRAM read and give an extra clock if needed
            //too slow fails size detect in both REU tests
            if(ARM_DWT_CYCCNT - StartCycCnt > nSToCyc(nS_DMASetup-90)) // 75: not enough,  80: intermittent, 85: OK (PAL)
            //NUVIES: 90, 95,105,125: silent works but glitchy; 85: nope
            {  //missed completing read within VIC cycle, wait for next one.
               //MisCount++;
               while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase, in case it's not there yet)
               while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
               StartCycCnt = ARM_DWT_CYCCNT;
            }
#endif
            DMA_RnW = false; //true=read, false=write
            DMAByte_BASkip(&Data1);
            break;
            
         case REUReg_Command_TypeSwp:
            //read both and swap
            REU_RAM_READ(ModREUAddr, Data1); //read REU into Data1
#ifdef USE_PSRAM       
            //Look for slow PSRAM read and give an extra clock if needed
            //fixes block missing pixels during Bit Fill in CMD 1750 Test:
            if(ARM_DWT_CYCCNT - StartCycCnt > nSToCyc(nS_DMASetup-85)) // nS_DMASetup
            {  //missed completing read within VIC cycle, wait for next one.
               //MisCount++;
               while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase, in case it's not there yet)
               while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
               StartCycCnt = ARM_DWT_CYCCNT;
            }
#endif
            DMA_RnW = true; //true=read, false=write
            DMAByte_BASkip(&Data2); //read C64 into Data2

            //clock/part 2:
            //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)  <Needed?????
            while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
            StartCycCnt = ARM_DWT_CYCCNT;

            DMA_RnW = false; //true=read, false=write
            DMAByte_BASkip(&Data1);  //write Data1 to C64
            REU_RAM_WRITE(ModREUAddr, Data2); //write Data2 to REU
            break;
            
         case REUReg_Command_TypeVer:
            //read both and verify
            DMA_RnW = true; //true=read, false=write
            DMAByte_BASkip(&Data2); //read C64 into Data2
            REU_RAM_READ(ModREUAddr, Data1); //read REU into Data1

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
               ErrOut = true; //break out of while loop...
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
   //Printf_dbg_reu(" Type %d REU xfer took %luuS, Miss Count=%lu\n", REURegs[REUReg_Command] & REUReg_Command_TypeMask, StartTime, MisCount);
   Serial.flush();
   
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising (start transfer phase)
   LastCycCnt = ARM_DWT_CYCCNT; //   Ready for next active cycle
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling (start VIC phase)
   //StartCycCnt = ARM_DWT_CYCCNT; //back to IRQ control
   SetDMADeassert;
}
#endif

   
bool REU_FF00_W_Check(uint16_t Address, bool R_Wn)
{
   if (Address == 0xff00 && !R_Wn) 
   { //Trigger REU start NOW
      fBusSnoop = NULL;
#ifdef Direct_REU
      DirectREU();
      return true; //skip the rest of the cycle, DMA occurred
#else
      DMA_State = DMA_S_StartActive;   //activate immediately
#endif
   }
   return false;  //continue cycle
}

#ifndef Direct_REU
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
#elif defined(USE_SD)

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
   //check was removed from SetUpMainMenuROM(), add it back if using this
   Printf_dbg_reu(" %luuS Check+R/W\n", micros()-StartuS);
}
#endif
#endif  // !Direct_REU

FLASHMEM void SpecialBtn_REU(bool Up_nDn)
{
   if(Up_nDn) 
   {  //on button release, save REU contents

      //Create filename based on current. reu000.reu becomes reu001.reu, etc
      char Filename[MaxPathLength];
      EEPreadStr(eepAdREUFilename, Filename);
      char *ptrFileName = Filename; //pointer to move past SD/USB/TR:
      RegMenuTypes MenuSourceID = RegMenuTypeFromFileName(&ptrFileName);

      if (MenuSourceID == rmtSD) SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected
      if (MenuSourceID == rmtUSBDrive) USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up
      //rmtTeensy not allowed, no reu files in Teensy Mem
      
      FS *sourceFS = FSfromSourceID(MenuSourceID);

      char Extension[20];
      char *pDot = strrchr(ptrFileName, '.'); //find last dot
      if (pDot == NULL) pDot = ptrFileName + strlen(ptrFileName); //if no extension, set to end of filename 
      strcpy(Extension, pDot); //copy extension
      *pDot = 0; //terminate at dot/extension
      
      bool IsNum = true; //are last 3digits of filename all numbers?
      if (strlen(ptrFileName)>=3)
      {
         for(char *charloc=pDot-3; charloc<pDot; charloc++)
         {  
            if (*charloc<'0' || *charloc>'9') IsNum = false;
            //Serial.printf("%08x: %c %02x\n", (uint32_t)charloc, *charloc, *charloc);
         }
      }
      else IsNum = false;
      
      
      uint16_t NewNum = 0;
      if (IsNum)
      {  //find current number and then terminate it
         NewNum = atoi(pDot-3);
         *(pDot-3) = 0; //terminate number
      }
      
      //now see if the file exists, or itterate until it doesn't
      char NewFilename[MaxPathLength];

      do sprintf(NewFilename, "%s%03d%s", ptrFileName, NewNum++, Extension);
      while (sourceFS->exists(NewFilename));
      
      Serial.printf("Saving REU: %s\n", NewFilename);
      File SaveFile = sourceFS->open(NewFilename, FILE_WRITE);
      if (!SaveFile)
      {
         Serial.println("Unable to open!");
         return;
      }
            
      //uint32_t StartmS = millis();
      uint8_t NextByte;
      
      for (uint32_t CharNum=0; CharNum<REU_Size; CharNum++)
      {
         REU_RAM_READ(CharNum, NextByte);
         SaveFile.write(NextByte);
      }
      //Serial.printf("Saved %lu KBytes in %lumS\n", REU_Size/1024, millis()-StartmS);
      Serial.printf("Saved %luKBytes\n", REU_Size/1024);
      SaveFile.close();
      
      //Flash LED to ack save
      uint32_t LastmS = millis();
      bool LEDState = true;
      
      for (uint32_t FlashCount=0; FlashCount<20; FlashCount++)
      {
         while(millis()-LastmS < 75);
         LastmS = millis();
         if ((LEDState = !LEDState)) SetLEDOn;
         else SetLEDOff;
      }     
   }
}


//______________________________________________________________________________________________

FLASHMEM void InitHndlr_REU()
{
   fBusSnoop = NULL;
   //set reg defaults:
   uint8_t REURegsInit[REUReg_NumRegs]={0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f, 0x3f};
   memcpy(REURegs, REURegsInit, REUReg_NumRegs);

#ifdef Direct_REU
   Printf_dbg_reu("Direct REU mode\n");
#else
   Printf_dbg_reu("Indirect REU mode\n");
#endif   

#ifdef USE_RAM12
   //Allocate full REU size in Ram 1 and 2
   FreeCrtChips(); //re-using CrtChips for this, free mem allocated in RAM2 and reset NumCrtChips
   FreeDriveDirMenu(); //free/clear prev loaded directory to make space. Doing it regardless to preserve continuity
   //Serial.printf("RAM2#x- %d\n", RAM2blocks());

   uint8_t *pRAM_Image = RAM_Image;

   Serial.printf("%dk REU, (%d banks x %d bytes)\n", REU_Size/1024, REU_Size/REU_RAM_Bank_Size, REU_RAM_Bank_Size);
   //First use RAM_Image from RAM1:
   while ((uint32_t)pRAM_Image - (uint32_t)RAM_Image + REU_RAM_Bank_Size <= RAM_ImageSize)
   {
      CrtChips[NumCrtChips].ChipROM = pRAM_Image;
      pRAM_Image += REU_RAM_Bank_Size;
      NumCrtChips++;
   }
   Printf_dbg_reu("Used %lu/%lu Bytes of RAM1 Image\n", (uint32_t)pRAM_Image-(uint32_t)RAM_Image, RAM_ImageSize);

   //allocate the rest from RAM2
   while(NumCrtChips < REU_Size/REU_RAM_Bank_Size)
   {
      if (NULL == (CrtChips[NumCrtChips].ChipROM = (uint8_t*)malloc(REU_RAM_Bank_Size)))
      {
         //if (DriveDirMenu == NULL) //doing this here fragments RAM2
         Serial.printf("alloc err bank %d!\n", NumCrtChips);
         //Serial.flush(); //doesn't flush Tx before reboot?
         delay(250);
         REBOOT; //no better way to fail...
      }
      NumCrtChips++;
   }
   Printf_dbg_reu("Used %lu bytes from RAM2, %luK bytes REU total\n", REU_Size+(uint32_t)RAM_Image-(uint32_t)pRAM_Image, REU_Size/1024);

   
   //pre-load REU here:
   
   // char Filename[]="/reu/Test PRGs/gillham test/ship512.reu";
   // 
   // Serial.printf("Loading REU: %s\n", Filename);
   // File LoadFile = SD.open(Filename, FILE_READ);
   // if (!LoadFile)
   // {
   //    Serial.println("Not found!");
   //    return;
   // }
   // 
   // if (LoadFile.size() > REU_Size)
   // {
   //    Serial.println("Too large!");
   //    return;
   // }
   // 
   // uint32_t StartmS = millis();
   // uint32_t CharNum = 0;
   // while (LoadFile.available())
   // {
   //    REU_RAM_WRITE(CharNum, LoadFile.read());
   //    CharNum++;
   // }
   // Serial.printf("Read %lu Bytes in %lumS\n", CharNum, millis()-StartmS);
   // LoadFile.close();
   
   
   
   
#elif defined(USE_PSRAM)
   //higher clock speed and shorter CS setup/hold do help, but still not enough for NUVIEs w/ sound

   // //ref: https://forum.pjrc.com/index.php?threads/faster-way-to-read-a-single-byte-from-flash-or-ext-psram.73428/
   // //set sclk to 132 Mhz: (default CCM_CBCMR=95AE8304 (105.6 MHz))
   // CCM_CCGR7 |= CCM_CCGR7_FLEXSPI2(CCM_CCGR_OFF);
   // CCM_CBCMR = (CCM_CBCMR & ~(CCM_CBCMR_FLEXSPI2_PODF_MASK | CCM_CBCMR_FLEXSPI2_CLK_SEL_MASK))
   //    | CCM_CBCMR_FLEXSPI2_PODF(4) | CCM_CBCMR_FLEXSPI2_CLK_SEL(2); // 528/5 = 132 MHz
   // CCM_CCGR7 |= CCM_CCGR7_FLEXSPI2(CCM_CCGR_ON);
   // 
   // //FlexSPI setup:
   // FLEXSPI2_INTEN = 0;
   // FLEXSPI2_FLSHA1CR0 = 0x2000; // 8 MByte    
   // //default: TCSH=3, TCSS=3 (CS Setup/Hold)
   // FLEXSPI2_FLSHA1CR1 = FLEXSPI_FLSHCR1_CSINTERVAL(2)
   //    | FLEXSPI_FLSHCR1_TCSH(0) | FLEXSPI_FLSHCR1_TCSS(0);
   // FLEXSPI2_FLSHA1CR2 = FLEXSPI_FLSHCR2_AWRSEQID(6) | FLEXSPI_FLSHCR2_AWRSEQNUM(0)
   //         | FLEXSPI_FLSHCR2_ARDSEQID(5) | FLEXSPI_FLSHCR2_ARDSEQNUM(0);

   uint8_t size = external_psram_size;
   Serial.printf("PSRAM Memory: %dMB\n", size);
   if (size == 0) return;
   const float clocks[4] = {396.0f, 720.0f, 664.62f, 528.0f};
   const float frequency = clocks[(CCM_CBCMR >> 8) & 3] / (float)(((CCM_CBCMR >> 29) & 7) + 1);
   Serial.printf(" CCM_CBCMR=%08X (%.1f MHz)\n", CCM_CBCMR, frequency);
   Serial.printf(" PSRAM loc: $%08x\n", pPSRAM);
   
#elif defined(USE_SD)
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

   fSpecialBtnChange = &SpecialBtn_REU;
 
}  //InitHndlr_REU


void IO2Hndlr_REU(uint8_t Address, bool R_Wn)
{
   #ifdef DbgIOTraceLog
      BigBuf[BigBufCount] = Address; //initialize w/ address 
   #endif
   Address &= 0x1f; //only 5 register address lines, regs are ghosted over $DFxx 8x
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
#ifndef Direct_REU
   //buffer/indirect implementation:
   if (DMA_State != DMA_S_ActiveReady) return;

//DMA asserted, Do REU transfer
   uint32_t StartTime = micros();  
   uint32_t REULength = REURegs[REUReg_TransLengthLo] + 256*REURegs[REUReg_TransLengthHi];
   if (REULength == 0) REULength = 0x10000;  //full 64k if set to zero
   uint8_t *REUBuf = (uint8_t*)malloc(REULength); //allocate space
   uint32_t REUAddr = REU_Size_Mask & (REURegs[REUReg_REUStartAddrLo] + 256*REURegs[REUReg_REUStartAddrMed] + 256*256*REURegs[REUReg_REUStartAddrHi]);
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
#endif
}

