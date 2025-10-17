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

extern uint16_t LOROM_Mask;

void InitHndlr_ZaxxonSuper();                           
void ROMLHndlr_ZaxxonSuper(uint32_t Address);  

stcIOHandlers IOHndlr_ZaxxonSuper =
{
  "Zaxxon_SuperZaxxon",  //Name of handler
  &InitHndlr_ZaxxonSuper,//Called once at handler startup
  NULL,                  //IO1 R/W handler
  NULL,                  //IO2 R/W handler
  &ROMLHndlr_ZaxxonSuper,//ROML Read handler, in addition to any ROM data sent
  NULL,                  //ROMH Read handler, in addition to any ROM data sent
  NULL,                  //Polled in main routine
  NULL,                  //called at the end of EVERY c64 cycle
};

void InitHndlr_ZaxxonSuper()
{
   LOROM_Mask = 0x0fff; //mirror 4k to both sides of 8klo

   LOROM_Image = CrtChips[0].ChipROM;
   HIROM_Image = CrtChips[1].ChipROM;
}

void ROMLHndlr_ZaxxonSuper(uint32_t Address)
{
   HIROM_Image = CrtChips[(Address & 0x1000) ? 2 : 1].ChipROM;
}

