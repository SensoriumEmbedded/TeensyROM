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

//  TeensyROM: A C64 ROM emulator and loader/interface cartidge based on the Teensy 4.1
//  Copyright (c) 2023 Travis Smith <travis@sensoriumembedded.com> 


#include <SD.h>
//#include <USBHost_t36.h>
//#include <SPI.h>
//#include <NativeEthernet.h>
//#include <NativeEthernetUdp.h>
#include <EEPROM.h>
#include "TeensyROM.h"
#include "Menu_Regs.h"
#include "DriveDirLoad.h"
//#include "MainMenuItems.h"
#include "IOHandlers.h"

uint8_t RAM_Image[RAM_ImageSize]; //Main RAM1 file storage buffer
volatile uint8_t BtnPressed = false; 
volatile uint8_t EmulateVicCycles = false;
uint8_t CurrentIOHandler = IOH_None;
StructMenuItem *DriveDirMenu = NULL;
uint16_t NumDrvDirMenuItems = 0;
char DriveDirPath[MaxPathLength];
uint16_t LOROM_Mask, HIROM_Mask;
bool RemoteLaunched = false; //last app was launched remotely

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
   SetDebugDeassert;
   SetResetAssert; //assert reset until main loop()
  
   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Reset_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Reset_Btn_In_PIN), isrButton, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   NVIC_SET_PRIORITY(IRQ_GPIO6789,16); //set HW ints as high priority, otherwise ethernet int timer causes misses
   
   //myusbHost.begin(); // Start USBHost_t36, HUB(s) and USB devices.
#ifdef nfcScanner
   nfcInit(); //connect to nfc scanner
#endif
  
#ifdef Dbg_TestMin
   //write a game path to execute
   EEPwriteStr(eepAdCrtBootName, "/OneLoad v5/Main- MagicDesk CRTs/Auriga.crt");
#endif  
  
   uint32_t MagNumRead;
   EEPROM.get(eepAdMagicNum, MagNumRead);
   if (MagNumRead != eepMagicNum) runApp(UpperAddr); //jump to main app if EEP not initialized
   if (EEPROM.read(eepAdCrtBootName) == 0) runApp(UpperAddr); //jump to main app if not booting a CRT

   char *CrtBootNamePath = (char*)malloc(MaxPathLength);
   EEPreadNBuf(eepAdCrtBootName, (uint8_t*)CrtBootNamePath, MaxPathLength); //load the source/path/name from EEPROM
   Serial.printf("Sel CRT: %s\n", CrtBootNamePath);
   EEPROM.write(eepAdCrtBootName, 0); //clear the boot flag for next boot

   IO1 = (uint8_t*)calloc(IO1Size, sizeof(uint8_t)); //allocate IO1 space and init to 0
   IO1[rwRegStatus]        = rsReady;
   IO1[rWRegCurrMenuWAIT] = rmtTeensy;
   IO1[rRegPresence1]     = 0x55;   
   IO1[rRegPresence2]     = 0xAA;   
   for (uint16_t reg=rRegSIDStrStart; reg<rRegSIDStringTerm; reg++) IO1[reg]=' '; 
   IO1[rRegSIDStringTerm] = 0;   
   IO1[rwRegPwrUpDefaults]= EEPROM.read(eepAdPwrUpDefaults);
   IO1[rwRegTimezone]     = EEPROM.read(eepAdTimezone);  
   //IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr); //done each entry into menu
   SetUpMainMenuROM();
   MenuChange(); //set up drive path, menu source/size

   for(uint8_t cnt=0; cnt<IOH_Num_Handlers; cnt++) PadSpace(IOHandler[cnt]->Name, IOHNameLength-1); //done so selection shown on c64 overwrites previous

   //for(uint8_t cnt=0; cnt<NumPageLinkBuffs; cnt++) PageLinkBuff[cnt] = NULL; //initialize page link buffer for swiftlink browser mode
   //for(uint8_t cnt=0; cnt<NumPrevURLQueues; cnt++) PrevURLQueue[cnt] = NULL; //initialize previous link buffer for swiftlink browser mode
   //for(uint8_t cnt=0; cnt<RxQueueNumBlocks; cnt++) RxQueue[cnt] = NULL;      //initialize RxQueue for swiftlink

   //StrSIDInfo = (char*)calloc(StrSIDInfoSize, sizeof(char)); //SID header info storage
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));
   sprintf(SerialStringBuf, "       FW: %s, %s\r\n   Teensy: %luMHz  %.1fC", __DATE__, __TIME__, (F_CPU_ACTUAL/1000000), tempmonGetTemp());
   Serial.printf("\n%s\nTeensyROM %s is on-line\n", SerialStringBuf, strVersionNumber);
   
