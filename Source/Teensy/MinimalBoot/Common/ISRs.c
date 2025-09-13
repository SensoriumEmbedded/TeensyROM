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


FASTRUN void isrButton()
{
   BtnPressed = true;
}


//Phi2 rising edge:
FASTRUN void isrPHI2() 
{
   StartCycCnt = ARM_DWT_CYCCNT;
   uint32_t CycSinceLast = StartCycCnt-LastCycCnt;
   LastCycCnt = StartCycCnt;
   
   if (CycSinceLast > nSToCyc(nS_MaxAdj)) // If we're late, adjust...
   {
      StartCycCnt += nSToCyc(nS_MaxAdj) - CycSinceLast; 
      #ifdef DbgCycAdjLog      
         if (BigBuf != NULL)
         {
            BigBuf[BigBufCount] = CycSinceLast | AdjustedCycleTiming;
            if (BigBufCount < BigBufSize) BigBufCount++;
         }
      #endif
   }
   
#ifdef DbgSignalIsrPHI2
   SetDebugAssert;
#endif
   //if (DMA_State == DMA_S_ActiveReady) return;

   WaitUntil_nS(nS_RWnReady); 
   uint32_t GPIO_6 = ReadGPIO6; //Address bus and  R/*W are (almost) valid on Phi2 rising, Read now
   uint16_t Address = GP6_Address(GPIO_6); //parse out address
   bool R_Wn = GP6_R_Wn(GPIO_6);  //parse read/write bit
   
   if (R_Wn) SetDataBufOut; //set data buffer direction (on pcb v0.3+)
   else SetDataBufIn;

   if (fBusSnoop != NULL)
   {
      if (fBusSnoop(Address, R_Wn)) return;
   }
   
   WaitUntil_nS(nS_PLAprop); 
   uint32_t GPIO_9 = ReadGPIO9; //Now read the derived signals 
   
   if (!GP9_ROML(GPIO_9)) //ROML: 8000-9FFF address space, read only
   {
      if (LOROM_Image!=NULL) DataPortWriteWait(LOROM_Image[Address & LOROM_Mask]); 
      if (IOHandler[CurrentIOHandler]->ROMLHndlr != NULL) IOHandler[CurrentIOHandler]->ROMLHndlr(Address);
      //Printf_dbg("roml addr: %04x\n", Address); //useful for HW debug of address lines
   }  //ROML
   else if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
   {
      if (HIROM_Image!=NULL) DataPortWriteWait(HIROM_Image[Address & HIROM_Mask]); 
      if (IOHandler[CurrentIOHandler]->ROMHHndlr != NULL) IOHandler[CurrentIOHandler]->ROMHHndlr(Address);
   }  //ROMH
   else if (!GP9_IO1n(GPIO_9)) //IO1: DExx address space
   {
      Address &= 0xFF;
      #ifdef DbgIOTraceLog
         BigBuf[BigBufCount] = Address; //initialize w/ address
      #endif

      if (IOHandler[CurrentIOHandler]->IO1Hndlr != NULL) IOHandler[CurrentIOHandler]->IO1Hndlr(Address, R_Wn);

      #ifdef DbgIOTraceLog
         if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
         if (BigBufCount < BigBufSize) BigBufCount++;
      #endif
   }  //IO1
   
   else if (!GP9_IO2n(GPIO_9))  //IO2: DFxx address space
   {
      Address &= 0xFF;

      if (IOHandler[CurrentIOHandler]->IO2Hndlr != NULL) IOHandler[CurrentIOHandler]->IO2Hndlr(Address, R_Wn);
   }
   
   if (IOHandler[CurrentIOHandler]->CycleHndlr != NULL) IOHandler[CurrentIOHandler]->CycleHndlr();

   
   if (EmulateVicCycles || DMA_State > DMA_S_BeginStartStates)
   {
      while(GP6_Phi2(ReadGPIO6)); //Re-align to phi2 falling, if needed
      //phi2 has gone low..........................................................................
      
      StartCycCnt = ARM_DWT_CYCCNT;
      
      // activate/disable DMA during low phase of Phi2
      if (DMA_State > DMA_S_BeginStartStates)
      {
         switch (DMA_State)
         {
            case DMA_S_StartDisable:
               //WaitUntil_nS(200);
               SetDMADeassert;
               DMA_State = DMA_S_DisableReady;
               break;
            case DMA_S_StartActive:
               WaitUntil_nS(200); 
               SetDMAAssert;
               DMA_State = DMA_S_ActiveReady;
               return;
               //break;
            case DMA_S_Start_BA_Active:
               if (!GP9_BA(ReadGPIO9)) // bus not available, bad line
               { 
                  WaitUntil_nS(200); 
                  SetDMAAssert;
                  DMA_State = DMA_S_ActiveReady;
                  return;
               }
               //break;
         }
      }
      
      if (EmulateVicCycles)
      {
         SetDataBufOut;  //only read allowed in vic cycle, set data buf to output
         WaitUntil_nS(nS_VICStart);
         
         GPIO_6 = ReadGPIO6; //Address bus and R/*W 
         Address = GP6_Address(GPIO_6); //parse out address
         GPIO_9 = ReadGPIO9; //Now read the derived signals
         
         if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
         {
            if (HIROM_Image!=NULL) DataPortWriteWaitVIC(HIROM_Image[Address & 0x1FFF]); //uses nS_VICDHold hold time
         } 
      }
   }
   
   //leave time enough time to re-trigger on rising edge!
#ifdef DbgSignalIsrPHI2
   SetDebugDeassert;
#endif
}

