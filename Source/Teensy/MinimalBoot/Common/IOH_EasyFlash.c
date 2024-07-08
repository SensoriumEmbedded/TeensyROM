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

#define NumDecodeBanks   64
uint8_t *BankDecode[NumDecodeBanks][2];
uint8_t EZFlashRAM[256];

extern volatile uint8_t EmulateVicCycles;

void InitHndlr_EasyFlash();  
void IO1Hndlr_EasyFlash(uint8_t Address, bool R_Wn);  
void IO2Hndlr_EasyFlash(uint8_t Address, bool R_Wn);  

stcIOHandlers IOHndlr_EasyFlash =
{
  "EasyFlash",          //Name of handler
  &InitHndlr_EasyFlash, //Called once at handler startup
  &IO1Hndlr_EasyFlash,  //IO1 R/W handler
  &IO2Hndlr_EasyFlash,  //IO2 R/W handler
  NULL,                 //ROML Read handler, in addition to any ROM data sent
  NULL,                 //ROMH Read handler, in addition to any ROM data sent
  NULL,                 //Polled in main routine
  NULL,                 //called at the end of EVERY c64 cycle
};

void InitHndlr_EasyFlash()
{
   //initialize bank decode pointers
   for(uint8_t BankNum = 0; BankNum < NumDecodeBanks; BankNum++) {BankDecode[BankNum][0]=NULL;BankDecode[BankNum][1]=NULL;}
   for(uint8_t ChipNum = 0; ChipNum < NumCrtChips; ChipNum++)
   {
      if (CrtChips[ChipNum].BankNum < NumDecodeBanks)
         BankDecode[CrtChips[ChipNum].BankNum][CrtChips[ChipNum].LoadAddress == 0x8000 ? 0 : 1] = CrtChips[ChipNum].ChipROM;
      else Serial.printf("Unexp Bank# (%d) in chip %d\n", CrtChips[ChipNum].BankNum, ChipNum);
   }
   
   memset(EZFlashRAM, 0, 256);
   
   //start with Bank 0:
   LOROM_Image = BankDecode[0][0];
   HIROM_Image = BankDecode[0][1];
   //Printf_dbg("Jmp vect: $%02x%02x\n", HIROM_Image[0x1FFD], HIROM_Image[0x1FFC]);
   if(memcmp(HIROM_Image+0x1800, "eapi", 4)==0) Serial.printf("EAPI found\n");
   else Printf_dbg("EAPI *not* found: %d %d %d %d\n", HIROM_Image[0x1800], HIROM_Image[0x1801], HIROM_Image[0x1802], HIROM_Image[0x1803]);

   //force ultimax mode:
   SetGameAssert;
   SetExROMDeassert;
   //but turn off VIC emmulation:
   EmulateVicCycles = false;
}

void IO1Hndlr_EasyFlash(uint8_t Address, bool R_Wn)
{
   if (!R_Wn) //IO1 Write  -------------------------------------------------
   {
      uint8_t Data = DataPortWaitRead(); 
      TraceLogAddValidData(Data);
      switch (Address)
      {
         case 0x00:   // Register $DE00 – EasyFlash Bank (write-only)
            Data &= 0x3f;
            LOROM_Image = BankDecode[Data][0];
            HIROM_Image = BankDecode[Data][1];
            break;
         case 0x02:   // Register $DE02 – EasyFlash Control (write-only)
            if (Data & 0x80) SetLEDOn; //Status LED, 1 = on
            else SetLEDOff;

            if (Data & 0x02) SetExROMAssert; //EXROM state, 0 = /EXROM high
            else SetExROMDeassert;

            if (Data & 0x01) SetGameAssert; //GAME state if M (bit 2 sel bit 0 or jumper for GAME) = 1, 0 = /GAME high
            else SetGameDeassert;
            break;
      }
   }
}

void IO2Hndlr_EasyFlash(uint8_t Address, bool R_Wn)
{
   if (R_Wn) //IO2 Read  -------------------------------------------------
   {
      DataPortWriteWaitLog(EZFlashRAM[Address]);
   }
   else  // IO2 write    -------------------------------------------------
   {
      uint8_t Data = DataPortWaitRead(); 
      EZFlashRAM[Address] = Data;
      TraceLogAddValidData(Data);
   }
}
