// MIT License
// 
// Copyright (c) 2026 Paul Harker
//  Used by permission
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

//IO Handler for RetroReplay 

void InitHndlr_RetroReplay();                           
void IO1Hndlr_RetroReplay(uint8_t Address, bool R_Wn);  
void ROMLHndlr_RetroReplay(uint32_t Address, bool R_Wn);
void CycleHndlr_RetroReplay(bool R_Wn);
void NoFreeze(bool dummyval);  // do-nothing function to call when freeze is disabled

stcIOHandlers IOHndlr_RetroReplay =
{
  "RetroReplay",           //Name of handler, IOHNameLength max
  &InitHndlr_RetroReplay,  //Called once at handler startup
  &IO1Hndlr_RetroReplay,   //IO1 R/W handler
  NULL,                    //IO2 R/W handler not used to maintain REU compatibility 
  &ROMLHndlr_RetroReplay,  //ROML Read handler, in addition to any ROM data sent
  NULL,                    //ROMH Read handler, in addition to any ROM data sent
  NULL,                    //Polled in main routine
  CycleHndlr_RetroReplay,  //called at the end of EVERY c64 cycle
};

extern volatile uint32_t CycleCountdown;
extern uint8_t *lcl_LOROM_Image;           

#define RR_RAM_Buf  TgetQueue  //re-use this as it is freed on main menu start

// IOH_RetroReplay.c based on Cyberpunx firmware description:
// https://rr.c64.org/wiki/Inside_Replay_Essentials.txt
// And RetroReplay hardware description:
// https://rr.c64.org/wiki/Inside_Replay.txt
// ... and a few peeks into the VICE CRT documentation:
// https://vice-emu.sourceforge.io/vice_17.html#SEC442

// NOTE: Known deviations from hardware definition.
//       REU memory map only: IO2 is not supported.
//       No bank changes via $de01 ECR (see Replay Essentials)
//       No Clockport support
         

// $de00 Control Register - Write only
#define RR_CR_RBANK15   0b10000000   // Banking bit 15
#define RR_CR_RELEASE   0b01000000   // Release freeze (reset Game and EXROM)
#define RR_CR_RAMEN     0b00100000   // RAM enable 
#define RR_CR_RBANK14   0b00010000   // Banking bit 14
#define RR_CR_RBANK13   0b00001000   // Banking bit 13
#define RR_CR_DISABLE   0b00000100   // Disable RR cart, regs, and ROM/RAM
#define RR_CR_EXROM     0b00000010   // EXROM signal control, 1=High
#define RR_CR_nGAME     0b00000001   // GAME signal control, 1=low

// $de01 Extended Control Register - Write only
// RR38: Bits 1, 2, and 6 are written once then uses $de00
#define RR_ECR_RBANK15   0b10000000  // Banking bit 15
#define RR_ECR_REU_MAP   0b01000000  // Memory Map: 0 = Std  1 = REU compatible  
#define RR_ECR_RBANK16   0b00100000  // Hardware EEPROM bank: Always 0
#define RR_ECR_RBANK14   0b00010000  // Banking bit 14
#define RR_ECR_RBANK13   0b00001000  // Banking bit 13
#define RR_ECR_NOFREEZ   0b00000100  // Disables Freeze function 
#define RR_ECR_ALWBNK    0b00000010  // Allows banking of RAM 
#define RR_ECR_CLOCKEN   0b00000001  // Enable Clockport connector: Always 0
                                     
// $de00 & $de01 Status Register - read only
// Status of bits set by writes to CR and ECR
#define RR_SR_RBANK15   0b10000000  // Banking bit 15 
#define RR_SR_REU_MAP   0b01000000  // REU compatible memory map - RR38 = 1
#define RR_SR_RBANK16   0b00100000  // EEPROM Banking bit: Always 0
#define RR_SR_RBANK14   0b00010000  // Banking bit 14 
#define RR_SR_RBANK13   0b00001000  // Banking bit 13 
#define RR_SR_FREEZE    0b00000100  // Freeze button status: 1 = pressed
#define RR_SR_ALWBNK    0b00000010  // AllowBank active: RR38 & AR = 0
#define RR_SR_FLASH     0b00000001  // EEPROM flash mode active: Always 0   
                             
//bool RR_RAM_Enabled;
uint8_t RR_StatusReg = 0;
                           
