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


#define NumPageLinkBuffs   99
#define NumPrevURLQueues   8
#define MaxURLHostSize     100
#define MaxURLPathSize     200
#define MaxTagSize         (MaxURLHostSize+MaxURLPathSize)
#define TxMsgMaxSize       128
#define RxQueueSize        (1024*320) 
#define C64CycBetweenRx    2300   //stops NMI from re-asserting too quickly. chars missed in large buffs when lower
#define NMITimeoutnS       300   //if Rx data not read within this time, deassert NMI anyway

// 6551 ACIA interface emulation
//register locations (IO1, DExx)
#define IORegSwiftData     0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus   0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand  0x02   // Swift Emulation Command Reg
#define IORegSwiftControl  0x03   // Swift Emulation Control Reg

//status reg flags
#define SwiftStatusIRQ     0x80   // high if ACIA caused interrupt;
#define SwiftStatusDSR     0x40   // reflects state of DSR line
#define SwiftStatusDCD     0x20   // reflects state of DCD line
#define SwiftStatusTxEmpty 0x10   // high if xmit-data register is empty
#define SwiftStatusRxFull  0x08   // high if receive-data register full
#define SwiftStatusErrOver 0x04   // high if overrun error
#define SwiftStatusErrFram 0x02   // high if framing error
#define SwiftStatusErrPar  0x01   // high if parity error

//command reg flags
#define SwiftCmndRxIRQEn   0x02   // low if Rx IRQ enabled
#define SwiftCmndDefault   0xE0   // Default command reg state

//PETSCII Colors/Special Symbols
#define PETSCIIpurple      0x9c
#define PETSCIIwhite       0x05
#define PETSCIIlightBlue   0x9a
#define PETSCIIyellow      0x9e
#define PETSCIIpink        0x96
#define PETSCIIlightGreen  0x99
#define PETSCIIdarkGrey    0x97
#define PETSCIIgrey        0x98

#define PETSCIIreturn      0x0d
#define PETSCIIrvsOn       0x12
#define PETSCIIrvsOff      0x92
#define PETSCIIclearScreen 0x93
#define PETSCIIcursorUp    0x91
#define PETSCIIhorizBar    0x60
#define PETSCIIspace       0x20

#define RxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+RxQueueSize-RxQueueTail))

struct stcURLParse
{
   char host[MaxURLHostSize];
   uint16_t port;
   char path[MaxURLPathSize];
};

extern volatile uint32_t CycleCountdown;
extern void EEPreadBuf(uint16_t addr, uint8_t* buf, uint8_t len);
extern void EEPwriteBuf(uint16_t addr, const uint8_t* buf, uint8_t len);

uint8_t* RxQueue = NULL;  //circular queue to pipe data to the c64 
char* TxMsg = NULL;  //to hold messages (AT/browser commands) when off line
char* PageLinkBuff[NumPageLinkBuffs]; //hold links from tags for user selection in browser
stcURLParse* PrevURLQueue[NumPrevURLQueues]; //For browse previous

uint8_t  PrevURLQueueNum;   //where we are in the link history queue
uint8_t  UsedPageLinkBuffs;   //how many PageLinkBuff elements have been Used
uint32_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost, BrowserMode, PagePaused, PrintingHyperlink;
uint32_t PageCharsReceived;
uint32_t NMIassertMicros;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
uint8_t PlusCount;
uint32_t LastTxMillis = millis();


// Browser mode: Buffer saved in ASCII from host, converted before sending out
//               Uses Send...Immediate  commands for direct output
// AT/regular:   Buffer saved in (usually) PETSCII from host
//               Uses Add...ToRxQueue for direct output

void ParseHTMLTag();
void SetEthEEPDefaults();
#include "Swift_RxQueue.c"
#include "Swift_ATcommands.c"

