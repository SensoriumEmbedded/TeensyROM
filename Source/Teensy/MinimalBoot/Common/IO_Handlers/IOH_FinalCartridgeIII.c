// MIT License
// 
// Copyright (c) 2026 Paul Harker
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

// IO Handler for Final Cartridge III series

//  Supports 
//    Final Cartridge III
//    Final Cartridge III 101% - with TR+ REU or External REU
//    Final Cartridge III+     - 250k ROM

// NOTE:
// FCIII handler has three possible "modes"
// - FCIII or FCIII+: All of IO2 belongs to Final Cartridge -- NO REU support
// - FCIII 101% with possible external REU: FC3-101% skips first 32 bytes of IO2
// - FCIII 101% with TR+ REU: FC3-101% passes first 32 bytes of IO2 to TR+ REU
// The first two scenarios are addressed in IO2Hndlr_FinalCartridgeIII()
// The third scenario is addressed in IO2Hndlr_FCIII_101_REU()

void InitHndlr_FinalCartridgeIII();                           
void IO1Hndlr_FinalCartridgeIII(uint8_t Address, bool R_Wn);  
void IO2Hndlr_FinalCartridgeIII(uint8_t Address, bool R_Wn);  
void CycleHndlr_FinalCartridgeIII(bool R_Wn);

void InitHndlr_FCIII_101_REU();
void IO2Hndlr_FCIII_101_REU(uint8_t Address, bool R_Wn); 

// Final Cartridge III, III+ and 101% without TR+ REU 
stcIOHandlers IOHndlr_FinalCartridgeIII =
{
  "FinalCartridgeIII",            // Name of handler, IOHNameLength max
  &InitHndlr_FinalCartridgeIII,   // Called once at handler startup
  &IO1Hndlr_FinalCartridgeIII,    // IO1 R/W handler
  &IO2Hndlr_FinalCartridgeIII,    // IO2 R/W handler
  NULL,                           // ROML Read handler, in addition to any ROM data sent
  NULL,                           // ROMH Read handler, in addition to any ROM data sent
  NULL,                           // Polled in main routine
  CycleHndlr_FinalCartridgeIII,   // called at the end of EVERY c64 cycle
};

// Final Cartridge III 101% with TR+ REU 
stcIOHandlers IOHndlr_FCIII_101_REU =  
{
  "FC3_101_REU",                  // Name of handler, IOHNameLength max 
  &InitHndlr_FCIII_101_REU,       // Called at handler start. Adds TR+ REU 
  &IO1Hndlr_FinalCartridgeIII,    // IO1 R/W handler
  &IO2Hndlr_FCIII_101_REU,        // IO2 R/W handler adds TR+ REU
  NULL,                           // ROML Read handler, in addition to any ROM data sent
  NULL,                           // ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_REU,              // Polled in main routine
  &CycleHndlr_FinalCartridgeIII,  // called at the end of EVERY c64 cycle
};  

extern volatile uint8_t EmulateVicCycles;
extern volatile uint32_t CycleCountdown;
static bool FC3_101 = false;
#define FC3_ControlReg RR_StatusReg //reused global: keep track of FC3_CR_HIDDEN

// REFERENCES
// http://www.pokefinder.org/wiki/Final_Cartridge_III_Internals.txt
// https://rr.pokefinder.org/wiki/Final_Cartridge_III_Internals_Errata.txt
// https://1541u-documentation.readthedocs.io/en/latest/howto/cartridges.html

// Final Cartridge III uses one write-only register at $DFFF
#define FC3_CR_HIDDEN    0b10000000   // Hide control register
#define FC3_CR_NMI       0b01000000   // (0 = low = active)
#define FC3_CR_GAME      0b00100000   // (0 = low = active)
#define FC3_CR_EXROM     0b00010000   // (0 = low = active)
#define FC3_CR_BANK3     0b00001000   // III=0 / III+ = Bank bit 3   
#define FC3_CR_BANK2     0b00000100   // III=0 / III+ = Bank bit 2 
#define FC3_CR_BANK1     0b00000010   // Bank bit 1
#define FC3_CR_BANK0     0b00000001   // Bank bit 0

