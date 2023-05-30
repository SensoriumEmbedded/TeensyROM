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

void InitHndlr_EpyxFastLoad();                           
void IO1Hndlr_EpyxFastLoad(uint8_t Address, bool R_Wn);  
void IO2Hndlr_EpyxFastLoad(uint8_t Address, bool R_Wn);  
void ROMLHndlr_EpyxFastLoad(uint32_t Address);           
void CycleHndlr_EpyxFastLoad();                          

stcIOHandlers IOHndlr_EpyxFastLoad =
{
  "Epyx Fast Load",          //Name of handler
  &InitHndlr_EpyxFastLoad,   //Called once at handler startup
  &IO1Hndlr_EpyxFastLoad,    //IO1 R/W handler
  &IO2Hndlr_EpyxFastLoad,    //IO2 R/W handler
  &ROMLHndlr_EpyxFastLoad,   //ROML Read handler, in addition to any ROM data sent
  NULL,                      //ROMH Read handler, in addition to any ROM data sent
  NULL,                      //Polled in main routine
  &CycleHndlr_EpyxFastLoad,  //called at the end of EVERY c64 cycle
};


#define EpyxMaxCycleCount  512 //Numer for C64 clock cycles to disable Epyx
#define EpyxFastLoadCycleReset {SetExROMAssert;if(CycleCountdown<EpyxMaxCycleCount)CycleCountdown=EpyxMaxCycleCount;}  //don't interfere with long count set at init

extern const unsigned char *LOROM_Image;

uint32_t CycleCountdown=0;

void InitHndlr_EpyxFastLoad()
{
   CycleCountdown=100000; //give extra time at start
   SetExROMAssert;
}

void IO1Hndlr_EpyxFastLoad(uint8_t Address, bool R_Wn)
{
   EpyxFastLoadCycleReset;
}

void IO2Hndlr_EpyxFastLoad(uint8_t Address, bool R_Wn)
{
   if (R_Wn) DataPortWriteWait(LOROM_Image[Address | 0x1F00]); 
}

void ROMLHndlr_EpyxFastLoad(uint32_t Address)
{
   EpyxFastLoadCycleReset
}

void CycleHndlr_EpyxFastLoad()
{
   if (CycleCountdown)
   {
      if(--CycleCountdown == 0) SetExROMDeassert;
   }
}