FLASHMEM bool EthernetInit()
{
   uint32_t beginWait = millis();
   uint8_t  mac[6];
   bool retval = true;
   Serial.print("\nEthernet init ");
   
   EEPreadBuf(eepAdMyMAC, mac, 6);

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
   EEPwriteBuf(eepAdMyMAC, buf, 6);
   EEPROM.put(eepAdMyIP       , (uint32_t)IPAddress(192,168,1,10));
   EEPROM.put(eepAdDNSIP      , (uint32_t)IPAddress(192,168,1,1));
   EEPROM.put(eepAdGtwyIP     , (uint32_t)IPAddress(192,168,1,1));
   EEPROM.put(eepAdMaskIP     , (uint32_t)IPAddress(255,255,255,0));
   EEPROM.put(eepAdDHCPTimeout, (uint16_t)9000);
   EEPROM.put(eepAdDHCPRespTO , (uint16_t)4000);   
}
   
void ParseHTMLTag()
{ //retrieve and interpret HTML Tag
  //https://www.w3schools.com/tags/
  
   char TagBuf[MaxTagSize];
   uint16_t BufCnt = 0;
   
   //pull tag from queue until >, queue empty, or buff max size
   while (RxQueueUsed > 0)
   {
      TagBuf[BufCnt] = PullFromRxQueue();
      if(TagBuf[BufCnt] == '>') break;
      if(++BufCnt == MaxTagSize-1) break;
   }
   TagBuf[BufCnt] = 0;  //terminate it

   //check for known tags and do formatting, etc
   if(strcmp(TagBuf, "br")==0 || strcmp(TagBuf, "p")==0 || strcmp(TagBuf, "/p")==0) 
   {
      SendPETSCIICharImmediate(PETSCIIreturn);
      PageCharsReceived += 40-(PageCharsReceived % 40);
   }
   else if(strcmp(TagBuf, "/b")==0) SendPETSCIICharImmediate(PETSCIIwhite); //unbold
   else if(strcmp(TagBuf, "b")==0) //bold, but don't change hyperlink color
   {
      if(!PrintingHyperlink) SendPETSCIICharImmediate(PETSCIIyellow);
   } 
   else if(strcmp(TagBuf, "eoftag")==0) AddBrowserCommandsToRxQueue();  // special tag to signal complete
   else if(strcmp(TagBuf, "li")==0) //list item
   {
      SendPETSCIICharImmediate(PETSCIIdarkGrey); 
      SendASCIIStrImmediate("\r * ");
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PageCharsReceived += 40-(PageCharsReceived % 40)+3;
   }
   else if(strncmp(TagBuf, "a href=", 7)==0) 
   { //start of hyperlink text, save hyperlink
      //Printf_dbg("LinkTag: %s\n", TagBuf);
      SendPETSCIICharImmediate(PETSCIIpurple); 
      SendPETSCIICharImmediate(PETSCIIrvsOn); 
      if (UsedPageLinkBuffs < NumPageLinkBuffs)
      {
         for(uint16_t CharNum = 8; CharNum < strlen(TagBuf); CharNum++) //skip a href="
         { //terminate at first 
            if(TagBuf[CharNum]==' '  || 
               TagBuf[CharNum]=='\'' ||
               TagBuf[CharNum]=='\"' ||
               TagBuf[CharNum]=='#') TagBuf[CharNum] = 0; //terminate at space, #, ', or "
         }
         strcpy(PageLinkBuff[UsedPageLinkBuffs], TagBuf+8); // remove quote from beginning
         
         Printf_dbg("Link #%d: %s\n", UsedPageLinkBuffs+1, PageLinkBuff[UsedPageLinkBuffs]);
         UsedPageLinkBuffs++;
         
         if (UsedPageLinkBuffs > 9) SendPETSCIICharImmediate('0' + UsedPageLinkBuffs/10);
         SendPETSCIICharImmediate('0' + (UsedPageLinkBuffs%10));
      }
      else SendPETSCIICharImmediate('*');
      
      SendPETSCIICharImmediate(PETSCIIlightBlue); 
      SendPETSCIICharImmediate(PETSCIIrvsOff);
      PageCharsReceived++;
      PrintingHyperlink = true;
   }
   else if(strcmp(TagBuf, "/a")==0)
   { //end of hyperlink text
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PrintingHyperlink = false;
   }
   else if(strcmp(TagBuf, "html")==0)
   { //Start of HTML
      SendPETSCIICharImmediate(PETSCIIclearScreen); // comment these two lines out to 
      UsedPageLinkBuffs = 0;                        //  scroll header instead of clear
      PageCharsReceived = 0;
      PagePaused = false;
   }
   //else Printf_dbg("Unk Tag: <%s>\n", TagBuf);  //There can be a lot of these...
   
} 

