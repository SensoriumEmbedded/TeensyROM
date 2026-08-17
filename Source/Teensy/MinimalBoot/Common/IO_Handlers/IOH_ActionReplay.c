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


//IO Handler for ActionReplay 

void InitHndlr_ActionReplay();                           
void IO1Hndlr_ActionReplay(uint8_t Address, bool R_Wn);  
void IO2Hndlr_ActionReplay(uint8_t Address, bool R_Wn);  
void ROMLHndlr_ActionReplay(uint32_t Address, bool R_Wn);
void CycleHndlr_ActionReplay(bool R_Wn);

stcIOHandlers IOHndlr_ActionReplay =
{
  "ActionReplay",          //Name of handler, IOHNameLength max
  &InitHndlr_ActionReplay, //Called once at handler startup
  &IO1Hndlr_ActionReplay,  //IO1 R/W handler
  &IO2Hndlr_ActionReplay,  //IO2 R/W handler
  &ROMLHndlr_ActionReplay, //ROML Read handler, in addition to any ROM data sent
  NULL,                    //ROMH Read handler, in addition to any ROM data sent
  NULL,                    //Polled in main routine
  CycleHndlr_ActionReplay, //called at the end of EVERY c64 cycle
};

extern volatile uint32_t CycleCountdown;
extern uint8_t *lcl_LOROM_Image;

#define AR_RAM_Buf  TgetQueue  //re-use this as it is freed on main menu start

#define AR_CR_RBANK15   0b10000000   // ROM/RAM bank bit 15 (unused)
#define AR_CR_RELEASE   0b01000000   // Release freeze
#define AR_CR_RAMEN     0b00100000   // RAM enable (ROML and IO2 are ROM if disabled)
#define AR_CR_RBANK14   0b00010000   // ROM/RAM bank bit 14
#define AR_CR_RBANK13   0b00001000   // ROM/RAM bank bit 13
#define AR_CR_DISABLE   0b00000100   // Disable AR cart, regs, and ROM/RAM
#define AR_CR_EXROM     0b00000010   // EXROM signal control, 1=High
#define AR_CR_nGAME     0b00000001   // GAME signal control, 1=low

//__________________________________________________________________________________


void ProcessARControlReg(uint8_t ControlReg)
{
   if (ControlReg & AR_CR_RELEASE) // Release freeze
   {
      SetNMIDeassert;
      SetIRQDeassert;
   }

   //set RAM/ROM bank:
   BankNum = ((ControlReg & (AR_CR_RBANK14 | AR_CR_RBANK13)) >> 3);
   lcl_LOROM_Image = CrtChips[BankNum].ChipROM;  //default to ROM, may update below.
   HIROM_Image = lcl_LOROM_Image; //ROM only, same as lower ROM image

   if (ControlReg & AR_CR_RAMEN) lcl_LOROM_Image = AR_RAM_Buf; //Set lcl_LOROM_Image to RAM
   //else disable RAM, use lcl_LOROM_Image already set

   if (ControlReg & AR_CR_EXROM) SetExROMDeassert;  //rtBin8kHi or None
   else SetExROMAssert;  //rtBin16k or 8kLo
   
   if (ControlReg & AR_CR_nGAME) SetGameAssert; //rtBin8kHi or rtBin16k
   else SetGameDeassert;  //8kLo or None

   if (ControlReg & AR_CR_DISABLE) //disable cart
   {
      lcl_LOROM_Image = NULL;
      HIROM_Image = NULL;
      SetLEDOff;
   }   
}

//__________________________________________________________________________________

FLASHMEM void InitHndlr_ActionReplay()
{
   fSpecialBtnChange = &SpecialBtn_SuperSnapshotV5; //same trigger as SSv5
   
   AR_RAM_Buf = (uint8_t*)calloc(8*1024, sizeof(uint8_t)); //8k RAM
   //if(AR_RAM_Buf == NULL)
   //{
   //   Serial.println("AR OOM");
   //   REBOOT;
   //}

   // fake out the Phi2 isr to not serve LOROM_Image directly as read-only
   //  use ROMLHndlr_ActionReplay for R/W instead
   LOROM_Image = NULL; 
   
   CycleCountdown = 0;
   
   ProcessARControlReg(0);
 
   Printf_dbg("AR init done\n");
}   

void IO1Hndlr_ActionReplay(uint8_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image == NULL) return;  //skip if disabled
   
   if (!R_Wn) // IO1 write
   {  //The control Register is the full I/O-1 range when writing
      uint8_t ControlReg = DataPortWaitRead(); 
      ProcessARControlReg(ControlReg);      
      //TraceLogAddValidData(ControlReg);
   } //write
}

void IO2Hndlr_ActionReplay(uint8_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image == NULL) return;  //skip if disabled

   if (R_Wn) //High (IO2 Read)
   {  //The last page of the currently selected RAM or ROM bank is mirrored in the I/O2 range
      DataPortWriteWaitLog(lcl_LOROM_Image[0x1f00+Address]); //Read RAM or ROM
   }
   else  // IO2 write
   {  
      if (lcl_LOROM_Image == AR_RAM_Buf) //!= CrtChips[BankNum].ChipROM) //allow RAM writes only
         lcl_LOROM_Image[0x1f00+Address] = DataPortWaitRead();
      //else Printf_dbg("Attempted write to ROM via IO2\n");
   }
}

void ROMLHndlr_ActionReplay(uint32_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image!=NULL) 
   {
      if (R_Wn) // Read
      {
         DataPortWriteWait(lcl_LOROM_Image[Address & 0x1fff]); 
      }
      else
      {
         if (lcl_LOROM_Image == AR_RAM_Buf ) //!= CrtChips[BankNum].ChipROM) //allow RAM writes only
            lcl_LOROM_Image[Address & 0x1fff] = DataPortWaitRead();
      }
   }
}

void CycleHndlr_ActionReplay(bool R_Wn)
{
   if (CycleCountdown)
   {
      if (CycleCountdown == CycCntFreeze)
      {
         if (R_Wn) 
         {
            //assert IRQ/NMI during read cycle
            SetNMIAssert;
            SetIRQAssert;
            SetLEDOn;
            //CycleCountdown = CycCntNumWr;  //will happen below
         }
         else return; //preserve CycCntFreeze state, wait for read
      }
      
      if (R_Wn) CycleCountdown = CycCntNumWr; //require 3 writes *in a row*
      else if(--CycleCountdown == 0) 
      {
         ProcessARControlReg(AR_CR_nGAME | AR_CR_EXROM);
      }
   }
}

