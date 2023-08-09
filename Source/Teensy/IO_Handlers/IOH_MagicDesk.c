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

void InitHndlr_MagicDesk();                           
void IO1Hndlr_MagicDesk(uint8_t Address, bool R_Wn);  

stcIOHandlers IOHndlr_MagicDesk =
{
  "Magic Desk",          //Name of handler
  &InitHndlr_MagicDesk,   //Called once at handler startup
  &IO1Hndlr_MagicDesk,    //IO1 R/W handler
  NULL,                  //IO2 R/W handler
  NULL,                   //ROML Read handler, in addition to any ROM data sent
  NULL,                      //ROMH Read handler, in addition to any ROM data sent
  NULL,                      //Polled in main routine
  NULL,                  //called at the end of EVERY c64 cycle
};

extern StructCrtChip CrtChips[];
extern uint8_t NumCrtChips;

uint8_t MDBankReg; 

void InitHndlr_MagicDesk()
{
   MDBankReg = 0;
}

void IO1Hndlr_MagicDesk(uint8_t Address, bool R_Wn)
{
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      DataPortWriteWaitLog(MDBankReg);
   }
   else  // IO1 write    -------------------------------------------------
   {
      MDBankReg = DataPortWaitRead(); 
      if (MDBankReg & 0x80) SetExROMDeassert; //turn off cart
      else SetExROMAssert;
      
      TraceLogAddValidData(MDBankReg);
      MDBankReg &= 0x7f;
      if (MDBankReg<NumCrtChips) LOROM_Image = CrtChips[MDBankReg].ChipROM;
   }
}

