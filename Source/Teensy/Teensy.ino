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
#include <USBHost_t36.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>
#include "TeensyROM.h"
#include "MinimalBoot/Common/Common_Defs.h"
#include "MinimalBoot/Common/Menu_Regs.h"
#include "MinimalBoot/Common/DriveDirLoad.h"
#include "MainMenuItems.h"
#include "MinimalBoot/Common/IOHandlers.h"

uint8_t RAM_Image[RAM_ImageSize]; //Main RAM1 file storage buffer
volatile uint8_t BtnPressed = false; 
volatile uint8_t EmulateVicCycles = false;
uint8_t CurrentIOHandler = IOH_None;
StructMenuItem *DriveDirMenu = NULL;
uint16_t NumDrvDirMenuItems = 0;
char DriveDirPath[MaxPathLength];
uint16_t LOROM_Mask, HIROM_Mask;
bool RemoteLaunched = false; //last app was launched remotely
uint8_t nfcState = nfcStateBitDisabled; //default disabled unless set in eeprom and passes init
Stream *CmdChannel  = &Serial; 

#ifdef FeatTCPListen
   EthernetServer tcpServer(2112); // We will assume control on port 2112
   EthernetClient tcpClient;
#endif

#include "MinimalBoot/Common/ISRs.c"
extern "C" uint32_t set_arm_clock(uint32_t frequency);
extern float tempmonGetTemp(void);

void setup() 
{
   set_arm_clock(816000000);  //slight overclocking, no cooling required
   
   SetLEDOff;  //On from minimal build, off for this setup completion
   Serial.begin(115200); // baud rate doesn't matter here, uses USB layer only (no HW serial bus)
   if (CrashReport) Serial.print(CrashReport);

   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
#ifdef Fab04_FullDMACapable
   SetAddrPortDirIn;
   SetAddrBufsIn;   //default to reading address (normal use)
#endif
#ifdef Fab04_DataBufAlwaysEnabled
   SetDataPortDirIn; //default to input (for C64 Write)
   SetDataBufIn;
   //DataBufEnable; //buffer always enabled via HW
#else
   DataBufDisable; //buffer disabled
   //SetDataBufOut  done in ISR based on R/W signal state
   SetDataPortDirOut; //default to output (for C64 Read)
#endif
   
   SetDMADeassert;
   SetIRQDeassert;
   SetNMIDeassert;
#ifdef Fab04_BiDirReset
   pinMode(BiDir_Reset_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(BiDir_Reset_PIN), isrButton, FALLING );
#endif   
   SetResetAssert; //assert reset until main loop()

#ifdef Fab04_SpecialButton
   pinMode(Special_Btn_In_PIN, INPUT_PULLUP);
   attachInterrupt( digitalPinToInterrupt(Special_Btn_In_PIN), isrSpecial, FALLING );
#else
#ifdef DbgSignalSenseReset
   pinMode(DotClk_Debug_PIN, INPUT_PULLUP);  //use Dot_Clk input as reset sense input
   attachInterrupt( digitalPinToInterrupt(DotClk_Debug_PIN), isrButton, FALLING );
#else
#ifdef DbgFab0_3plus
   pinMode(DotClk_Debug_PIN, OUTPUT);  //p28 is Debug output on fab 0.3+
   SetDebugDeassert;
#else
   pinMode(DotClk_Debug_PIN, INPUT_PULLUP);  //p28 is Dot_Clk input (unused) on fab 0.2x
#endif
#endif
#endif

   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Menu_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Menu_Btn_In_PIN), isrButton, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   NVIC_SET_PRIORITY(IRQ_GPIO6789,16); //set HW ints as high priority, otherwise ethernet int timer causes misses
   
   myusbHost.begin(); // Start USBHost_t36, HUB(s) and USB devices.
   
   uint32_t MagNumRead;
   EEPROM.get(eepAdMagicNum, MagNumRead);
   if (MagNumRead != eepMagicNum) SetEEPDefaults();

   IO1 = (uint8_t*)calloc(IO1Size, sizeof(uint8_t)); //allocate IO1 space and init to 0
   IO1[rwRegStatus]        = rsReady;
   IO1[rWRegCurrMenuWAIT] = rmtTeensy;
   IO1[rRegPresence1]     = 0x55;   
   IO1[rRegPresence2]     = 0xAA;   
   for (uint8_t reg=rRegSIDStrStart; reg<rRegSIDStringTerm; reg++) IO1[reg]=' '; 
   IO1[rRegSIDStringTerm] = 0;   
   IO1[rwRegPwrUpDefaults]= EEPROM.read(eepAdPwrUpDefaults);
   IO1[rwRegPwrUpDefaults2]= EEPROM.read(eepAdPwrUpDefaults2);
   IO1[rwRegTimezone]     = EEPROM.read(eepAdTimezone);  
   for (uint8_t reg=0; reg<NumColorRefs; reg++) IO1[rwRegColorRefStart+reg]=EEPROM.read(eepAdColorRefStart+reg); 
   //IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr); //done each entry into menu
   SetUpMainMenuROM();
   MenuChange(); //set up drive path, menu source/size

   for(uint8_t cnt=0; cnt<IOH_Num_Handlers; cnt++) PadSpace(IOHandler[cnt]->Name, IOHNameLength-1); //done so selection shown on c64 overwrites previous

   for(uint8_t cnt=0; cnt<NumPageLinkBuffs; cnt++) PageLinkBuff[cnt] = NULL; //initialize page link buffer for swiftlink browser mode
   for(uint8_t cnt=0; cnt<NumPrevURLQueues; cnt++) PrevURLQueue[cnt] = NULL; //initialize previous link buffer for swiftlink browser mode
   for(uint8_t cnt=0; cnt<RxQueueNumBlocks; cnt++) RxQueue[cnt] = NULL;      //initialize RxQueue for swiftlink

   StrSIDInfo = (char*)calloc(StrSIDInfoSize, sizeof(char)); //SID header info storage
   LatestSIDLoaded = (char*)malloc(MaxPathLength); //Last loaded Source/SID path/filename
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));

   MakeBuildInfo();
   Serial.printf("\n%s\nTeensyROM %s is on-line\n", SerialStringBuf, strVersionNumber);
