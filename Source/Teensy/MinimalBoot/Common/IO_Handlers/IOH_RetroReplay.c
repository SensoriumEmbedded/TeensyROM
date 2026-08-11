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

stcIOHandlers IOHndlr_RetroReplay =
{
  "RetroReplay",           //Name of handler, IOHNameLength max
  &InitHndlr_RetroReplay,  //Called once at handler startup
  &IO1Hndlr_RetroReplay,   //IO1 R/W handler
  NULL,                    //IO2 R/W handler not use in RR REU compatibility mode
  &ROMLHndlr_RetroReplay,  //ROML Read handler, in addition to any ROM data sent
  NULL,                    //ROMH Read handler, in addition to any ROM data sent
  NULL,                    //Polled in main routine
  CycleHndlr_RetroReplay,  //called at the end of EVERY c64 cycle
};

extern volatile uint32_t CycleCountdown;
extern uint8_t *lcl_LOROM_Image;           

#define RR_RAM_Buf  TgetQueue  //re-use this as it is freed on main menu start

// RetroReplay IOH design based on description of RetroReplay firmware at:
// https://rr.c64.org/wiki/Inside_Replay_Essentials.txt
// Also hardware description at 
// https://rr.c64.org/wiki/Inside_Replay.txt
// and a few peeks in the VICE CRT documentation at
// https://vice-emu.sourceforge.io/vice_17.html#SEC442

// Assumed that RR firmware initializes extended control register only once with a value
// of 64 (REU compatible memory map) and ignores $de01 afterward. 
// This means no freeze disable or and access to only ram bank 0 at $8000. 

// $de00 Control Register - write only
#define RR_CR_RBANK15   0b10000000   // ROM/RAM bank bit 15 (unused)
#define RR_CR_RELEASE   0b01000000   // Release freeze (reset Game and EXROM)
#define RR_CR_RAMEN     0b00100000   // RAM enable (ROML and IO2 are ROM if disabled)
#define RR_CR_RBANK14   0b00010000   // ROM/RAM bank bit 14
#define RR_CR_RBANK13   0b00001000   // ROM/RAM bank bit 13
#define RR_CR_DISABLE   0b00000100   // Disable RR cart, regs, and ROM/RAM
#define RR_CR_EXROM     0b00000010   // EXROM signal control, 1=High
#define RR_CR_nGAME     0b00000001   // GAME signal control, 1=low

// $de01 Control Register - write only
// Bits 1, 2, and 6 can only be written once
// RR firmware Uses $de00 after $de01 'write once'
// -----------------------------------------------
// Bit 7: ROM/RAM bank bit 15 (unused)
// Bit 6: REU_MAP Enable  
// Bit 5: Hardware Flash mode software is always 0
// Bit 4: ROM/RAM bank bit 14
// Bit 3: ROM/RAM bank bit 13
// Bit 2: Disables Freeze function 
// Bit 1: Allows banking of RAM in DF00/DE02 area
// Bit 0: Enable Clockport connector
                             
// $de00 & $de01 Status Register Read
// #define RR_SR_RBANK15   0b10000000 // Banking bit 15 - RR_CR_RBANK15
// #define RR_SR_REU_MAP   0b01000000 // REU compatible memory map - Always 1
// #define RR_SR_RBANK16   0b00100000 // Banking bit 16 - NOT USED Alway 0
// #define RR_SR_RBANK14   0b00010000 // Banking bit 14 - RR_CR_RBANK14 
// #define RR_SR_RBANK13   0b00001000 // Banking bit 13 - RR_CR_RBANK13  
// #define RR_SR_FREEZE    0b00000100 // Status of Freeze button (1 = pressed)
// #define RR_SR_ALWBNK    0b00000010 // AllowBank - Always 0 
// #define RR_SR_FLASH     0b00000001 // Flashmode - Always 0

uint8_t WriteOnce = 0; // With RR Firmware will have REU_MAP (64) set upon first write                                    
uint8_t StatusReg;
                           
void ProcessRRControlReg(uint8_t ControlReg)
{
  if (ControlReg & RR_CR_RELEASE) // Release freeze         
  {
     SetNMIDeassert;
     SetIRQDeassert;
  }

  //set RAM/ROM bank:
  BankNum = (((ControlReg & (RR_CR_RBANK14 | RR_CR_RBANK13)) >> 3) |
             ((ControlReg & RR_CR_RBANK15) >> 5));
  lcl_LOROM_Image = CrtChips[BankNum].ChipROM;  //default to ROM, may update below.     
  HIROM_Image = lcl_LOROM_Image;

  if (ControlReg & RR_CR_RAMEN) lcl_LOROM_Image = RR_RAM_Buf; //Set lcl_LOROM_Image to RAM
  //else disable RAM, use lcl_LOROM_Image already set 
  
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

  StatusReg = ((ControlReg & (RR_CR_RBANK15 | RR_CR_RBANK14 | RR_CR_RBANK13)) |
                              WriteOnce); 
                              // Writeonce could probably be hard-coded to 64. 
}

FLASHMEM void InitHndlr_RetroReplay()
{
   fSpecialBtnChange = &SpecialBtn_SuperSnapshotV5; //same trigger as SSv5
   
   RR_RAM_Buf = (uint8_t*)calloc(8*1024, sizeof(uint8_t)); //8k

   // fake out the Phi2 isr to not serve LOROM_Image directly as read-only
   //  use ROMLHndlr_RetroReplay: for R/W instead
   LOROM_Image = NULL; 
   
   CycleCountdown = 0;
   
   ProcessRRControlReg(0);  // Initialize Control    
}   

// $deXX Handler
// Assuming REU_Map and  AllowBank Off 
// $de02-$deff contains mirrored $9e02-$9eff of selected block 
void IO1Hndlr_RetroReplay(uint8_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image == NULL) return;  //skip if disabled
   
   if (!R_Wn) // IO1 write
   { 
   	  uint8_t Data = DataPortWaitRead(); 
   	  switch (Address)
   	  { 
   	  	 case 0x00:           // Control Register write
   	  	 	  ProcessRRControlReg(Data);  
   	  	 	  break;
   	     case 0x01:           // Extended Control Register write
   	        WriteOnce = Data; 
   	        StatusReg = Data; 
   	        break;
   	     default: 
       	  if (lcl_LOROM_Image == RR_RAM_Buf)  //only write if RAM is enabled
            lcl_LOROM_Image[0x1e00+Address] = Data;  
      }        
   }  // end write           
   else // HIGH (IO1 read)
   { 
 	   switch (Address)
 	   {	   
 	   	  // Return StatusRegister 
       case 0x00: 
       case 0x01: 
          DataPortWriteWait(StatusReg);  
          break;
       default: 
          DataPortWriteWait(lcl_LOROM_Image[0x1e00+Address]);  
    }
   } // End read
}

void ROMLHndlr_RetroReplay(uint32_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image!=NULL) 
 
   {
      if (R_Wn) // Read
      {
         DataPortWriteWait(lcl_LOROM_Image[Address & 0x1fff]); 
      }
      else  // Write -- RAM ONLY
      {
         if (lcl_LOROM_Image == RR_RAM_Buf ) //!= CrtChips[BankNum].ChipROM) //allow RAM writes only
            lcl_LOROM_Image[Address & 0x1fff] = DataPortWaitRead();
      }
   }
}

void CycleHndlr_RetroReplay(bool R_Wn)
{
   if (CycleCountdown)  
   {
      if (CycleCountdown == CycCntFreeze) // button activated
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
         ProcessRRControlReg(RR_CR_nGAME | RR_CR_EXROM);
      }
  }
}

