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

void InitHndlr_SuperGames();                           
void IO2Hndlr_SuperGames(uint8_t Address, bool R_Wn);  

stcIOHandlers IOHndlr_SuperGames =
{
  "SuperGames",          //Name of handler
  &InitHndlr_SuperGames, //Called once at handler startup
  NULL,                  //IO1 R/W handler
  &IO2Hndlr_SuperGames,  //IO2 R/W handler
  NULL,                  //ROML Read handler, in addition to any ROM data sent
  NULL,                  //ROMH Read handler, in addition to any ROM data sent
  NULL,                  //Polled in main routine
  NULL,                  //called at the end of EVERY c64 cycle
};

bool WPLatched; 

void InitHndlr_SuperGames()
{
   WPLatched = false;
}

void IO2Hndlr_SuperGames(uint8_t Address, bool R_Wn)
{
   if (!R_Wn) // IO2 write  -------------------------------------------------
   {     
      uint8_t Data = DataPortWaitRead(); 
      if (!WPLatched) 
      {
         uint8_t BankReg = Data & 3;
         if (BankReg<NumCrtChips)
         {
            LOROM_Image = CrtChips[BankReg].ChipROM;
            HIROM_Image = CrtChips[BankReg].ChipROM+0x2000;
         }         
         
         if (Data & 4)
         {
            SetExROMDeassert; //turn off cart
            SetGameDeassert;
         }
         else
         {
            SetExROMAssert; //16k config
            SetGameAssert;
         }
         
         WPLatched = (Data & 8) == 8;            
      }
      TraceLogAddValidData(Data);
   }
}