#ifdef Fab04_Features
   Serial.printf("  for Fab 0.4 PCB\n");
#else
   Serial.printf("  for Fab 0.2/0.3 PCB\n");
#endif
   Printf_dbg("Debug messages enabled!\n");
   Printf_dbg_sw("Swiftlink debug messages enabled!\n");

   if (IO1[rwRegPwrUpDefaults2] & rpud2NFCEnabled) nfcInit(); //connect to nfc scanner

   if (IO1[rwRegPwrUpDefaults2] & rpud2TRContEnabled) //connect to TR Control device
   {  //takes 200mS typical, 5 seconds if usb serial device not present!
      USBHostSerial.begin(115200, USBHOST_SERIAL_8N1); // 115200 460800 2000000
      Serial.println("USB Host Control Enabled");
      //USBHostSerial.printf("USB Host Serial Control Ready\n");
   }
   
   
   switch (EEPROM.read(eepAdMinBootInd))
   {
      case MinBootInd_SkipMin: //normal first power up
         if (ReadButton!=0) //skip autolaunch checks if button pressed
         {
            uint32_t AutoStartmS = millis();
            if(!CheckLaunchSDAuto()) //if nothing autolaunched from SD autolaunch file
            {
               if (EEPROM.read(eepAdAutolaunchName) && (ReadButton!=0)) //If name is non zero length & button not pressed
               {
                  EEPRemoteLaunch(eepAdAutolaunchName);
               }
            }
            Printf_dbg("Autolaunch checks: %lumS\n", millis()-AutoStartmS);
         }
         break;
         
      case MinBootInd_LaunchFull: // Launch command received in minimal, launch it from full
         EEPROM.write(eepAdMinBootInd, MinBootInd_SkipMin);
         EEPRemoteLaunch(eepAdCrtBootName);
         break;
         
      default:  //ignore anything else (most likely MinBootInd_FromMin), set back to default for next time 
         EEPROM.write(eepAdMinBootInd, MinBootInd_SkipMin);
         break;
   }
   
   SetLEDOn;  //done last as indicator of init completion
   
} 
     