void ParseURL(const char * URL, stcURLParse &URLParse)
{
   //https://en.wikipedia.org/wiki/URL
   //https://www.w3.org/Library/src/HTParse.html
   //https://stackoverflow.com/questions/726122/best-ways-of-parsing-a-url-using-c
   //https://gist.github.com/j3j5/8336b0224167636bed462950400ff2df       Test URLs
   //the format of a URI is as follows: "ACCESS :// HOST : PORT / PATH # ANCHOR"
   
   URLParse.host[0] = 0;
   URLParse.port = 80;
   URLParse.path[0] = 0;
   
   //Find/skip access ID
   if(strstr(URL, "http://") != URL && strstr(URL, "https://") != URL) //no access ID, relative path only
   {
      strcat(URLParse.path, URL);
   }
   else
   {
      const char * ptrServerName = strstr(URL, "://")+3; //move past the access ID
      char * ptrPort = strstr(ptrServerName, ":");  //find port identifier
      char * ptrPath = strstr(ptrServerName, "/");  //find path identifier
      
      //need to check for userid? http://userid@example.com:8080/
      
      //finalize server name and update port, if present
      if (ptrPort != NULL) //there's a port ID
      {
         URLParse.port = atoi(ptrPort+1);  //skip the ":"
         strncpy(URLParse.host, ptrServerName, ptrPort-ptrServerName);
         URLParse.host[ptrPort-ptrServerName]=0; //terminate it
      }
      else if (ptrPath != NULL)  //there's a path
      {
         strncpy(URLParse.host, ptrServerName, ptrPath-ptrServerName);
         URLParse.host[ptrPath-ptrServerName]=0; //terminate it
      }
      else strcpy(URLParse.host, ptrServerName);  //no port or path
   
      //copy path, if present
      if (ptrPath != NULL) strcpy(URLParse.path, ptrPath);
      else strcpy(URLParse.path, "/");
   }

   Printf_dbg("\nOrig  = \"%s\"\n", URL);
   Printf_dbg(" serv = \"%s\"\n", URLParse.host);
   Printf_dbg(" port = %d\n", URLParse.port);
   Printf_dbg(" path = \"%s\"\n", URLParse.path);
} 

bool ReadClientLine(char* linebuf, uint16_t MaxLen)
{
   uint16_t charcount = 0;
   
   while (client.connected()) 
   {
      while (client.available()) 
      {
         uint8_t c = client.read();
         linebuf[charcount++] = c;
         if(charcount == MaxLen) return false;
         if (c=='\n')
         {
            linebuf[charcount] = 0; //terminate it
            return true;
         }
      }
   }
   return false;
}

