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


#include <SD.h>
#include <EEPROM.h>
#include "Min_TeensyROM.h"
#include "Common/Common_Defs.h"
#include "Min_Menu_Regs.h"
#include "Min_DriveDirLoad.h"
#include "Min_IOHandlers.h"

uint8_t RAM_Image[RAM_ImageSize]; //Main RAM1 file storage buffer
volatile uint8_t BtnPressed = false; 
volatile uint8_t EmulateVicCycles = false;
uint8_t CurrentIOHandler = IOH_None;
StructMenuItem DriveDirMenu;
char DriveDirPath[MaxPathLength];
uint16_t LOROM_Mask, HIROM_Mask;
//bool RemoteLaunched = false; //last app was launched remotely

#include "Common/ISRs.c"

extern "C" uint32_t set_arm_clock(uint32_t frequency);
extern float tempmonGetTemp(void);

void setup() 
{
   set_arm_clock(816000000);  //slight overclocking, no cooling required
   
   Serial.begin(115200);
   if (CrashReport) Serial.print(CrashReport);

   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
   DataBufDisable; //buffer disabled
   SetDataPortDirOut; //default to output (for C64 Read)
   SetDMADeassert;
   SetIRQDeassert;
   SetNMIDeassert;
   SetLEDOn;
   SetResetAssert; //assert reset until main loop()
  
#ifdef DbgFab0_3plus
   pinMode(DotClk_Debug_PIN, OUTPUT);  //p28 is Debug output on fab 0.3+
   SetDebugDeassert;
#else
   pinMode(DotClk_Debug_PIN, INPUT_PULLUP);  //p28 is Dot_Clk input (unused) on fab 0.2x
#endif

   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Reset_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Reset_Btn_In_PIN), isrButton, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   NVIC_SET_PRIORITY(IRQ_GPIO6789,16); //set HW ints as high priority, otherwise ethernet int timer causes misses
  
#ifdef Dbg_TestMin
   //write a game path to execute
   //EEPwriteStr(eepAdCrtBootName, "/OneLoad v5/Main- MagicDesk CRTs/Auriga.crt");
   //EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/802k Briley Witch Chronicles 2 v1.0.3.crt");
   //EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/770k Where in USA is Carmen Sandiego [EasyFlash].crt");  //PAL only

   //very large, for testing swaps:
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/tools/Digi Player Demo (2012-06-17)(Onslaught)[EasyFlash 2012-06-17].crt");
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/tools/Multi Easy (2013-04-11)(Lord Crass)[EasyFlash].crt");                                                                                                                                       //  Swaps?   performance
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/commercial_20XX_releases/A_Pig_Quest_1.02_ef.crt");                            //  some     no fails observed
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/commercial_20XX_releases/A Pig Quest +2 {EasyFlash}[EX].crt");             //not on SD!
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/commercial_20XX_releases/a_pig_quest_v102_+9_[trex].crt");               //not on SD!
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/oneload64v4/Extras/OtherCRTs/Turrican & Turrican II [EasyFlash].crt"); 
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/oneload64v4/AlternativeFormats/EasyFlash/OneLoad64-Vol#5.crt"); 
   //EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/882k Maniac Mansion & Zak McKracken [EasyFlash].crt");
   EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/882k Last Ninja 1 + 2, The [EasyFlash].crt");

   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/ezf 48Khz_hifi_Elvis_Costello_[EASYFLASH].crt");   //good test of all banks, *does not* click during swaps at the end
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/Other-Large/hf_audio_playback_01.crt"); //good test of all banks, clicks during swaps at the end
   //EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/954k Eye of the Beholder - v1.00 [EasyFlash].crt");        //swaps quickly during play                            //   Lots!
   //EEPwriteStr(eepAdCrtBootName, "/validation/crts/32_EasyFlash/Other-Large/svc64_update2.crt");  //SNK vs CAPCOM,  swaps quickly during play                                //   Lots!
 
 EEPROM.write(eepAdMinBootInd, MinBootInd_ExecuteMin);
