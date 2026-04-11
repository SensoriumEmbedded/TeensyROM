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


//IO Handler for SuperSnapshotV5 

void InitHndlr_SuperSnapshotV5();                           
void IO1Hndlr_SuperSnapshotV5(uint8_t Address, bool R_Wn);  
void ROMLHndlr_SuperSnapshotV5(uint32_t Address, bool R_Wn);

stcIOHandlers IOHndlr_SuperSnapshotV5 =
{
  "SuperSnapshotV5",          //Name of handler, IOHNameLength max
  &InitHndlr_SuperSnapshotV5, //Called once at handler startup
  &IO1Hndlr_SuperSnapshotV5,  //IO1 R/W handler
  NULL,                       //IO2 R/W handler
  &ROMLHndlr_SuperSnapshotV5, //ROML Read handler, in addition to any ROM data sent
  NULL,                       //ROMH Read handler, in addition to any ROM data sent
  NULL,                       //Polled in main routine
  NULL,                       //called at the end of EVERY c64 cycle
};

extern void (*fSpecialBtnChange)(bool Up_nDn);  //Pointer to function called when Special Button Changes
extern uint16_t LOROM_Mask;
extern uint8_t* TgetQueue;
uint8_t *lcl_LOROM_Image;
uint8_t BankNum;

#define SSv5_RAM_Buf  TgetQueue  //re-use this as it is freed on main menu start

//ref: https://vice-emu.sourceforge.io/vice_17.html#SEC466
//     https://rr.pokefinder.org/wiki/Super_Snapshot
//     https://rr.pokefinder.org/wiki/File:Super_Snapshot_v5_Schematics.gif
//     https://rr.pokefinder.org/rrwiki/images/8/80/Super_Snapshot_binaries_rr.c64.org_2021-03.rar

#define SSv5_CR_RELEASE   0b00000001   // release freeze, !GAME
#define SSv5_CR_RAMEN     0b00000010   // RAM enable, EXROM
#define SSv5_CR_RBANK0    0b00000100   // ROM/RAM bank bit 0
#define SSv5_CR_ROMEN     0b00001000   // ROM enable
#define SSv5_CR_RBANK1    0b00010000   // ROM/RAM bank bit 1
#define SSv5_CR_RBANK2    0b00100000   // ROM/RAM bank bit 2 (unused/unsupported, for 128KiB ROM)

//__________________________________________________________________________________


void ProcessControlReg(uint8_t ControlReg)
{
   //set RAM/ROM bank:
   BankNum = ((ControlReg & SSv5_CR_RBANK0) >> 2) | ((ControlReg & (SSv5_CR_RBANK2 | SSv5_CR_RBANK1)) >> 3);
   lcl_LOROM_Image = CrtChips[BankNum].ChipROM;  //default to ROM, may update below.
   HIROM_Image = lcl_LOROM_Image + 0x2000; //ROM only, 8k above, 16k total per chip

   //RAM enable, EXROM   !ram enable (0: enabled, 1: disabled), !EXROM (0: high, 1: low)
   if (ControlReg & SSv5_CR_RAMEN) 
   {
      //disable RAM, use lcl_LOROM_Image already set
      SetExROMAssert;  //rtBin16k or 8kLo
   }
   else 
   {
      //Set lcl_LOROM_Image to enable RAM, Set RAM bank
      lcl_LOROM_Image = SSv5_RAM_Buf + (BankNum & 3) * 0x2000;
      SetExROMDeassert;  //rtBin8kHi or None
   }
   
   //release freeze, !GAME    GAME (0: low, 1: high)
   if (ControlReg & SSv5_CR_RELEASE) 
   {
      SetGameDeassert;  //8kLo or Off
      SetNMIDeassert;
      SetIRQDeassert;
   }
   else SetGameAssert; //rtBin8kHi or rtBin16k

   //ROM enable   !rom enable (0: enabled, 1: disabled), Disables the cart
   if (ControlReg & SSv5_CR_ROMEN) 
   {
      lcl_LOROM_Image = HIROM_Image = NULL;
      SetLEDOff;
   }   
}