void ProcessRRControlReg(uint8_t ControlReg)
{
  if (ControlReg & RR_CR_RELEASE) // Release freeze         
  {
     SetNMIDeassert;
     SetIRQDeassert;
     RR_StatusReg &= ~RR_SR_FREEZE;  //clear freeze bit
  }

  //set ROM bank:
  BankNum = (((ControlReg & (RR_CR_RBANK14 | RR_CR_RBANK13)) >> 3) |
             ((ControlReg & RR_CR_RBANK15) >> 5));
  lcl_LOROM_Image = CrtChips[BankNum].ChipROM;  //default to ROM, may update below.     
  HIROM_Image = lcl_LOROM_Image;

  if (ControlReg & RR_CR_RAMEN) 
   	lcl_LOROM_Image = ( RR_RAM_Buf + ((BankNum & 3) * 0x2000) * sizeof(uint8_t) );  
 	 
  if (ControlReg & RR_CR_EXROM) SetExROMDeassert;  //rtBin8kHi or None
  else SetExROMAssert;  //rtBin16k or 8kLo  
  
  if (ControlReg & RR_CR_nGAME) SetGameAssert; //rtBin8kHi or rtBin16k
  else SetGameDeassert;  //8kLo or None
        
  if (ControlReg & RR_CR_DISABLE) //disable cart
  {
     lcl_LOROM_Image = NULL;   
     HIROM_Image = NULL;
     SetLEDOff;
  } 

  RR_StatusReg = ((RR_StatusReg & (RR_SR_ALWBNK | RR_SR_REU_MAP)) | // Maintain RR38 1x bits
                 (ControlReg & (RR_CR_RBANK15 | RR_CR_RBANK14 | RR_CR_RBANK13))); // set banking bits
}

FLASHMEM void InitHndlr_RetroReplay()
{
  fSpecialBtnChange = &SpecialBtn_SuperSnapshotV5; //same trigger as SSv5
   
  RR_RAM_Buf = (uint8_t*)calloc(32*1024, sizeof(uint8_t)); //32k

  // fake out the Phi2 isr to not serve LOROM_Image directly as read-only
  //  use ROMLHndlr_RetroReplay: for R/W instead
  LOROM_Image = NULL; 
   
  CycleCountdown = 0;
   
  ProcessRRControlReg(0);  // Initialize Control    
}   

// $deXX Handler -- REU Memory Map
// $de02-$deff contains mirrored $9e02-$9eff of selected bank 
void IO1Hndlr_RetroReplay(uint8_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image != NULL) 
   {	
   	// if HIROM and LOROM not eqaul then lcl_LOROM_Image = RAM 
      if ((HIROM_Image != lcl_LOROM_Image) & !(RR_StatusReg & RR_SR_ALWBNK)) 
      	lcl_LOROM_Image = RR_RAM_Buf;  // Use bank 0 if banking not enabled
      	
      if (!R_Wn) // IO1 write
      { 
         uint8_t Data = DataPortWaitRead(); 
         switch (Address)
         { 
            case 0x00:           // Control Register write
               ProcessRRControlReg(Data);  
               break;
            case 0x01:           // Extended Control Register write                
               // ECR first write should always have RR_ECR_REU_MAP set. So we can avoid an 
               // additional global by using RR_StatusReg to flag *and* store first write. 
               if (RR_StatusReg == 0)  
               { 	 
                  if (Data & RR_ECR_NOFREEZ) fSpecialBtnChange = &NoFreeze; // Disable Freeze
                  RR_StatusReg = (Data & ~RR_ECR_NOFREEZ);  //RR_StatusReg does not have NOFREEZ bit
               }              
               // else not first write. Ignore additional writes.
               break;
            default: 
               if (HIROM_Image != lcl_LOROM_Image)  // if not ROM, then RAM
                  lcl_LOROM_Image[Address+0x1e00] = Data;  // write  
         }        
      }             
      else // IO1 read
      { 
         switch (Address)
         {    
            case 0x00: //$de00 & $de01 reads return status register
            case 0x01: 
               DataPortWriteWait(RR_StatusReg);  
            break;
            default: 
               DataPortWriteWait(lcl_LOROM_Image[Address+0x1e00]);  
         }
      }
   }
}

void NoFreeze(bool dummyval) {}  // do-nothing function to call when freeze is disabled

void ROMLHndlr_RetroReplay(uint32_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image != NULL)           
   {   
      if (R_Wn) // Read
      {
         DataPortWriteWait(lcl_LOROM_Image[Address & 0x1fff]); 
      }
      else  // Write -- RAM ONLY
      {  
      	// if not equal then lcl_LOROM_Image = RAM 
         if (HIROM_Image != lcl_LOROM_Image)       
            lcl_LOROM_Image[Address & 0x1fff] = DataPortWaitRead(); 
      }
   }
}

// TR+ uses Gideon's asynch freeze technique
// https://codebase64.com/lib/exe/fetch.php?media=base:safely_freezing_the_c64.pdf
void CycleHndlr_RetroReplay(bool R_Wn)
{
   if (CycleCountdown)  
   {
 
      if (CycleCountdown == CycCntFreeze) // button activated
      {  
      	 RR_StatusReg &= RR_SR_FREEZE;  //set freeze bit
      	 
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
         ProcessRRControlReg(RR_CR_nGAME | RR_CR_EXROM);
      }
  }
}

