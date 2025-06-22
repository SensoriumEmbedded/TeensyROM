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


//Network, 6551 ACIA interface emulation
//with AT Command sub-system and Internet Browser


void IO1Hndlr_SwiftLink(uint8_t Address, bool R_Wn);  
void PollingHndlr_SwiftLink();                           
void InitHndlr_SwiftLink();                           
void CycleHndlr_SwiftLink();                           

stcIOHandlers IOHndlr_SwiftLink =
{
  "SwiftLink/Modem",        //Name of handler
  &InitHndlr_SwiftLink,     //Called once at handler startup
  &IO1Hndlr_SwiftLink,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_SwiftLink,  //Polled in main routine
  &CycleHndlr_SwiftLink,    //called at the end of EVERY c64 cycle
};


#define NumPageLinkBuffs    99
#define NumPrevURLQueues     8
#define MaxURLHostSize     100
#define MaxURLPathSize     300
#define MaxTagSize         300
#define TxMsgMaxSize       128    //DL path saved in EEPROM is tied to this
#define eepBMTitleSize      75    //max chars in bookmark title
#define eepBMURLSize       225    //Max Chars in bookmark URL path
#define eepNumBookmarks      9    //Num Bookmarks saved
#define BytesPerDot        (25*1024) //dot every 25k when downloading
#define RxQueueNumBlocks    40 
#define RxQueueBlockSize   (1024*8) // 40*8k=320k
#define RxQueueSize        (RxQueueNumBlocks*RxQueueBlockSize) 
#define NMITimeoutuS       300    //if Rx data not read within this time, deassert NMI anyway
#define Drive_USB            1
#define Drive_SD             2

// 6551 ACIA interface emulation
//register locations (IO1, DExx)
#define IORegSwiftData     0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus   0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand  0x02   // Swift Emulation Command Reg
#define IORegSwiftControl  0x03   // Swift Emulation Control Reg

//status reg flags
#define SwiftStatusIRQ     0x80   // high if ACIA caused interrupt;
#define SwiftStatusDCD     0x40   // reflects state of DCD line
#define SwiftStatusDSR     0x20   // reflects state of DSR line
#define SwiftStatusTxEmpty 0x10   // high if xmit-data register is empty
#define SwiftStatusRxFull  0x08   // high if receive-data register full
#define SwiftStatusErrOver 0x04   // high if overrun error
#define SwiftStatusErrFram 0x02   // high if framing error
#define SwiftStatusErrPar  0x01   // high if parity error
#define SwiftStatusDefault (SwiftStatusTxEmpty|SwiftStatusDCD)   // default/power-up/reset value

//command reg flags
#define SwiftCmndRxIRQEn   0x02   // low if Rx IRQ enabled
#define SwiftCmndDefault   0xE0   // Default command reg state

//Control Reg baud rate settings
enum enBaudRates
{
   Baud_300 = 5,
   Baud_600    ,
   Baud_1200   ,
   Baud_2400   ,
   Baud_3600   ,
   Baud_4800   ,
   Baud_7200   ,
   Baud_9600   ,
   Baud_14400  ,
   Baud_19200  ,
   Baud_38400  ,
};

//PETSCII Colors/Special Symbols
#define PETSCIIpurple      0x9c
#define PETSCIIwhite       0x05
#define PETSCIIlightBlue   0x9a
#define PETSCIIyellow      0x9e
#define PETSCIIbrown       0x95
#define PETSCIIpink        0x96
#define PETSCIIlightGreen  0x99
#define PETSCIIgreen       0x1e
#define PETSCIIlightGrey   0x9b
#define PETSCIIdarkGrey    0x97
#define PETSCIIgrey        0x98
#define PETSCIIorange      0x81

#define PETSCIIreturn      0x0d
#define PETSCIIrvsOn       0x12
#define PETSCIIrvsOff      0x92
#define PETSCIIclearScreen 0x93
#define PETSCIIcursorUp    0x91
#define PETSCIIhorizBar    0x60
#define PETSCIIspace       0x20
#define PETSCIIhiLoChrSet  0x0e
#define PETSCIIupGfxChrSet 0x8e

#define RxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+RxQueueSize-RxQueueTail))

struct stcURLParse
{
   char host[MaxURLHostSize];
   uint16_t port;
   char path[MaxURLPathSize];
   char postpath[MaxURLPathSize];
};