void SpecialBtn_SuperSnapshotV5(bool Up_nDn)
{
   Serial.printf("SpecialBtn_SuperSnapshotV5: %d\n", Up_nDn);

//AI bullshit???

/*
NMI Assertion

Immediate Banking Change: The firmware instantly updates the virtual $DF01 register. At the signal level, it sets GAME = 0 and EXROM = 0 (Ultimax Mode).
ROM Mapping: The Teensy maps the SSv5 ROM bank (usually the first 8KB or 16KB of the .CRT file) to the $E000-$FFFF range.
CPU Vector Hijack: When the C64 CPU acknowledges the 
, it looks at $FFFA/$FFFB. Because the Teensy is now mapping the SSv5 ROM into that space, the CPU jumps to the cartridge’s freezer entry point instead of the C64's internal routine. 
*/

//releasing the button allows the line to return to a logic HIGH
   if(Up_nDn) 
   {
      SetIRQAssert;
      SetNMIAssert;
      
      //need to wait for the 3 write for this:
      ProcessControlReg(0);
      SetLEDOn;
      //SetGameAssert;
      //SetExROMDeassert;
      //lcl_LOROM_Image = SSv5_RAM_Buf; //RAM Bank 0
      //HIROM_Image = CrtChips[0].ChipROM+0x2000;  //8k above, 16k total per chip
   }
}

//__________________________________________________________________________________

void InitHndlr_SuperSnapshotV5()
{
   fSpecialBtnChange = &SpecialBtn_SuperSnapshotV5;
   
   SSv5_RAM_Buf = (uint8_t*)malloc(32*1024); //32k RAM
   //memset(SSv5_RAM_Buf, 0xaa, 32*1024); //no need
   //if(SSv5_RAM_Buf == NULL)
   //{
   //   Serial.println("SS5 OOM");
   //   REBOOT;
   //}

   // fake out the Phi2 isr to not serve LOROM_Image directly as read-only
   //  use ROMLHndlr_SuperSnapshotV5 for R/W instead
   LOROM_Image = NULL; 
   
   ProcessControlReg(0);
   ////force rtBin8kHi:
   //SetGameAssert;
   //SetExROMDeassert;
   //lcl_LOROM_Image = SSv5_RAM_Buf; //RAM Bank 0
   //HIROM_Image = CrtChips[0].ChipROM+0x2000;  //8k above, 16k total per chip
   
   Printf_dbg("SSv5, 8kHi mode forced");
}   

void IO1Hndlr_SuperSnapshotV5(uint8_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image == NULL) return;  //skip if disabled
   
   if (R_Wn) //High (IO1 Read)
   {  //The last page of the currently selected lower ROM bank is mirrored in the I/O-1 range when reading
      //DataPortWriteWaitLog(lcl_LOROM_Image[Address]); //may be pointing at RAM, use BankNum set previously
      DataPortWriteWaitLog(CrtChips[BankNum].ChipROM[0x1e00+Address]); 
   }
   else  // IO1 write
   {  //The control Register is the full I/O-1 range when writing
      uint8_t ControlReg = DataPortWaitRead(); 
      TraceLogAddValidData(ControlReg);
      
      ProcessControlReg(ControlReg);      
  } //write
}

void ROMLHndlr_SuperSnapshotV5(uint32_t Address, bool R_Wn)
{
   if (lcl_LOROM_Image!=NULL) 
   {
      if (R_Wn) 
      {
         DataPortWriteWait(lcl_LOROM_Image[Address & 0x1fff]); 
      }
      else
      {
         //if (!(ControlReg & SSv5_CR_RAMEN)) //we have to be pointing at "RAM" for ROML to be active
         //SetDebugAssert;
         lcl_LOROM_Image[Address & 0x1fff] = DataPortWaitRead();
         //SetDebugDeassert;
      }
   }
}