#endif  
   
   if (EEPROM.read(eepAdMinBootInd) != MinBootInd_ExecuteMin) runMainTRApp(); //jump to main app if not booting a CRT
   
   uint32_t MagNumRead;
   EEPROM.get(eepAdMagicNum, MagNumRead);
   if (MagNumRead != eepMagicNum) runMainTRApp(); //jump to main app if EEP not initialized/matching main
   
   //we have a crt to load in minimal mode, procede....
   
   EEPROM.write(eepAdMinBootInd, MinBootInd_SkipMin); //clear the boot flag for next boot default, in case power is lost

   char *CrtBootNamePath = (char*)malloc(MaxPathLength);
   EEPreadNBuf(eepAdCrtBootName, (uint8_t*)CrtBootNamePath, MaxPathLength); //load the source/path/name from EEPROM
   Serial.printf("Sel CRT: %s\n", CrtBootNamePath);

   //SetUpMainMenuROM();
   //SetGameDeassert;
   //SetExROMAssert; //emulate 8k cart ROM
   LOROM_Image = NULL; //TeensyROMC64_bin;
   HIROM_Image = NULL;
   LOROM_Mask = HIROM_Mask = 0x1fff;
   EmulateVicCycles = false;
   FreeCrtChips();
   
   strcpy(DriveDirPath, "/");
   SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated

   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));
   Serial.printf("\nTeensyROM minimal %s is on-line\n", strVersionNumber);
   Serial.printf(" %luMHz  %.1fC\n FW: %s, %s\n", (F_CPU_ACTUAL/1000000), tempmonGetTemp(), __DATE__, __TIME__);
   
#ifdef Dbg_TestMin
   //calc/show free RAM space for CRT:
   uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
   Serial.printf(" RAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);   
   uint8_t NumChips = RAM2blocks();
   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
   NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
   Serial.printf(" RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
   Serial.printf(" Swap Blks: %luK (%lu blks)\n", Num8kSwapBuffers*8, Num8kSwapBuffers);
   CrtMax += NumChips*8;
   Serial.printf(" %luk free for CRT\n", (uint32_t)(CrtMax*1.004));  //larger File size due to header info.
#endif

   // assuming it's a .crt file, and present on SD drive (verified in main image)
   LoadCRT(CrtBootNamePath);
   if (!doReset) 
   {
      Serial.print("CRT not loaded, Abort!\n");
      runMainTRApp_FromMin(); //didn't load right if not calling for reset
   }
} 
     
void loop()
{
   if (BtnPressed)
   {
      runMainTRApp_FromMin(); 
      //Serial.print("Button detected (minimal)\n"); 
   }
   
   if (doReset)
   {
      SetResetAssert; 
      Serial.println("Resetting C64"); 
      Serial.flush();
      delay(50); 
      doReset=false;
      SetResetDeassert;
   }
  
   if (Serial.available()) ServiceSerial();
   
   //handler specific polling items:
   if (IOHandler[CurrentIOHandler]->PollingHndlr != NULL) IOHandler[CurrentIOHandler]->PollingHndlr();
}


void EEPwriteNBuf(uint16_t addr, const uint8_t* buf, uint8_t len)
{
   while (len--) EEPROM.write(addr+len, buf[len]);    
}

void EEPwriteStr(uint16_t addr, const char* buf)
{
   EEPwriteNBuf(addr, (uint8_t*)buf, strlen(buf)+1); //include terminator    
}

void EEPreadNBuf(uint16_t addr, uint8_t* buf, uint16_t len)
{
   while (len--) buf[len] = EEPROM.read(addr+len);   
}

void EEPreadStr(uint16_t addr, char* buf)
{
   uint16_t CharNum = 0;
   
   do
   {
      buf[CharNum] = EEPROM.read(addr+CharNum); 
   } while (buf[CharNum++] !=0); //end on termination, but include it in buffer
}


void LoadCRT( const char *FileNamePath)
{
   //Launch (emulate) .crt file

   ///IO1[rWRegCurrMenuWAIT] = rmtSD;
   SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   
   //set path & filename
   strcpy(DriveDirPath, FileNamePath);
   char* ptrFilename = strrchr(DriveDirPath, '/'); //pointer file name, find last slash
   if (ptrFilename == NULL) 
   {  //no path:
      strcpy(DriveDirPath, "/");
      ptrFilename = (char*)FileNamePath; 
   }
   else
   {  //separate path/filename
      *ptrFilename = 0; //terminate DriveDirPath
      ptrFilename++; //inc to point to filename
   }
   
   DriveDirMenu.Name = ptrFilename;
   DriveDirMenu.ItemType = rtFileCrt; //Assoc_Ext_ItemType(DriveDirMenu[0].Name);
     
   HandleExecution();
}

//from IOH_TeensyROM.c :
FLASHMEM uint8_t RAM2blocks()
{  //see how many 8k banks will fit in RAM2
   char *ptrChip[70]; //64 8k blocks would be 512k (size of RAM2)
   uint8_t ChipNum = 0;
   while(1)
   {
      ptrChip[ChipNum] = (char *)malloc(8192);
      if (ptrChip[ChipNum] == NULL) break;
      ChipNum++;
   } 
   for(uint8_t Cnt=0; Cnt < ChipNum; Cnt++) free(ptrChip[Cnt]);
   //Serial.printf("Created/freed %d  8k blocks (%dk total) in RAM2\n", ChipNum, ChipNum*8);
   return ChipNum;
}