bool WebConnect(const stcURLParse &DestURL)
{
   //   case wc_Filter:   strcpy(UpdWebPage, "/read.php?a=http://");
   
   memcpy(PrevURLQueue[PrevURLQueueNum], &DestURL, sizeof(stcURLParse)); //overwrite previous entry
   if (++PrevURLQueueNum == NumPrevURLQueues) PrevURLQueueNum = 0; //inc/wrap around top
 
   client.stop();
   RxQueueHead = RxQueueTail = 0; //dump the queue
   
   Printf_dbg("Connect: \"%s%s\"\n", DestURL.host, DestURL.path);
   
   SendASCIIStrImmediate("\rConnecting to: ");
   SendASCIIStrImmediate(DestURL.host);
   SendASCIIStrImmediate(DestURL.path);
   SendPETSCIICharImmediate(PETSCIIreturn);
   
   if (client.connect(DestURL.host, DestURL.port))
   {
      const uint16_t MaxBuf = 200;
      char inbuf[MaxBuf];
      
      client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
         DestURL.path, DestURL.host);

      while(ReadClientLine(inbuf, MaxBuf)==true)
      {
         Printf_dbg("H: %s", inbuf); 
         if (strcmp(inbuf, "\r\n") == 0) 
         {
            SendASCIIStrImmediate("Connected\r");
            return true; //blank line indicates end of header
         }
      }
      client.stop();
      SendASCIIStrImmediate("Bad Header\r");
   }

   SendASCIIStrImmediate("Connect Failed\r");
   return false;
}

void DoSearch(const char *Term)
{
   char HexChar[] = "01234567890abcdef";
   stcURLParse URL =
   {
      "www.frogfind.com", //strcpy(URL.host = "www.frogfind.com");
      80,                 //URL.port = 80;
      "/?q=",             //strcpy(URL.path = "/?q=");
   };
      
   uint16_t UWPCharNum = strlen(URL.path);
   
   //encode special chars:
   //https://www.eso.org/~ndelmott/url_encode.html
   for(uint16_t CharNum=0; CharNum <= strlen(Term); CharNum++) //include terminator
   {
      //already lower case(?)
      uint8_t NextChar = Term[CharNum];
      if((NextChar >= 'a' && NextChar <= 'z') ||
         (NextChar >= 'A' && NextChar <= 'Z') ||
         (NextChar >= '.' && NextChar <= '9') ||  //   ./0123456789
          NextChar == 0)                          //include terminator
      {      
         URL.path[UWPCharNum++] = NextChar;      
      }
      else
      {
         //encode character (%xx hex val)
         URL.path[UWPCharNum++] = '%';
         URL.path[UWPCharNum++] = HexChar[NextChar >> 4];
         URL.path[UWPCharNum++] = HexChar[NextChar & 0x0f];
      }
   }
   
   WebConnect(URL);
}

void DownloadFile()
{  //assumes client connected and ready for download
   char FileName[] = "download1.prg";
   if (!client.connected())    
   {
      SendPETSCIICharImmediate(PETSCIIpink);
      SendASCIIStrImmediate("No data\r");  
      return;      
   }

   if (!SD.begin(BUILTIN_SDCARD))  // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   {
      SendPETSCIICharImmediate(PETSCIIpink);
      SendASCIIStrImmediate("No SD card\r");  
      return;      
   }
   
   //if (sourceFS->exists(FileNamePath))
   if (SD.exists(FileName))
   {
      SendPETSCIICharImmediate(PETSCIIpink);
      SendASCIIStrImmediate("File already exists\r");  
      return;      
   }
   
   File dataFile = SD.open(FileName, FILE_WRITE);
   if (!dataFile) 
   {
      SendPETSCIICharImmediate(PETSCIIpink);
      SendASCIIStrImmediate("Error opening file\r");
      return;
   }   
   
   SendASCIIStrImmediate("Downloading: ");
   SendASCIIStrImmediate(FileName);
   SendPETSCIICharImmediate(PETSCIIreturn);

   uint32_t BytesRead = 0;
   while (client.connected()) 
   {
      while (client.available()) 
      {
         dataFile.write(client.read());
         BytesRead++;
      }
   }      
   dataFile.close();
   char buf[100];
   sprintf(buf, "\rFinished: %lu bytes", BytesRead);
   SendASCIIStrImmediate(buf);
}

