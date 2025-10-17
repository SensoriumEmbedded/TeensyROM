// MIT License
// 
// Copyright (c) 2025 Travis Smith
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

// https://github.com/crystalct/MagicDesk2

#ifdef MinimumBuild 
   extern stcSwapBuffers SwapBuffers[Num8kSwapBuffers]; 
   extern uint8_t* ImageCheckAssign(uint8_t* BankRequested);
   extern bool PathIsRoot();
   extern char DriveDirPath[];
   extern StructMenuItem DriveDirMenu;
   extern File myFile;

   extern void LoadBank(uint32_t SeekTo, uint8_t* ptrImage);
#endif

extern volatile uint8_t EmulateVicCycles;

void InitHndlr_MagicDesk2();  
void IO1Hndlr_MagicDesk2(uint8_t Address, bool R_Wn);  
void PollingHndlr_MagicDesk2();                           

stcIOHandlers IOHndlr_MagicDesk2 =
{
  "MagicDesk2",                //Name of handler
  &InitHndlr_MagicDesk2,       //Called once at handler startup
  &IO1Hndlr_MagicDesk2,        //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_MagicDesk2,    //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};


void InitHndlr_MagicDesk2()
{   
#ifdef MinimumBuild
   //initialize/invalidate swap buffer, pre-load first swappable chips
   uint8_t ChipNum = 0;
   for(uint8_t BuffNum = 0; BuffNum < Num8kSwapBuffers; BuffNum++) 
   {
      SwapBuffers[BuffNum].Offset=0; //default to zero/invalid
      //find next chip marked as swappable
      while (ChipNum < NumCrtChips)
      {
         if(((uint32_t)CrtChips[ChipNum].ChipROM & SwapSeekAddrMask) == SwapSeekAddrMask)
         {
            //pre-load and update
            SwapBuffers[BuffNum].Offset = (uint32_t)CrtChips[ChipNum].ChipROM;   
            LoadBank(SwapBuffers[BuffNum].Offset & ~SwapSeekAddrMask, SwapBuffers[BuffNum].Image);
            Printf_Swaps("Pre %02d: %08x & ", BuffNum, (uint32_t) SwapBuffers[BuffNum].Offset & ~SwapSeekAddrMask);

            BuffNum++;
            SwapBuffers[BuffNum].Offset = (uint32_t)CrtChips[ChipNum].ChipROM+0x2000;   
            LoadBank(SwapBuffers[BuffNum].Offset & ~SwapSeekAddrMask, SwapBuffers[BuffNum].Image);
            Printf_Swaps("%02d: %08x\n", BuffNum, (uint32_t) SwapBuffers[BuffNum].Offset & ~SwapSeekAddrMask);

            ChipNum++;            
            break;  //exit while
         }
         ChipNum++;
      }
   }
#endif
   
   //start with Bank 0:
   LOROM_Image = CrtChips[0].ChipROM;
   HIROM_Image = CrtChips[0].ChipROM+0x2000;

   //force 16k mode:
   SetGameAssert;
   SetExROMAssert;
   //but turn off VIC emmulation:
   EmulateVicCycles = false;
}

void IO1Hndlr_MagicDesk2(uint8_t Address, bool R_Wn)
{
   if (!R_Wn) //IO1 Write  -------------------------------------------------
   {
      uint8_t Data = DataPortWaitRead(); 
      TraceLogAddValidData(Data);
      if (Address == 0)  //bank select
      {
         //Printf_dbg("B:%d", Data);
         Data &= 0x7f;
            
#ifdef MinimumBuild
            //check if swapped bank is being selected, check for same or initiate swap
         LOROM_Image = ImageCheckAssign(CrtChips[Data].ChipROM);
         HIROM_Image = ImageCheckAssign(CrtChips[Data].ChipROM+0x2000);
#else
         LOROM_Image = CrtChips[Data].ChipROM;
         HIROM_Image = CrtChips[Data].ChipROM+0x2000;
#endif
      }
   }
}

void PollingHndlr_MagicDesk2()
{
#ifdef MinimumBuild   
   if (DMA_State == DMA_S_ActiveReady)
   {
      //DMA asserted, paused for bank swap from SD
      uint32_t Startms = millis();
      static uint8_t NextSwapBuffNum = 0; 
      
      //if (((uint32_t)LOROM_Image & SwapSeekAddrMask) == SwapSeekAddrMask)
      //{   //16k banks, swap both halves:
      LoadBank((uint32_t)LOROM_Image & ~SwapSeekAddrMask, SwapBuffers[NextSwapBuffNum].Image);
      Printf_Swaps("L%02d: %08x  ", NextSwapBuffNum, (uint32_t)LOROM_Image); // & ~SwapSeekAddrMask);
      SwapBuffers[NextSwapBuffNum].Offset = (uint32_t)LOROM_Image;
      LOROM_Image = SwapBuffers[NextSwapBuffNum].Image;
      
      NextSwapBuffNum++;
      LoadBank((uint32_t)HIROM_Image & ~SwapSeekAddrMask, SwapBuffers[NextSwapBuffNum].Image);
      Printf_Swaps("H%02d: %08x  ", NextSwapBuffNum, (uint32_t)HIROM_Image); // & ~SwapSeekAddrMask);
      SwapBuffers[NextSwapBuffNum].Offset = (uint32_t)HIROM_Image;
      HIROM_Image = SwapBuffers[NextSwapBuffNum].Image;
      
      if(++NextSwapBuffNum==Num8kSwapBuffers) NextSwapBuffNum=0; //assumes even number of swap banks
      //}         
            
      Printf_Swaps(" %lu mS swp\n", millis()-Startms);
      Serial.flush();
      DMA_State = DMA_S_StartDisable;
   }
#endif
}