extern volatile uint32_t CycleCountdown;
extern volatile uint8_t BtnPressed;
extern void EEPreadNBuf(uint16_t addr, uint8_t* buf, uint16_t len);
extern void EEPwriteNBuf(uint16_t addr, const uint8_t* buf, uint16_t len);
extern void EEPwriteStr(uint16_t addr, const char* buf);
extern void EEPreadStr(uint16_t addr, char* buf);
extern void FreeDriveDirMenu();
extern void SetRandomSeed();
extern uint32_t RAM2BytesFree();

uint8_t* RxQueue[RxQueueNumBlocks];  //circular queue to pipe data to the c64, divided into blocks for better malloc
char* TxMsg = NULL;  //to hold messages (AT/browser commands) when off line
char* PageLinkBuff[NumPageLinkBuffs]; //hold links from tags for user selection in browser
stcURLParse* PrevURLQueue[NumPrevURLQueues]; //For browse previous
char CurrPageTitle[eepBMTitleSize]; //keep current page title, could move to RAM2

uint8_t  PrevURLQueueNum;   //current/latest in the link history queue
uint8_t  UsedPageLinkBuffs;   //how many PageLinkBuff elements have been Used
uint32_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool Verbose, EchoOn, ConnectedToHost, BrowserMode, PagePaused, PrintingHyperlink;
uint32_t PageCharsReceived;
uint32_t NMIassertMicros;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
uint8_t PlusCount;
uint32_t C64CycBetweenRx, LastTxMillis = millis();


// Browser mode: Buffer saved in ASCII from host, converted before sending out
//               Uses Send...Immediate  commands for direct output
// AT/regular:   Buffer saved in (usually) PETSCII from host
//               Uses Add...ToRxQueue for direct output

void ParseHTMLTag();
void ParseEntityReference();
void SetEthEEPDefaults();
void SendBrowserCommandsImmediate();
void UnPausePage();
#include "Swift_RxQueue.c"
#include "Swift_ATcommands.c"
#include "Swift_Browser.c"

void SetBaud(enBaudRates BaudNum)
{  //called from Phi IRQ, be quick
   uint16_t ActualBaud[] =
   { //order matches enBaudRates-Baud_300
        300, // Baud_300  (5)
        600, // Baud_600  
       1200, // Baud_1200 
       2400, // Baud_2400 
       3600, // Baud_3600 
       4800, // Baud_4800 
       7200, // Baud_7200 
       9600, // Baud_9600 
      14400, // Baud_14400
      19200, // Baud_19200
      38400, // Baud_38400 (15)
   };
   
   //stops NMI from re-asserting too quickly.
   //basing cycle count off of PAL, NTSC will be ~3.8% faster
   C64CycBetweenRx = 985250*8/ActualBaud[BaudNum-Baud_300];
                           // 2300 is lowest without CCGMS misses (chars missed in large buffs when lower)
                           // 3410 for 2400 baud Rx
                           // 6819 for 1200 baud Rx
   Printf_dbg_sw("baud: %d, %d Cycles\n", ActualBaud[BaudNum-Baud_300], C64CycBetweenRx); 
}

void FreeSwiftlinkBuffs()
{
   for(uint8_t cnt=0; cnt<NumPageLinkBuffs; cnt++) {free(PageLinkBuff[cnt]); PageLinkBuff[cnt]=NULL;}
   for(uint8_t cnt=0; cnt<NumPrevURLQueues; cnt++) {free(PrevURLQueue[cnt]); PrevURLQueue[cnt]=NULL;}
   for(uint8_t cnt=0; cnt<RxQueueNumBlocks; cnt++) {free(RxQueue[cnt]); RxQueue[cnt]=NULL;}
   free(TxMsg); TxMsg = NULL;   
}

