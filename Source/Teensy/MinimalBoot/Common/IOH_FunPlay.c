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

void IO1Hndlr_FunPlay(uint8_t Address, bool R_Wn);  

stcIOHandlers IOHndlr_FunPlay =
{
  "FunPlay",          //Name of handler
  NULL,                //Called once at handler startup
  &IO1Hndlr_FunPlay,    //IO1 R/W handler
  NULL,                  //IO2 R/W handler
  NULL,                   //ROML Read handler, in addition to any ROM data sent
  NULL,                      //ROMH Read handler, in addition to any ROM data sent
  NULL,                      //Polled in main routine
  NULL,                  //called at the end of EVERY c64 cycle
};


void IO1Hndlr_FunPlay(uint8_t Address, bool R_Wn)
{
   if (!R_Wn) // IO1 write    -------------------------------------------------
   {     
      uint8_t Data = DataPortWaitRead(); 
      uint8_t BankReg = ((Data >> 3) & 7) | ((Data & 1) << 3); //remap to bank #
      
      if ((Data & 0xc6) == 0x86) SetExROMDeassert; //turn off cart
          
      TraceLogAddValidData(Data);
      if (BankReg<NumCrtChips) LOROM_Image = CrtChips[BankReg].ChipROM;
   }
}

