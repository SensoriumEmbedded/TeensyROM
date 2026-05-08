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


   // Prevents actual writes to data bus for HIRAM sense tuning and 
   //  performs HIRAM check every time to drive Debug signal (enable KERNAL_DEBUG_SIG_CONT).
   //  Scope ROMH and Debug to detect incorrect detections
// #define KERNAL_MONITOR_ONLY   
   
   // Drives debug signal for trigger: falling edge/low=ROM, rising edge/high=RAM:
// #define KERNAL_DEBUG_SIG_CONT

void InitHndlr_KernalReplace();                           

stcIOHandlers IOHndlr_KernalReplace =
{
  "Kernal Replace",         //Name of handler (IOHNameLength max)
  &InitHndlr_KernalReplace, //Called once at handler startup
  NULL,                     //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  NULL,                     //Polled in main routine
  NULL,                     //called at the end of EVERY c64 cycle
};

#define KernalBin MIDIRxBuf

uint8_t HIRAM_State;

enum enumHIRAM_States
{
   HIRAM_State_Unknown,
   HIRAM_State_ROM,
   HIRAM_State_RAM
};

extern volatile uint8_t doReset;

//______________________________________________________________________________________________

FASTRUN bool KernalCheck(uint16_t Address, bool R_Wn)
{
   //monitor port $0001 for changes
   if ((Address & 0xfffe) == 0  && !R_Wn) HIRAM_State = HIRAM_State_Unknown;  //write to addr 0 or 1 causes re-evaluation of HIRAM

   if( (Address & 0xe000) != 0xe000 || !R_Wn || !GP9_BA(ReadGPIO9)) return false;  //continue with cycle processing

   // CPU read access from address $E000..$FFFF
   
   //prepare modified Address for Skoe HIRAM check (if needed)
   uint32_t RegAddrBits = ((Address & 0xbfff) << 16); //drive A14 low
   CORE_PIN19_PORTSET = RegAddrBits; //set address port value to be ready for output drive
   CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;
   
   //prepare the kernal byte to be pushed onto the bus (if needed)
   uint8_t Data = KernalBin[Address & 0x1fff];
   uint32_t RegDataBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN10_PORTSET = RegDataBits;
   CORE_PIN10_PORTCLEAR = ~RegDataBits & GP7_DataMask;
   //WaitUntil_nS(nS_DataSetup); //>300 from Phi2 rise to game assert
   
   SetGameAssert;
   
#ifndef KERNAL_MONITOR_ONLY
   if (HIRAM_State == HIRAM_State_Unknown) 
#endif
   {  //determine if it's reading RAM or KERNAL w/ Skoe method
      SetExROMAssert;
      SetAddrBufsOut;   //set address buffers to output
      SetAddrPortDirOut;//set address ports to output 
      
      //wait prop delay for ROMH to react
      //Measured ~30nS via o-scope
      // Cyc=nS*.816       nS=Cyc/.816       uint32_t cycles = nSToCyc(nS);
      uint32_t begin = ARM_DWT_CYCCNT;
      while (ARM_DWT_CYCCNT - begin < Cyc_KernProp);
      
      HIRAM_State = (GP9_ROMH(ReadGPIO9)==0 ? HIRAM_State_ROM : HIRAM_State_RAM); //read ROMH to determine if it's a Kernal or RAM access
      
      SetAddrPortDirIn;//set address ports to input
      SetAddrBufsIn;   //set address buffers to input
      SetExROMDeassert;
   }
   
   if (HIRAM_State == HIRAM_State_ROM) 
   {
#ifndef KERNAL_MONITOR_ONLY
      //Drive Data bus:
      SetDataBufOut; //buffer out first
      SetDataPortDirOut; //then set data ports to outputs
      
      //WaitUntil_nS(nS_DataHold);  
      uint32_t Cyc_DataHold = nSToCyc(nS_DataHold); //avoid calculating every time
      while((ARM_DWT_CYCCNT-StartCycCnt) < Cyc_DataHold)
      if(!GP6_Phi2(ReadGPIO6)) break; //make sure Phi2 is still high, about 50nS of overshoot into VIC cycle if detected

      SetDataPortDirIn; //set data ports back to inputs/default
      SetDataBufIn;     //then set buffer dir to input
#endif
#ifdef KERNAL_DEBUG_SIG_CONT
      SetDebugDeassert; // falling edge/low=ROM
   }
   else 
   {
      SetDebugAssert;  //rising edge/high=RAM
#endif
   }
   SetGameDeassert;
    
   return true;  //Skip out of phi2 isr
}

//______________________________________________________________________________________________

extern void EEPreadStr(uint16_t addr, char* buf);
extern RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName);
extern bool SDFullInit();
extern bool USBFileSystemWait();
extern FS *FSfromSourceID(RegMenuTypes SourceID);

FLASHMEM void InitHndlr_KernalReplace()
{

   if (KernalBin != NULL) free((void*)KernalBin);
   KernalBin = (uint8_t*)malloc(8192);
   //KernalBin = RAM_Image;
   
   //load kernal image into RAM:
   char Filename[MaxPathLength];
   EEPreadStr(eepAdKERNALBinName, Filename);
   char *ptrFileName = Filename; //pointer to move past SD/USB/TR:
   Serial.printf("Loading Kernal: %s\n", Filename);
   RegMenuTypes MenuSourceID = RegMenuTypeFromFileName(&ptrFileName);

   if (MenuSourceID == rmtSD) SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected
   if (MenuSourceID == rmtUSBDrive) USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up
   //rmtTeensy not allowed, no kernal files in Teensy Mem
   
   FS *sourceFS = FSfromSourceID(MenuSourceID);
   File LoadFile = sourceFS->open(ptrFileName, FILE_READ);
   if (!LoadFile)
   {
      Serial.println("Not found!");
      return;
   }
   
   if (LoadFile.size() != 8192)
   {
      Serial.println("Wrong Size!");
      return;
   }

   //uint32_t StartmS = millis();
   uint32_t CharNum = 0;
   while (LoadFile.available())
   {
      KernalBin[CharNum++] = LoadFile.read();
   }
   //Serial.printf("Read %lu Bytes in %lumS\n", CharNum, millis()-StartmS);
   Serial.printf("Read %lu Bytes\nKernal replace Enabled\n", CharNum);
   LoadFile.close();
   delay(250);
   
   //hook kernal checks:
   HIRAM_State = HIRAM_State_Unknown;
   fBusSnoop = &KernalCheck; 

}