FLASHMEM bool EthernetInit()
{
   uint32_t beginWait = millis();
   uint8_t  mac[6];
   bool retval = true;
   Serial.print("\nEthernet init ");
   
   EEPreadNBuf(eepAdMyMAC, mac, 6);

   if (EEPROM.read(eepAdDHCPEnabled))
   {
      Serial.print("via DHCP... ");

      uint16_t DHCPTimeout, DHCPRespTO;
      EEPROM.get(eepAdDHCPTimeout, DHCPTimeout);
      EEPROM.get(eepAdDHCPRespTO, DHCPRespTO);
      if (Ethernet.begin(mac, DHCPTimeout, DHCPRespTO) == 0)
      {
         Serial.println("*Failed!*");
         // Check for Ethernet hardware present
         if (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("Ethernet HW was not found.");
         else if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");   
         retval = false;
      }
      else
      {
         Serial.println("passed.");
      }
   }
   else
   {
      Serial.println("using Static");
      uint32_t ip, dns, gateway, subnetmask;
      EEPROM.get(eepAdMyIP, ip);
      EEPROM.get(eepAdDNSIP, dns);
      EEPROM.get(eepAdGtwyIP, gateway);
      EEPROM.get(eepAdMaskIP, subnetmask);
      Ethernet.begin(mac, ip, dns, gateway, subnetmask);
   }
   
   Serial.printf("Took %d mS\nIP: ", (millis() - beginWait));
   Serial.println(Ethernet.localIP());
   return retval;
}
   
FLASHMEM void SetEthEEPDefaults()
{
   EEPROM.write(eepAdDHCPEnabled, 1); //DHCP enabled
   uint8_t buf[6]={0xBE, 0x0C, 0x64, 0xC0, 0xFF, 0xEE};
   EEPwriteNBuf(eepAdMyMAC, buf, 6);
   EEPROM.put(eepAdMyIP       , (uint32_t)IPAddress(192,168,1,10));
   EEPROM.put(eepAdDNSIP      , (uint32_t)IPAddress(192,168,1,1));
   EEPROM.put(eepAdGtwyIP     , (uint32_t)IPAddress(192,168,1,1));
   EEPROM.put(eepAdMaskIP     , (uint32_t)IPAddress(255,255,255,0));
   EEPROM.put(eepAdDHCPTimeout, (uint16_t)9000);
   EEPROM.put(eepAdDHCPRespTO , (uint16_t)4000);  
   EEPROM.write(eepAdDLPathSD_USB, Drive_SD); //default to root of SD card
   EEPwriteStr(eepAdDLPath, "/"); 

   const char * DefBookmarks[eepNumBookmarks][2] =
   {
      "TinyWeb64 @ Sensorium",              "http://sensoriumembedded.com/tinyweb64/",
      "Digitalman TeensyROM Demo",          "http://digitalman.azurewebsites.net/",
      "68k.news Headlines from the Future", "http://68k.news/",
      "CNN Lite (filtered)",                "http://www.frogfind.com/read.php?a=http://lite.cnn.com/",
      "CBC Lite News (filtered)",           "http://www.frogfind.com/read.php?a=http://www.cbc.ca/lite/news",
      "textfiles.com",                      "http://textfiles.com/directory.html",
      "Hyperlinked Text (filtered)",        "http://www.frogfind.com/read.php?a=http://sjmulder.nl/en/textonly.html",
      "legiblenews.com (filtered)",         "http://www.frogfind.com/read.php?a=http://legiblenews.com/",
      "text-only news sites (filtered)",    "http://www.frogfind.com/read.php?a=http://greycoder.com/a-list-of-text-only-new-sites",
   };
   
   for (uint8_t BMNum=0; BMNum<eepNumBookmarks; BMNum++)
   {
      EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize),DefBookmarks[BMNum][0]);
      EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize,DefBookmarks[BMNum][1]);
   }
}


//_____________________________________Handlers_____________________________________________________

