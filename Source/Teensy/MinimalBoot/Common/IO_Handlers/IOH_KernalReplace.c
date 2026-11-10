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

#ifdef Fab04_GlobalKernalReplace
	uint8_t *KernalBin = NULL; //use unique global pointer
#else
	#define KernalBin MIDIRxBuf //use Special IO resource
#endif

uint8_t HIRAM_State;

enum enumHIRAM_States
{
   HIRAM_State_Unknown,
   HIRAM_State_ROM,
   HIRAM_State_RAM
};

extern volatile uint8_t doReset;

//HIRAM sense: how KernalCheck finds out, after a write to $00/$01, whether the KERNAL is banked in.
//  (The 6510 doesn't put the value of a $00/$01 write on the external data bus, so it can't be tracked from the writes.)
//  Skoe: on the first $E000+ read, drive A14 low with /EXROM+/GAME asserted (16K mode) and look at ROMH.
//     This needs the PLA to see the address the TR drives over the CPU's.  On a C128 in C64 mode it doesn't: the
//     expansion port's address lines come through a transceiver that drives towards the port outside DMA, so ROMH never
//     answers, every sense reads "RAM", and the replacement image is never served.
//  DMA: right after the write, hold the CPU with /DMA for one cycle and, as bus master, read $A000 with /EXROM+/GAME
//     asserted: the PLA selects ROMH there only with HIRAM set.  The cycle after a store to $00/$01 is always the next
//     opcode fetch, a read, so the CPU halts on it.  Costs the CPU one cycle per write to $00/$01 (more when the VIC
//     holds the bus).  Used on a C128.
enum enumHIRAMSenseModes
{
   KSense_Skoe = 0,
   KSense_DMA  = 2,
   KSense_Auto = 9,  //DMA on a C128 (the menu's machine report), Skoe otherwise
};
uint32_t KSenseMode = KSense_Auto;  //serial tx# (Dbg_SerTimChg), applied at the next launch
uint8_t  KSenseEff  = KSense_Skoe;  //the method the last launch hooked (KernalCheckArm)

FLASHMEM void KSenseApplyMode()
{
   KSenseEff = (KSenseMode == KSense_Auto) ? ((IO1[wRegVid_TOD_Clks] & rvtcC128) ? KSense_DMA : KSense_Skoe) :
               (KSenseMode == KSense_DMA) ? KSense_DMA : KSense_Skoe;
   if (KSenseEff == KSense_DMA) Serial.println(PSTR("Kernal HIRAM sense: DMA"));
   else Serial.println(PSTR("Kernal HIRAM sense: Skoe"));
}

enum enumKDS_States
{
   KDS_Idle,
   KDS_Skip,         //the sense cycle's rising-edge interrupt arrives late (in Phi2 low): drop it.  It always arrives: the
                     //  Teensy core clears the GPIO interrupt flag before it calls isrPHI2, so the edge during the sense sets it again.
   KDS_WaitBus,      //DMA asserted, BA was low: try the sense read on the next cycle
   KDS_AfterRead01,  //a write to $00/$01 right after a read of $00/$01: the first of a read-modify-write's two
                     //  writes, or a store after an indexed dummy read.  The next cycle tells which.
};
volatile uint8_t KDS_State = KDS_Idle;
uint8_t  KDS_Guess = HIRAM_State_ROM;  //last verdict, for a KERNAL read no sense could precede
uint32_t KDS_Read01Cyc = 0;            //ARM_DWT_CYCCNT of the last CPU read of $00/$01
bool     KDS_Read01Valid = false;

//Phi2 low (StartCycCnt = falling), /DMA asserted: one DMA read of $A000 with /GAME+/EXROM, address timing as DMAByte's.
//false: bus not available (badline, sprite fetch), try again on the next cycle
FASTRUN bool KDS_SenseCycle()
{
   WaitUntil_nS_fine(nS_DMABAWait);
   if (!GP9_BA(ReadGPIO9)) return false;

   uint32_t RegAddrBits = (0xA000 << 16);
   CORE_PIN19_PORTSET = RegAddrBits; //set address port value to be ready for output drive
   CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;
   WaitUntil_nS(nS_DMASetup);  //just ahead of Phi2 rising
   StartCycCnt = ARM_DWT_CYCCNT;
   SetAddrBufsOut;
   SetAddrPortDirOut;
   SetExROMAssert;
   SetGameAssert;

   WaitUntil_nS_fine(nS_KSenseSample);
   HIRAM_State = KDS_Guess = (GP9_ROMH(ReadGPIO9)==0 ? HIRAM_State_ROM : HIRAM_State_RAM);

   WaitUntil_nS_fine(nS_DMADataSetup); //keep the address valid as long as a DMA read does
   SetGameDeassert;
   SetExROMDeassert;
   SetAddrPortDirIn;
   SetAddrBufsIn;
   return true;
}