void ProcessBrowserCommand()
{
   char* CmdMsg = TxMsg; //local copy for manipulation
   
   if(strcmp(CmdMsg, "x") ==0) //Exit browse mode
   {
      client.stop();
      BrowserMode = false;
      RxQueueHead = RxQueueTail = 0; //dump the queue
      AddToPETSCIIStrToRxQueueLN("\rBrowser mode exit");
   }
   
   else if(strcmp(CmdMsg, "b") ==0) // Back/previous web page
   {
      if (PrevURLQueueNum<2) PrevURLQueueNum += NumPrevURLQueues-2; //wrap around bottom
      else PrevURLQueueNum -= 2;
      
      Printf_dbg("PrevURL# %d\n", PrevURLQueueNum);
      WebConnect(*PrevURLQueue[PrevURLQueueNum]);
   }
   
   else if(*CmdMsg >= '0' && *CmdMsg <= '9') //Hyperlink #
   {
      uint8_t CmdMsgVal = atoi(CmdMsg);
      
      if (CmdMsgVal > 0 && CmdMsgVal <= UsedPageLinkBuffs)
      {
         //we have a valid link # to follow...
         stcURLParse URL;
         
         ParseURL(PageLinkBuff[CmdMsgVal-1], URL); //zero based
         while (*CmdMsg >='0' && *CmdMsg <='9') CmdMsg++;  //move pointer past numbers
         
         if(URL.host[0] == 0) //relative path, use same server/port, append path
         {
            uint8_t CurQueuNum;
            if (PrevURLQueueNum == 0) CurQueuNum = NumPrevURLQueues - 1;
            else  CurQueuNum = PrevURLQueueNum - 1;
            
            if(URL.path[0] != '/') //if not root ref, add previous path to beginning
            {  
               char temp[MaxURLPathSize];
               strcpy(temp, URL.path); 
               strcpy(URL.path, PrevURLQueue[CurQueuNum]->path);
               strcat(URL.path, temp);
            }
            URL.port = PrevURLQueue[CurQueuNum]->port;
            strcpy(URL.host, PrevURLQueue[CurQueuNum]->host);
         }
         WebConnect(URL);
         
         if (*CmdMsg == 'd') 
         {
            DownloadFile();   
            client.stop();  //in case of unfinished/error, don't read it in as text
         }            
      }
   }
   
   else if(*CmdMsg == 'u') //URL
   {
      CmdMsg++; //past the 'u'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command
      
      stcURLParse URL;
      char httpServer[MaxTagSize] = "http://";
      strcat(httpServer, CmdMsg);
      ParseURL(httpServer, URL);
      WebConnect(URL);
   }
   
   else if(*CmdMsg == 's') //search
   {
      CmdMsg++; //past the 's'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command   
      DoSearch(CmdMsg);  //includes WebConnect
   }
   
   else if(*CmdMsg != 0) //unrecognized command
   {
      SendPETSCIICharImmediate(PETSCIIpink);
      SendASCIIStrImmediate("Unknown Command\r");
   }
   
   else if(PagePaused) //empty command, and paused
   { 
      SendPETSCIICharImmediate(PETSCIIcursorUp); //Cursor up to overwrite prompt & scroll on
      //SendPETSCIICharImmediate(PETSCIIclearScreen); //clear screen for next page
   }
   
   SendPETSCIICharImmediate(PETSCIIwhite); 
   PageCharsReceived = 0; //un-pause on any command, or just return key
   PagePaused = false;
   UsedPageLinkBuffs = 0;
}

//_____________________________________Handlers_____________________________________________________