FLASHMEM void InitHndlr_SwiftLink()
{
   //EthernetInit();
   SwiftRegStatus = SwiftStatusDefault;
   SwiftRegCommand = SwiftCmndDefault;
   SwiftRegControl = 0;
   CycleCountdown=0;
   PlusCount=0;
   PrevURLQueueNum = 0;
   NMIassertMicros = 0;
   PlusCount=0;
   ConnectedToHost = false;
   BrowserMode = false;
   EchoOn = true;
   Verbose = true;
   DumpQueueUnPausePage(); // UsedPageLinkBuffs = 0; PageCharsReceived = 0; PagePaused = false; RxQueueHead = RxQueueTail =0
   TxMsgOffset =0;
   PrintingHyperlink = false;
   SetBaud(Baud_2400);
   
   FreeDriveDirMenu(); //clear out drive menu to make space in RAM2
   // RAM2 usage as of 11/7/23:
   //    Queues/link buffs (below): 320k+128+29k+5.5k= ~355k total
   //    RAM2 free w/ ethernet loaded & drive menu cleared: 392k (though will show less if fragmented)
   //    RAM2 free w/ ethernet loaded & drive menu cleared:  396K  44k after buff malloc

   Printf_dbg_sw("RAM2 Bytes Free: %lu (%luK)\n\n", RAM2BytesFree(), RAM2BytesFree()/1024);
 
   for(uint8_t cnt=0; cnt<RxQueueNumBlocks; cnt++) 
   {
      RxQueue[cnt] = (uint8_t*)malloc(RxQueueBlockSize);
      if(RxQueue[cnt] == NULL) Serial.println("OOM RxQ");
   }
   TxMsg = (char*)malloc(TxMsgMaxSize);
   if(TxMsg == NULL) Serial.println("OOM TxQ");
   
   for(uint8_t cnt=0; cnt<NumPageLinkBuffs; cnt++)
   {
      PageLinkBuff[cnt] = (char*)malloc(MaxTagSize);
      if(PageLinkBuff[cnt] == NULL) Serial.println("OOM PageLinkBuff");
   }
   
   Printf_dbg_sw("RAM2 post-SW malloc: %lu (%luK)\n\n", RAM2BytesFree(), RAM2BytesFree()/1024);

   for(uint8_t cnt=0; cnt<NumPrevURLQueues; cnt++) 
   {
      PrevURLQueue[cnt] = (stcURLParse*)malloc(sizeof(stcURLParse));
      if(PrevURLQueue[cnt] == NULL) Serial.println("OOM PrevURLQueue");
      PrevURLQueue[cnt]->path[0] = 0;
      PrevURLQueue[cnt]->postpath[0] = 0;
      PrevURLQueue[cnt]->host[0] = 0;
      PrevURLQueue[cnt]->port = 80;
   }
   strcpy(CurrPageTitle, "None");
   SetRandomSeed();
   nfcState |= nfcStateBitPaused; //Pause NFC for time critical routine
}

void IO1Hndlr_SwiftLink(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      switch(Address)
      {
         case IORegSwiftData:   
            DataPortWriteWaitLog(SwiftRxBuf);
            SetNMIDeassert;
            SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
            break;
         case IORegSwiftStatus:  
            DataPortWriteWaitLog(SwiftRegStatus);
            break;
         case IORegSwiftCommand:  
            DataPortWriteWaitLog(SwiftRegCommand);
            break;
         case IORegSwiftControl:
            DataPortWriteWaitLog(SwiftRegControl);
            break;
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      Data = DataPortWaitRead();
      switch(Address)
      {
         case IORegSwiftData:  
            //add to input buffer
            SwiftTxBuf=Data;
            SwiftRegStatus &= ~SwiftStatusTxEmpty; //Flag full until Tx processed
            break;
         case IORegSwiftStatus:  
            //Write to status reg is a programmed reset
            SwiftRegCommand = SwiftCmndDefault;
            break;
         case IORegSwiftControl:
            SwiftRegControl = Data;
            //Could confirm setting 8N1?
            //Printf_dbg_sw("CtrlW: $%02x\n", SwiftRegControl);            
            Data &= 0x0f;
            if (Data >= Baud_300) SetBaud((enBaudRates)Data);
            break;
         case IORegSwiftCommand:  
            SwiftRegCommand = Data;
            //check for Tx/Rx IRQs enabled?
            //handshake line updates?
            break;
      }
      TraceLogAddValidData(Data);
   }
}