//Phi2 low, /DMA asserted, StartCycCnt = falling
FASTRUN void KDS_Run()
{
   if (!KDS_SenseCycle())
   {
      KDS_State = KDS_WaitBus;
      return;
   }
   while (GP6_Phi2(ReadGPIO6)); //Phi2 falling after the sense cycle
   SetDMADeassert;              //release in Phi2 low, as isrPHI2's DMA_S_StartDisable does
   KDS_State = KDS_Skip;
}

//Phi2 high of the CPU's write to $00/$01
FASTRUN void KDS_Begin()
{
   if (DMA_State != DMA_S_DisableReady)
   {  //another DMA user (REU, remote memory access) owns the line
      HIRAM_State = KDS_Guess;
      return;
   }
   while (GP6_Phi2(ReadGPIO6)); //Phi2 falling
   StartCycCnt = ARM_DWT_CYCCNT;
   WaitUntil_nS(nS_DMAAssert);
   SetDMAAssert;
   KDS_Run();
}

FASTRUN bool KernalCheck(uint16_t Address, bool R_Wn);

//KernalCheckDMA's cycles off the common path: a sense pending, or a $00/$01 access.  Returns the cycle's result:
//true when the sense used the cycle (nothing else may answer it), else KernalCheck's
FASTRUN bool KDS_Slow(uint16_t Address, bool R_Wn)
{
   switch (KDS_State)
   {
      case KDS_Skip:
         KDS_State = KDS_Idle;
         return true;
      case KDS_WaitBus:
         while (!GP6_Phi2(ReadGPIO6)); //a late call (in Phi2 low) waits for the next cycle
         while (GP6_Phi2(ReadGPIO6));
         StartCycCnt = ARM_DWT_CYCCNT;
         KDS_Run();
         return true;
   }
   if ((Address & 0xfffe) == 0)
   {
      if (R_Wn)
      {
         KDS_Read01Cyc = ARM_DWT_CYCCNT;
         KDS_Read01Valid = true;
      }
      else
      {  //write to $00/$01
         HIRAM_State = HIRAM_State_Unknown; //Unknown only while a sense is pending (see KernalCheckDMA): each path below senses,
                                            //  waits for the bus, or takes the last verdict
         bool AfterRead = KDS_Read01Valid && (ARM_DWT_CYCCNT - KDS_Read01Cyc) < nSToCyc(1500); //the previous cycle
         KDS_Read01Valid = false;
         if (AfterRead && KDS_State != KDS_AfterRead01)
         {
            KDS_State = KDS_AfterRead01;
            return true;
         }
         KDS_State = KDS_Idle;
         KDS_Begin();
         return true;
      }
   }
   if (KDS_State == KDS_AfterRead01)
   {  //it was a store after a dummy read: no sense is coming, so this read is served on the last verdict
      //  (Unknown must not outlive KDS_Idle, see KernalCheckDMA)
      KDS_State = KDS_Idle;
      HIRAM_State = KDS_Guess;
   }
   return KernalCheck(Address, R_Wn);
}

//______________________________________________________________________________________________

//The C64's check (Skoe sense).  It runs ahead of every IO1/IO2 answer while hooked, so its common cycle carries nothing
//  for the DMA sense: a C128 hooks KernalCheckDMA instead, which comes here once its own tests are done.
FASTRUN bool KernalCheck(uint16_t Address, bool R_Wn)
{
   //monitor port $0001 for changes
   if (!R_Wn && (Address & 0xfffe) == 0) HIRAM_State = HIRAM_State_Unknown;  //write to addr 0 or 1 causes re-evaluation of HIRAM

   if (!R_Wn || (Address & 0xe000) != 0xe000 || !GP9_BA(ReadGPIO9)) return false;  //continue with cycle processing

   // CPU read access from address $E000..$FFFF
#ifndef KERNAL_MONITOR_ONLY
   //HIRAM known to be RAM (no write to $00/$01 since it was sensed): the C64's own RAM answers, so leave /GAME and the
   //data bus alone.  Asserting /GAME (Ultimax) here takes the read away from the RAM.
   if (HIRAM_State == HIRAM_State_RAM) return true;
#endif
   
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

//The C128's check (DMA sense), hooked instead of KernalCheck.  It runs ahead of every IO1/IO2 answer while hooked, so the
//  common cycle costs only these tests: work added here answers IO late.  Written as one combined test, the cycle needs
//  no saved register and both exits are tail calls (two separate tests made GCC 11 save one: check the disassembly).
//KDS_Slow takes every cycle that writes $00/$01, so KernalCheck never resets HIRAM_State from here.  HIRAM_State is only
//  Unknown while a sense is pending (KDS_State not Idle), so KernalCheck never runs its Skoe sense for a C128.
FASTRUN bool KernalCheckDMA(uint16_t Address, bool R_Wn)
{
   uint32_t Slow = KDS_State | (uint32_t)((Address & 0xfffe) == 0); //a sense pending, or a $00/$01 access
   if (Slow) return KDS_Slow(Address, R_Wn);
   return KernalCheck(Address, R_Wn);
}

typedef bool (*KernalCheckFn)(uint16_t Address, bool R_Wn);

//The check to hook for the method KSenseApplyMode() picked, with HIRAM_State set for it.  Chosen once per launch, so
//  neither check tests the method on every cycle, and a tx# command can't switch methods under a sense in flight.
FLASHMEM KernalCheckFn KernalCheckArm()
{
   if (KSenseEff == KSense_DMA)
   {  //no sense precedes the first KERNAL read: serve it on the last verdict (KDS_Guess), never Unknown (see KernalCheckDMA)
      HIRAM_State = KDS_Guess;
      return &KernalCheckDMA;
   }
   HIRAM_State = HIRAM_State_Unknown;  //the first $E000+ read senses
   return &KernalCheck;
}

//______________________________________________________________________________________________

extern void EEPreadStr(uint16_t addr, char* buf);
extern RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName);
extern bool SDFullInit();
extern bool USBFileSystemWait();
extern FS *FSfromSourceID(RegMenuTypes SourceID);
extern bool HandshakeSnoop(uint16_t Address, bool R_Wn);