void ProcessFC3ControlReg(uint8_t ControlReg)
{ 
   // not sure if active/inactive checks are really necessary.  
   if ((ControlReg & FC3_CR_EXROM) && !(ControlReg & FC3_CR_GAME)) // If UltiMAX
   {                                   
      if (EmulateVicCycles != true)    // if not active
      {
         NVIC_DISABLE_IRQ(IRQ_ENET);     
         NVIC_DISABLE_IRQ(IRQ_PIT);      
         EmulateVicCycles = true;        
      }  	
   }	
   else  // if not UltiMAX 
   {	 	                                   
   	  if (EmulateVicCycles == true)  // if active
      {                             
         NVIC_ENABLE_IRQ(IRQ_ENET);    
         NVIC_ENABLE_IRQ(IRQ_PIT);
         EmulateVicCycles = false;     
      }	
   }	
   
   if (FC3_ControlReg & FC3_CR_HIDDEN) 
   {
      SetLEDOff; // Mimic LED behavior of original Cartridge
   }
   
   if (NumCrtChips == 4)  BankNum = (ControlReg & 3);  //  64K FC III
   else BankNum = (ControlReg & 15);                   // 256K FC III+  
   	   
   LOROM_Image = CrtChips[BankNum].ChipROM;  // ROM at $8000
   HIROM_Image = LOROM_Image + 0x2000;       // ROM at $A000

   if (ControlReg & FC3_CR_NMI) SetNMIDeassert;      
   else SetNMIAssert;  
   	  
   if (ControlReg & FC3_CR_EXROM) SetExROMDeassert;  //rtBin8kHi or None
   else SetExROMAssert;                              //rtBin16k or 8kLo
   
   if (ControlReg & FC3_CR_GAME) SetGameDeassert;    //8kLo or None
   else SetGameAssert;                               //rtBin8kHi or rtBin16k
          
}

FLASHMEM void InitHndlr_FinalCartridgeIII()
{
   // Check for presense of "REU " in the REU range of every FC3 101% bank
   FC3_101 = (memcmp("REU ", CrtChips[0].ChipROM + 0x1f00, 4) == 0);

   fSpecialBtnChange = &SpecialBtn_SuperSnapshotV5; //same trigger as SSv5   
   CycleCountdown = 0;
   FC3_ControlReg = 0;
   ProcessFC3ControlReg(0);  // HW power-on reset  
}    

FLASHMEM void InitHndlr_FCIII_101_REU() // WITH TR+ REU
{
   InitHndlr_FinalCartridgeIII();  // Initialize Final Cartridge III handler
   InitHndlr_REU();  // Initialize REU handler for REU compatibility
   fSpecialBtnChange = &SpecialBtn_FreezeCRT_REU; // replace handler with long/short press
}

// IO1: Mirrrors $1E00 to $1EFF of current ROM bank
void IO1Hndlr_FinalCartridgeIII(uint8_t Address, bool R_Wn)
{
   if (R_Wn)  // IO1 Read
   	  DataPortWriteWaitLog(LOROM_Image[0x1e00 + Address]); 
}

// FC3 101% with TR+ REU: if adddress is in REU range let REU handle, 
// Otherwise pass to Standard Final Cartridge III I/O2 handler
void IO2Hndlr_FCIII_101_REU(uint8_t Address, bool R_Wn)
{
   if (Address <= 0x1f) 
      IO2Hndlr_REU(Address, R_Wn);
   else  
   {
      if (R_Wn) // IO2 Read 
      { 
         DataPortWriteWaitLog(LOROM_Image[0x1f00+Address]); 
      }
      else  // Write to ControlReg @ $1FFF if FC3_CR_HIDDEN not set
      {  
         if ((Address == 0xFF) && !(FC3_ControlReg & FC3_CR_HIDDEN))  
         {
            FC3_ControlReg = DataPortWaitRead();
            ProcessFC3ControlReg(FC3_ControlReg);
         } 
      }
   } 
}

// IO2: Mirrors $1F00 to $1FFF of current ROM bank
void IO2Hndlr_FinalCartridgeIII(uint8_t Address, bool R_Wn)
{
   // If NOT (First 32 bytes && FC3_101) perform IO2 r/w, 
   if (!((Address <= 0x1f) && (FC3_101)))
   {
      if (R_Wn) // IO2 Read 
      { 
         DataPortWriteWaitLog(LOROM_Image[0x1f00+Address]); 
      }
      else  // Write to ControlReg @ $1FFF if FC3_CR_HIDDEN not set
      {  
         if ((Address == 0xFF) && !(FC3_ControlReg & FC3_CR_HIDDEN))  
         {
            FC3_ControlReg = DataPortWaitRead();
            ProcessFC3ControlReg(FC3_ControlReg);
         } 
      }
   }
}

// How the Final Cartridge III Freezer Works 
//   https://www.pagetable.com/?p=1810
void CycleHndlr_FinalCartridgeIII(bool R_Wn)
{
   if (CycleCountdown)  // Button press sets CycleCountdown = 255
   { 	  
      FC3_ControlReg &= ~FC3_CR_HIDDEN;  // clear register-hidden bit
      SetLEDOn;                          

   	  // wait for a read, then assert NMI and set countdown to 3 (below)
      if (CycleCountdown == CycCntFreeze)     // If still = 255    
      {       		  	 
         if (R_Wn)             //assert NMI during read cycle                          
         {
            SetNMIAssert;      
         }
         else return; //preserve CycCntFreeze state, wait for read
      }

      // initial read has occured: wait for 3 consecutive writes
      if (R_Wn) CycleCountdown = CycCntNumWr; // reset to 3 if read
      else if(--CycleCountdown == 0)          
      {
         // Assert GAME, de-assert NMI
         FC3_ControlReg = ((FC3_ControlReg & ~FC3_CR_GAME) | FC3_CR_NMI) ;
         ProcessFC3ControlReg(FC3_ControlReg); 
      }
   }
}