FLASHMEM void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = SwiftCmndDefault;
   SwiftRegControl = 0;
   CycleCountdown=0;
   PlusCount=0;
   PageCharsReceived = 0;
   PrevURLQueueNum = 0;
   NMIassertMicros = 0;
   PlusCount=0;
   ConnectedToHost = false;
   BrowserMode = false;
   PagePaused = false;
   PrintingHyperlink = false;
   
   RxQueueHead = RxQueueTail = TxMsgOffset =0;
   RxQueue = (uint8_t*)malloc(RxQueueSize);
   TxMsg = (char*)malloc(TxMsgMaxSize);
   for(uint8_t cnt=0; cnt<NumPageLinkBuffs; cnt++) PageLinkBuff[cnt] = (char*)malloc(MaxTagSize);
   for(uint8_t cnt=0; cnt<NumPrevURLQueues; cnt++) 
   {
      PrevURLQueue[cnt] = (stcURLParse*)malloc(sizeof(stcURLParse));
      strcpy(PrevURLQueue[cnt]->path, "/teensyrom/");
      strcpy(PrevURLQueue[cnt]->host, "sensoriumembedded.com");
      PrevURLQueue[cnt]->port = 80;
   }
   randomSeed(ARM_DWT_CYCCNT);
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
            CycleCountdown = C64CycBetweenRx;
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
            //Could confirm setting 8N1 & acceptable baud?
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
         if (!ConnectedToHost) AddRawStrToRxQueue("<br>*End of Page*<eoftag>");  //add special tag to catch when complete
      }
      else
      {
         AddToPETSCIIStrToRxQueue("\r\r\r*** ");
         if (ConnectedToHost) AddToPETSCIIStrToRxQueueLN("connected to host");
         else AddToPETSCIIStrToRxQueueLN("not connected");
      }
   }
   
   //if client data available, add to Rx Queue
   #ifdef DbgMsgs_IO
      if(client.available())
      {
         uint16_t Cnt = 0;
         //Serial.printf("RxIn %d+", RxQueueUsed);
         while (client.available())
         {
            AddRawCharToRxQueue(client.read());
            Cnt++;
         }
         //Serial.printf("%d=%d\n", Cnt, RxQueueUsed);
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
         //Printf_dbg("send %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         client.print((char)SwiftTxBuf);  //send it
         if(SwiftTxBuf=='+')
         {
            if(millis()-LastTxMillis>1000 || PlusCount!=0) //Must be preceded by at least 1 second of no characters
            {   
               if(++PlusCount>3) PlusCount=0;
            }
         }
         else PlusCount=0;
         
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
      }
      else  //off-line/at commands or BrowserMode..................................
      {         
         Printf_dbg("echo %02x: %c -> ", SwiftTxBuf, SwiftTxBuf);
         
         if(BrowserMode) SendPETSCIICharImmediate(SwiftTxBuf); //echo it now, buffer may be paused or filling
         else AddRawCharToRxQueue(SwiftTxBuf); //echo it at end of buffer
         
         SwiftTxBuf &= 0x7f; //bit 7 is Cap in Graphics mode
         if (SwiftTxBuf & 0x40) SwiftTxBuf |= 0x20;  //conv to lower case PETSCII
         Printf_dbg("%02x: %c\n", SwiftTxBuf);
         
         if (TxMsgOffset && (SwiftTxBuf==0x08 || SwiftTxBuf==0x14)) TxMsgOffset--; //Backspace in ascii  or  Delete in PETSCII
         else TxMsg[TxMsgOffset++] = SwiftTxBuf; //otherwise store it
         
         if (SwiftTxBuf == 13 || TxMsgOffset == TxMsgMaxSize) //return hit or max size
         {
            SwiftRegStatus |= SwiftStatusTxEmpty; //clear the flag after last SwiftTxBuf access
            TxMsg[TxMsgOffset-1] = 0; //terminate it
            Printf_dbg("TxMsg: %s\n", TxMsg);
            if(BrowserMode) ProcessBrowserCommand();
            else
            {
               ProcessATCommand();
               if (!BrowserMode) AddToPETSCIIStrToRxQueueLN("ok\r");
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
      client.stop();
      AddToPETSCIIStrToRxQueueLN("\r*click*");
   }

   if (PageCharsReceived < 880 || PrintingHyperlink) CheckSendRxQueue();
   else
   {
      if (!PagePaused)
      {
         PagePaused = true;
         SendPETSCIICharImmediate(PETSCIIrvsOn);
         SendPETSCIICharImmediate(PETSCIIpurple);
         SendASCIIStrImmediate("\rPause (#,S[],U[],X,B,Ret)");
         SendPETSCIICharImmediate(PETSCIIrvsOff);
         SendPETSCIICharImmediate(PETSCIIlightGreen);
      }
   }
}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}