#ifdef Fab04_GlobalKernalReplace
	bool (*fKernRepl)(uint16_t Address, bool R_Wn) = NULL;    //return true to skip out of phi2 isr
#endif

FLASHMEM void FreeKernalBin()
{  //nothing loaded: a NULL KernalBin keeps InitHndlr_KernalReplace from restaging KernalCheck
#ifndef Fab04_GlobalKernalReplace
   free((void*)KernalBin);
   KernalBin = NULL;
#endif
}

void FnetLoopPollStart(); //Teensy.ino

FLASHMEM void InitHndlr_KERNALReplace_PreStart() //not called automatically via init; called earlier, manually, from KERNALPreStart()
{
#ifdef Fab04_GlobalKernalReplace
   fKernRepl = NULL;  //default, make sure off during loading
   if (KernalBin == NULL) KernalBin = (uint8_t*)malloc(8192);
#else
   if (KernalBin != NULL) free((void*)KernalBin);
   KernalBin = (uint8_t*)malloc(8192);
   //KernalBin = RAM_Image;
#endif
   if (KernalBin == NULL)
   {
      Serial.println("Kernal buffer alloc failed!");
      return;
   }

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
      FreeKernalBin();
      return;
   }
   
   if (LoadFile.size() != 8192)
   {
      Serial.println("Wrong Size!");
      LoadFile.close();
      FreeKernalBin();
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
   KSenseApplyMode();
   KDS_State = KDS_Idle;
   KDS_Read01Valid = false;
   KDS_Guess = HIRAM_State_ROM;
   delay(250);
   
   //hook kernal checks: runs before BASIC init, well before any PRG-load handshake (see
   //HandshakeSnoop in IOH_TeensyROM.c) could be armed, so it's safe to touch fBusSnoop directly here.
#ifdef Fab04_GlobalKernalReplace
   fKernRepl = KernalCheckArm();
#else
   fBusSnoop = KernalCheckArm();
#endif
   FnetLoopPollStart(); //Ethernet poll off the PIT timer from now on (see FnetLoopPollService in Teensy.ino)
}

FLASHMEM void InitHndlr_KernalReplace()
{  //runs via the normal IOHandlerInit() handler-swap path (e.g. once PRG load completes).
   //The actual kernal load/setup already happened earlier in InitHndlr_KERNALReplace_PreStart();
   //this just re-stages the hook so the PRG-load handshake hands fBusSnoop back to KernalCheck
   //once it completes, instead of leaving it NULL.
#ifndef Fab04_GlobalKernalReplace  //Fab04_GlobalKernalReplace uses fKernRepl, which the handshake never touches -- nothing to restage
   //Only after a PRG load (the handshake owns fBusSnoop) whose PreStart loaded a kernal. PreStart
   //runs on PRG launches only, but a generic CRT inherits Kernal Replace as its Special IO too:
   //hooked there, KernalCheck reads an unloaded KernalBin and drives GAME/EXROM over the cart's own.
   if (fBusSnoop != &HandshakeSnoop || KernalBin == NULL) return;
   KDS_State = KDS_Idle;  //KernalCheck is unhooked during the handshake: no sense can be in flight
   KDS_Read01Valid = false;
   PendingfBusSnoop = KernalCheckArm(); //not fBusSnoop directly -- may still be mid PRG-load handshake, see HandshakeSnoop
#endif
}