#ifdef Dbg_TestMin
   //calc/show free RAM space for CRT:
   uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
   //Serial.printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);   
   uint8_t NumChips = RAM2blocks();
   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
   NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
   CrtMax += NumChips*8;
   Serial.printf(" %luk free for CRT\n", (uint32_t)(CrtMax*1.004));  //larger File size due to header info.
#endif

   //***todo: verify it's a .crt file, and present on SD drive
   
   LoadCRT(CrtBootNamePath);
   
} 
     
void loop()
{
   if (BtnPressed)
   {
      runApp(UpperAddr); 
      Serial.print("Button detected (minimal)\n"); 
      //SetLEDOn;
      //BtnPressed=false;
      //IO1[rwRegIRQ_CMD] = ricmdNone; //just to be sure, should already be 0/none
      //if (RemoteLaunched)
      //{
      //   IO1[rWRegCurrMenuWAIT] = rmtTeensy;
      //   MenuChange();
      //   RemoteLaunched = false;
      //   Printf_dbg("Remote recovery\n"); 
      //}   
      //SetUpMainMenuROM(); //back to main menu
   }
   
   if (doReset)
   {
      SetResetAssert; 
      Serial.println("Resetting C64"); 
      Serial.flush();
      delay(50); 
      while(ReadButton==0); //avoid self reset detection
      doReset=false;
      BtnPressed = false;
      SetResetDeassert;
   }
  
   if (Serial.available()) ServiceSerial();
   //myusbHost.Task();
#ifdef nfcScanner
   nfcCheck();
#endif
   
   //handler specific polling items:
   if (IOHandler[CurrentIOHandler]->PollingHndlr != NULL) IOHandler[CurrentIOHandler]->PollingHndlr();
}

void SetUpMainMenuROM()
{
   SetIRQDeassert;
   SetNMIDeassert;
   SetGameDeassert;
   SetExROMAssert; //emulate 8k cart ROM
   LOROM_Image = NULL; //TeensyROMC64_bin;
   HIROM_Image = NULL;
   LOROM_Mask = HIROM_Mask = 0x1fff;
   //NVIC_ENABLE_IRQ(IRQ_ENET); //make sure ethernet interrupt is back on
   //NVIC_ENABLE_IRQ(IRQ_PIT);
   EmulateVicCycles = false;
   
   FreeCrtChips();
   //FreeSwiftlinkBuffs();
   RedirectEmptyDriveDirMenu();
   //IOHandlerInit(IOH_TeensyROM);   
   doReset = true;
}

void PadSpace(char* StrToPad, uint8_t PadToLength)
{
   while(strlen(StrToPad)<PadToLength) strcat(StrToPad, " ");
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

//void SetEEPDefaults()
//{
//   Serial.println("--> Setting EEPROM to defaults");
//   EEPROM.write(eepAdPwrUpDefaults, 0x90 /* | rpudSIDPauseMask  | rpudNetTimeMask */); //default med js speed, music on, eth time synch off
//   EEPROM.write(eepAdTimezone, -14); //default to pacific time
//   EEPROM.write(eepAdNextIOHndlr, IOH_None); //default to no Special HW
//   //SetEthEEPDefaults();
//   EEPROM.put(eepAdMagicNum, (uint32_t)eepMagicNum); //set this last in case of power down, etc.
//}

void SetNumItems(uint16_t NumItems)
{
   NumItemsFull = NumItems;
   IO1[rRegNumItemsOnPage] = (NumItemsFull > MaxItemsPerPage ? MaxItemsPerPage : NumItemsFull);
   IO1[rwRegPageNumber] = 1;
   IO1[rRegNumPages] = 
      NumItems/MaxItemsPerPage + 
      (NumItems%MaxItemsPerPage!=0 ? 1 : 0) +
      (NumItems==0 ? 1 : 0);
}

void LoadCRT( const char *FileNamePath)
{
   //Launch (emulate) .crt file

   IO1[rWRegCurrMenuWAIT] = rmtSD;
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
   
   //free mem for DriveDirMenu in case current (non-tr) handler is using it all
   //FreeCrtChips();
   //FreeSwiftlinkBuffs();

   // Set up DriveDirMenu to point to file to load
   //    without doing LoadDirectory(&SD/&firstPartition);
   InitDriveDirMenu();
   SetDriveDirMenuNameType(0, ptrFilename);
   NumDrvDirMenuItems = 1;
   MenuSource = DriveDirMenu; 
   SetNumItems(1); //sets # of menu items
   IO1[rwRegCursorItemOnPg] = 0;
   SelItemFullIdx = 0;  //  "Select" item

   //MenuSource[SelItemFullIdx]
   
   HandleExecution();
}