void PollingHndlr_SwiftLink()
{
   //detect connection change
   if (ConnectedToHost != client.connected())
   {
      ConnectedToHost = client.connected();
      if (BrowserMode)
      {
         if (!ConnectedToHost) AddRawStrToRxQueue("<eoftag>");  //add special tag to catch when complete
      }
      else
      {
         if (ConnectedToHost) 
         {  //new connection
            //AddToPETSCIIStrToRxQueueLN("connected to host");
            FlushRxQueue();
            SwiftRegStatus &= ~SwiftStatusDCD; //indicate connected after message complete
         }
         else 
         {  //connection lost
            FlushRxQueue();
            SwiftRegStatus |= SwiftStatusDCD; //indicate disconnected before sending message
            //AddToPETSCIIStrToRxQueueLN("not connected");
            SendATresponse(ATRC_NO_CARRIER); 
         }
      }
   }
   
   //if client data available, add to Rx Queue
   #ifdef DbgMsgs_SW
      if(client.available())
      {
         uint32_t StartMicros = micros();
         uint16_t Cnt = 0;
         //Serial.printf("RxIn %d+", RxQueueUsed);
         while (client.available())
         {
            AddRawCharToRxQueue(client.read());
            Cnt++;
         }
         
         //toggle LED after each new packet received
         static bool LEDstate = true;
         if ( (LEDstate = !LEDstate) ) SetLEDOff;
         else SetLEDOn;
         
         StartMicros = micros() - StartMicros;
         Serial.printf("Rx %d in %duS\n", Cnt, StartMicros);
         if (RxQueueUsed>3000) Serial.printf("Lrg RxQueue add: %d  total: %d\n", Cnt, RxQueueUsed);
      }
   #else
      while (client.available()) AddRawCharToRxQueue(client.read());
   #endif
   
   //if Tx data available, get it from C64
   if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) 
   {
      if (client.connected() && !BrowserMode) //send Tx data to host
      {
         //Printf_dbg_sw("send %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         //client.print((char)SwiftTxBuf);  //send it
         client.write(SwiftTxBuf);
         //client.flush(); //adds ~100mS!
         if(SwiftTxBuf=='+')
         {
            if(millis()-LastTxMillis>1000 || PlusCount!=0) //Must be preceded by at least 1 second of no characters
            {   
               if(++PlusCount>3) PlusCount=0;
            }
         }
         else PlusCount=0;
         
   #ifdef DbgMsgs_SW
         //toggle Debug Signal after every 2 bytes sent
         static uint8_t DBGstate = 0;
         if (++DBGstate==4) DBGstate=0;  // 0-3
         if (DBGstate & 2) {SetDebugDeassert;}
         else {SetDebugAssert;}
   #endif

         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
      }
      else  //off-line/at commands or BrowserMode..................................
      {         
         //Printf_dbg_sw("echo %02x: %c -> ", SwiftTxBuf, SwiftTxBuf);
         
         if(BrowserMode)
         {
            SendPETSCIICharImmediate(SwiftTxBuf); //echo it now, buffer may be paused or filling
            if (SwiftTxBuf & 0x80) SwiftTxBuf &= 0x7f; //Cap to ascii
            else if (SwiftTxBuf & 0x40) SwiftTxBuf |= 0x20;  //lower case to ascii
         }
         else 
         {
            if (EchoOn) AddRawCharToRxQueue(SwiftTxBuf); //echo it at end of buffer
            SwiftTxBufToLcaseASSCII();
         }
         //Printf_dbg_sw("%02x: %c\n", SwiftTxBuf);
         
         if (TxMsgOffset && (SwiftTxBuf==0x08 || SwiftTxBuf==0x14)) TxMsgOffset--; //Backspace in ascii  or  Delete in PETSCII
         else TxMsg[TxMsgOffset++] = SwiftTxBuf; //otherwise store it
         
         if (SwiftTxBuf == 13 || TxMsgOffset == TxMsgMaxSize) //return hit or max size
         {
            SwiftRegStatus |= SwiftStatusTxEmpty; //clear the flag after last SwiftTxBuf access
            TxMsg[TxMsgOffset-1] = 0; //terminate it
            Printf_dbg_sw("TxMsg: %s\n", TxMsg);
            if(BrowserMode) 
            {
               ProcessBrowserCommand();
               SendPETSCIICharImmediate(PETSCIIwhite); 
            }
            else
            {
               ATRespCode Resp = ProcessATCommand();
               if (!BrowserMode) SendATresponse(Resp);
            }
            TxMsgOffset = 0;
         }
         else SwiftRegStatus |= SwiftStatusTxEmpty; //clear the flag after last SwiftTxBuf access
      }
      LastTxMillis = millis();
   }
   
   if(PlusCount==3 && millis()-LastTxMillis>1000) //Must be followed by one second of no characters
   {
      PlusCount=0;
      ClearClientStop();

      FlushRxQueue();
      SwiftRegStatus |= SwiftStatusDCD; //indicate disconnected before sending message
      AddToPETSCIIStrToRxQueueLN("*aborted by user*");
   }

   if (PageCharsReceived < 880 || PrintingHyperlink) CheckSendRxQueue();
   else
   {
      if (!PagePaused)
      {
         PagePaused = true;
         SendCommandSummaryImmediate(true);
      }
   }
}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}