void loop()
{
   if (BtnPressed)
   {
      CmdChannel->print("Button detected\n"); 
      SetLEDOn;
      BtnPressed=false;
      IO1[rwRegIRQ_CMD] = ricmdNone; //just to be sure, should already be 0/none
      if (RemoteLaunched)
      {
         IO1[rWRegCurrMenuWAIT] = rmtTeensy;
         MenuChange();
         RemoteLaunched = false;
         Printf_dbg("Remote recovery\n"); 
      }   
      if (IO1[rwRegPwrUpDefaults2] & rpud2NFCEnabled) nfcInit(); //connect to nfc scanner
      SetUpMainMenuROM(); //back to main menu, also sets doReset
   }
   
   if (doReset)
   {
#ifdef DbgSignalSenseReset
      detachInterrupt( digitalPinToInterrupt(DotClk_Debug_PIN) );
#endif
      SetResetAssert; 
      CmdChannel->println("Resetting C64"); 
      CmdChannel->flush();
      delay(50); 
      uint32_t NextInterval = 10000, beginWait = millis();
      bool LEDState = true, DefEEPReboot = false;
      while(ReadButton==0)
      {  //avoid self reset detection, check for long press
         if(millis()-beginWait > NextInterval)
         {
            DefEEPReboot = true;
            NextInterval += 150;
            LEDState = !LEDState;
            if (LEDState) SetLEDOn;
            else SetLEDOff;
         }
      }
      if (DefEEPReboot)
      {
         SetEEPDefaults();
         REBOOT;
      }
#ifdef Fab04_BiDirReset
      SetResetInput;
      delay(50);  //debounce
#else      
      SetResetDeassert;
#endif      
      doReset=false;
      BtnPressed = false;

#ifdef DbgSignalSenseReset
      delay(50); 
      attachInterrupt( digitalPinToInterrupt(DotClk_Debug_PIN), isrButton, FALLING );
#endif
   }
  
#ifdef DbgLEDSignalPolling
   static bool LEDLoopState = false;
   if (LEDLoopState = !LEDLoopState) SetLEDOn;
   else SetLEDOff;
#endif

   if (Serial.available()) ServiceSerial(&Serial);
   myusbHost.Task();
   
   if (nfcState == nfcStateEnabled) nfcCheck();
   
   if (IO1[rwRegPwrUpDefaults2] & rpud2TRContEnabled)
      if (USBHostSerial.available()) ServiceSerial(&USBHostSerial);

#ifdef FeatTCPListen
   if (NetListenEnable)
   {
      if(!tcpClient)
      {
        tcpClient = tcpServer.available();

        if(tcpClient)
        {
          IPAddress ip = tcpClient.remoteIP();
          Printf_dbg("New TCP Client, IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        }
      }
      if (tcpClient) ServiceTCP(tcpClient);
   }

#endif
 
   //handler specific polling items:
   if (IOHandler[CurrentIOHandler]->PollingHndlr != NULL) IOHandler[CurrentIOHandler]->PollingHndlr();
}

void SetUpMainMenuROM()
{
   SetDMADeassert;
   SetIRQDeassert;
   SetNMIDeassert;
   SetGameDeassert;
   SetExROMAssert; //emulate 8k cart ROM
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = NULL;
   LOROM_Mask = HIROM_Mask = 0x1fff;
   NVIC_ENABLE_IRQ(IRQ_ENET); //make sure ethernet interrupt is back on
   NVIC_ENABLE_IRQ(IRQ_PIT);
   EmulateVicCycles = false;
   nfcState &= ~nfcStateBitPaused; //clear paused bit in case paused by time critical function
   
   FreeCrtChips();
   FreeSwiftlinkBuffs();
   RedirectEmptyDriveDirMenu();
   free((void*)MIDIRxBuf); MIDIRxBuf=NULL;
   free(TgetQueue); TgetQueue=NULL;
   free(LSFileName); LSFileName=NULL;

   IOHandlerInit(IOH_TeensyROM);   
   doReset = true;
}

void PadSpace(char* StrToPad, uint8_t PadToLength)
{
   while(strlen(StrToPad)<PadToLength) strcat(StrToPad, " ");
}

void EEPwriteNBuf(uint16_t addr, const uint8_t* buf, uint16_t len)
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

void SetEEPDefaults()
{
   CmdChannel->println("--> Setting EEPROM to defaults");
   EEPROM.write(eepAdPwrUpDefaults, 0x90); //default: music on, eth time synch off, hide extensions, 12 hour clock, med js speed (9/15), see RegPowerUpDefaultMasks
   EEPROM.write(eepAdPwrUpDefaults2, 0x00); //default: NFC & Serial TRCont off, see see bit mask defs RegPowerUpDefaultMasks2
   EEPROM.write(eepAdTimezone, -14); //default to pacific time
   EEPROM.write(eepAdNextIOHndlr, IOH_None); //default to no Special HW
   SetEthEEPDefaults();
   EEPROM.write(eepAdDefaultSID, DefSIDSource);  
   EEPwriteStr(eepAdDefaultSID+1, DefSIDPath);
   EEPwriteStr(eepAdDefaultSID+strlen(DefSIDPath)+2, DefSIDName);  
   EEPROM.write(eepAdMinBootInd, MinBootInd_SkipMin);
   EEPROM.write(eepAdAutolaunchName, 0); //disable auto Launch
   //default color scheme:
   EEPROM.write(eepAdColorRefStart+EscBackgndColor , PokeBlack  ); 
   EEPROM.write(eepAdColorRefStart+EscBorderColor  , PokePurple ); 
   EEPROM.write(eepAdColorRefStart+EscTRBannerColor, PokePurple ); 
   EEPROM.write(eepAdColorRefStart+EscTimeColor    , PokeOrange ); 
   EEPROM.write(eepAdColorRefStart+EscOptionColor  , PokeYellow ); 
   EEPROM.write(eepAdColorRefStart+EscSourcesColor , PokeLtBlue ); 
   EEPROM.write(eepAdColorRefStart+EscNameColor    , PokeLtGreen); 
   //hot key defaults:
   EEPwriteStr(eepAdHotKeyPaths+0*MaxPathLength, "TR:/MIDI + ASID/Cynthcart 2.0.1       +Datel MIDI"); 
   EEPwriteStr(eepAdHotKeyPaths+1*MaxPathLength, "TR:/MIDI + ASID/Station64 2.6      +Passport MIDI"); 
   EEPwriteStr(eepAdHotKeyPaths+2*MaxPathLength, "TR:/Utilities/CCGMS 2021 Term       +SwiftLink "); 
   EEPwriteStr(eepAdHotKeyPaths+3*MaxPathLength, "TR:/MIDI + ASID/TeensyROM ASID Player    +TR ASID"); 
   EEPwriteStr(eepAdHotKeyPaths+4*MaxPathLength, "TR:/Games/Jupiter Lander"); 
   
   
   EEPROM.put(eepAdMagicNum, (uint32_t)eepMagicNum); //set this last in case of power down, etc.
}

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

bool SDFullInit()
{

   // begin() takes 3 seconds for fail, 20-200mS for pass, 2 seconds for unpopulated
   
   uint8_t Count = 2; //Max number of begin attempts
   uint32_t Startms = millis();
   
   Printf_dbg("[=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=]\n");  
   Printf_dbg("Start mediaPresent %d\n", SD.mediaPresent()); //This indicates zero regardless of actual prior to begin()
    
   while (!SD.begin(BUILTIN_SDCARD)) 
   {
      Count--;
      Printf_dbg("SD Init fail, %d tries left. Time: %lu mS\n", Count, millis()-Startms);
      if (SD.mediaPresent() == 0)
      {
         Printf_dbg("SD Not Present, Fail!  took %lu mS\n", millis()-Startms);
         return false;
      }
      if (Count == 0)
      {
         Printf_dbg("Out of tries, Fail!  took %lu mS\n", millis()-Startms);
         return false;
      }
   }
   
   Printf_dbg("SD Init OK, took %lu mS, mediaPresent %d\n", millis()-Startms, SD.mediaPresent());
   return true;
}

bool USBFileSystemWait()
{
   //wait for USB file system to init in case it's needed by startup SID or auto-launched
   // ~7mS for direct connect, ~1000mS for connect via hub, 1500mS timeout (drive not found)
   uint32_t StartMillis = millis();

   Printf_dbg("[=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=][=]\n");  
   
   while (!firstPartition && millis()-StartMillis < 1500) myusbHost.Task();
   
   if(firstPartition) 
   {
      Printf_dbg("%dmS to init USB drive\n", millis()-StartMillis); 
      return true;
   }

   Printf_dbg("USB drive not found!\n");  
   return false;
}

void SetRandomSeed()
{
   //Set the Random Seed once the first time it is called (set to cycle count)
   static bool SetOnce = false;
   
   if (SetOnce) return;
   
   SetOnce = true;
   Printf_dbg("Setting Random Seed\n");
   randomSeed(ARM_DWT_CYCCNT);
}

bool CheckLaunchSDAuto()
{         
   //returns true only if file launched
   
   // SD not present: 0mS
   // autolaunch.txt Not Found: 9-10mS
   // Launch file name too short: 10mS
   // Launch file attempted, not found: 39mS
   // Autolaunch file found, launch set-up: 10mS
   
   // _SD_DAT3 = pin 46
   pinMode(46, INPUT_PULLDOWN);
   if (digitalReadFast(46))
   {  //SD Presence detected, do full init and check for auotlaunch file    
      Printf_dbg("SD Presence detected\n");
      if (SDFullInit())
      {
         File AutoLaunchFile = SD.open("autolaunch.txt", FILE_READ);
         if (!AutoLaunchFile) 
         {
            Printf_dbg("autolaunch.txt Not Found\n");
            return false;
         }
         
         char AutoFileName[MaxPathLength];
         uint16_t CharNum = 0;
         char NextChar = 1;
         
         while (NextChar)
         {
            if(AutoLaunchFile.available()) NextChar = AutoLaunchFile.read();
            else NextChar = 0;
            
            if (NextChar=='\r' || NextChar=='\n' || CharNum == MaxPathLength-1) NextChar = 0;           
            
            AutoFileName[CharNum++]=NextChar;
         }
         AutoLaunchFile.close();
         
         Printf_dbg("SD First line: %d chars \"%s\"\n", CharNum, AutoFileName); 
         
         if (CharNum<6) 
         {
            Printf_dbg("Filename too short\n");
            return false;
         }

         char * ptrAutoFileName = AutoFileName; //pointer to move past SD/USB/TR:
         RegMenuTypes SourceID = RegMenuTypeFromFileName(&ptrAutoFileName);
         
         Printf_dbg("SD Autolaunch %d \"%s\"\n", SourceID, ptrAutoFileName); 
         
         //check if file exists????????????
         
         RemoteLaunch(SourceID, ptrAutoFileName, true); //do CRT directly 
         return true;
      }  //SD init
      else Printf_dbg("SDFullInit fail\n");
   }  //SD presence
   else Printf_dbg("No SD detected\n");

   return false;
}
