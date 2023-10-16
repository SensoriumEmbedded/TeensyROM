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

extern volatile uint32_t CycleCountdown;
extern void EEPreadBuf(uint16_t addr, uint8_t* buf, uint8_t len);
extern void EEPwriteBuf(uint16_t addr, const uint8_t* buf, uint8_t len);
void AddBrowserCommandsToRxQueue();

#define NumLinkBuffs  9

uint8_t* RxQueue = NULL;  //circular queue to pipe data to the c64 
char* TxMsg = NULL;  //to hold messages (AT/browser commands) when off line
char* LinkBuf[NumLinkBuffs]; //hold links from tags for user selection in browser
uint8_t  UsedLinkBuffs;   //how many LinkBuf elements have been Used
uint32_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost, BrowserMode, PagePaused, PrintingHyperlink;
uint32_t PageCharsReceived;
uint32_t NMIassertMicros;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
uint8_t PlusCount;
uint32_t LastTxMillis = millis();

#define MaxTagSize         300
#define TxMsgMaxSize       128
#define RxQueueSize       (1024*320) 
#define C64CycBetweenRx   2300   //stops NMI from re-asserting too quickly. chars missed in large buffs when lower
#define NMITimeoutnS       300   //if Rx data not read within this time, deassert NMI anyway

// 6551 ACIA interface emulation
//register locations (IO1, DExx)
#define IORegSwiftData    0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus  0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand 0x02   // Swift Emulation Command Reg
#define IORegSwiftControl 0x03   // Swift Emulation Control Reg

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

//PETSCII Special Symbols
#define PETSCIIpurple      0x9c
#define PETSCIIwhite       0x05
#define PETSCIIlightBlue   0x9a
#define PETSCIIyellow      0x9e
#define PETSCIIpink        0x96
#define PETSCIIlightGreen  0x99

#define PETSCIIreturn      0x0d
#define PETSCIIrvsOn       0x12
#define PETSCIIrvsOff      0x92
#define PETSCIIclearScreen 0x93
#define PETSCIIcursorUp    0x91

#define RxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+RxQueueSize-RxQueueTail))

bool EthernetInit()
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
   
void SetEthEEPDefaults()
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
   
uint8_t PullFromRxQueue()
{
  uint8_t c = RxQueue[RxQueueTail++]; 
  if (RxQueueTail == RxQueueSize) RxQueueTail = 0;
  //Printf_dbg("Pull H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
  return c;
}

bool ReadyToSendRx()
{
   //  if IRQ enabled, 
   //  and IRQ not set, 
   //  and enough time has passed
   //  then C64 is ready to receive...
   return ((SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && \
      (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0 && \
      CycleCountdown == 0);
}

bool CheckRxNMITimeout()
{
   //Check for Rx NMI timeout: Doesn't happen unless a lot of serial printing enabled (ie DbgMsgs_IO) causing missed reg reads
   if ((SwiftRegStatus & SwiftStatusIRQ)  && (micros() - NMIassertMicros > NMITimeoutnS))
   {
     Serial.println("Rx NMI Timeout!");
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
     return false;
   }
   return true;
}

void SendRxByte(uint8_t ToSend) 
{
   //send character if non-zero, otherwise skip it to save a full c64 char cycle
   //assumes ReadyToSendRx() is true before calling
   if(ToSend)
   {  
      SwiftRxBuf = ToSend;
      SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
      SetNMIAssert;
      NMIassertMicros = micros();
   }
}

void SendPETSCIICharImmediate(char CharToSend)
{
   //wait for c64 to be ready or NMI timeout
   while(!ReadyToSendRx()) if(!CheckRxNMITimeout()) return;

   if (BrowserMode) PageCharsReceived++;
   
   SendRxByte(CharToSend);
}

void SendASCIIStrImmediate(const char* CharsToSend)
{
   for(uint16_t CharNum = 0; CharNum < strlen(CharsToSend); CharNum++)
      SendPETSCIICharImmediate(ToPETSCII(CharsToSend[CharNum]));
}

void CheckSendRxQueue()
{  
   //  if queued Rx data available to send to C64, and C64 is ready, then read/send 1 character to C64...
   if (RxQueueUsed > 0 && ReadyToSendRx())
   {
      uint8_t ToSend = PullFromRxQueue();
      //Printf_dbg("RxBuf=%02x: %c\n", ToSend, ToSend); //not recommended
      
      if (BrowserMode)
      {  //browser data is stored in ASCII to preserve tag info, convert rest to PETSCII before sending
         if(ToSend == '<')
         { //retrieve and interpret HTML Tag
            char TagBuf[MaxTagSize];
            uint16_t BufCnt = 0;
            ToSend = 0; //default to no char if not set below
            
            //pull tag from queue until >, queue empty, or buff max size
            while (RxQueueUsed > 0)
            {
               TagBuf[BufCnt] = PullFromRxQueue();
               if(TagBuf[BufCnt] == '>') break;
               if(++BufCnt == MaxTagSize-1) break;
            }
            TagBuf[BufCnt] = 0;

            //execute tag, if needed
            if(strcmp(TagBuf, "br")==0 || strcmp(TagBuf, "li")==0 || strcmp(TagBuf, "p")==0 || strcmp(TagBuf, "/p")==0) 
            {
               ToSend = PETSCIIreturn;
               PageCharsReceived += 40-(PageCharsReceived % 40);
            }
            else if(strcmp(TagBuf, "/b")==0) ToSend = PETSCIIwhite; //unbold
            else if(strcmp(TagBuf, "b")==0) ToSend = PrintingHyperlink ? 0 : PETSCIIyellow; //bold, but don't change hyperlink color
            else if(strcmp(TagBuf, "eoftag")==0) AddBrowserCommandsToRxQueue();  // special tag to signal complete
            else if(strncmp(TagBuf, "a href=", 7)==0) 
            { //start of hyperlink text, save hyperlink
               SendPETSCIICharImmediate(PETSCIIpurple); 
               SendPETSCIICharImmediate(PETSCIIrvsOn); 
               if (UsedLinkBuffs < NumLinkBuffs)
               {
                  for(uint16_t CharNum = 8; CharNum < strlen(TagBuf); CharNum++)
                  { //terminate at first 
                     if(TagBuf[CharNum]==' ' || 
                        TagBuf[CharNum]=='\'' ||
                        TagBuf[CharNum]=='\"' ||
                        TagBuf[CharNum]=='#') TagBuf[CharNum] = 0; //terminate at space, #, ', or "
                  }
                  strcpy(LinkBuf[UsedLinkBuffs], TagBuf+8); // and from beginning
                  Printf_dbg("Link #%d: %s\n", UsedLinkBuffs+1, LinkBuf[UsedLinkBuffs]);
                  SendPETSCIICharImmediate(ToPETSCII('1' + UsedLinkBuffs++));
               }
               else SendPETSCIICharImmediate('*');
               
               SendPETSCIICharImmediate(PETSCIIlightBlue); 
               SendPETSCIICharImmediate(PETSCIIrvsOff);
               //Leave ToSend as 0, can't send again until we wait for prev to complete (ReadyToSendRx)
               PageCharsReceived++;
               PrintingHyperlink = true;
            }
            else if(strcmp(TagBuf, "/a")==0)
            { //end of hyperlink text
               ToSend = PETSCIIwhite; 
               PrintingHyperlink = false;
            }
            else if(strcmp(TagBuf, "/form")==0)
            { //OK as a standard?   FrogFind specific....
               ToSend = PETSCIIclearScreen;
               PageCharsReceived = 0;
               PagePaused = false;
               UsedLinkBuffs = 0;
            }
            else Printf_dbg("Unk Tag: <%s>\n", TagBuf);
            
         } // '<' (tag) received
         else 
         {
            if(ToSend == 13) ToSend = 0; //ignore return chars
            else
            {
               ToSend = ToPETSCII(ToSend);
               if (ToSend) PageCharsReceived++; //normal char
            }
         }
      } //BrowserMode
      
      SendRxByte(ToSend);
   }
   
   CheckRxNMITimeout();
}

void FlushRxQueue()
{
   while (RxQueueUsed) CheckSendRxQueue();  
}

void AddPETSCIICharToRxQueue(uint8_t c)
{
  if (RxQueueUsed >= RxQueueSize-1)
  {
     Serial.println("RxBuff Overflow!");
     //RxQueueHead = RxQueueTail = 0;
     ////just in case...
     //SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     //SetNMIDeassert;
     return;
  }
  RxQueue[RxQueueHead++] = c; 
  if (RxQueueHead == RxQueueSize) RxQueueHead = 0;
  //Printf_dbg("Push H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
}

void AddStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   //Printf_dbg("PStrToRx(Len=%d): %s\n", strlen(s), s);
   while(s[CharNum] != 0) AddPETSCIICharToRxQueue(s[CharNum++]);
}

void AddASCIIStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   //Printf_dbg("AStrToRx(Len=%d): %s\n", strlen(s), s);
   while(s[CharNum] != 0)
   {
      AddPETSCIICharToRxQueue(ToPETSCII(s[CharNum++]));
   }  
}

void AddASCIIStrToRxQueueLN(const char* s)
{
   AddASCIIStrToRxQueue(s);
   AddASCIIStrToRxQueue("\r");
}

void AddIPaddrToRxQueueLN(IPAddress ip)
{
   char Buf[50];
   sprintf(Buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
   AddASCIIStrToRxQueueLN(Buf);
}

void AddMACToRxQueueLN(uint8_t* mac)
{
   char Buf[50];
   sprintf(Buf, " MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   AddASCIIStrToRxQueueLN(Buf);
}

void AddInvalidFormatToRxQueueLN()
{
   AddASCIIStrToRxQueueLN("Invalid Format");
}

void AddBrowserCommandsToRxQueue()
{
   PageCharsReceived = 0;
   PagePaused = false;

   SendPETSCIICharImmediate(PETSCIIreturn);
   SendPETSCIICharImmediate(PETSCIIpurple); 
   SendPETSCIICharImmediate(PETSCIIrvsOn); 
   SendASCIIStrImmediate("Browser Commands:\r");
   SendASCIIStrImmediate("S[Term]: Search    [Link#]: Go to link\r");
   SendASCIIStrImmediate(" U[URL]: Go to URL       X: Exit\r");
   SendASCIIStrImmediate(" Return: Continue from pause\r");
   SendPETSCIICharImmediate(PETSCIIlightGreen);
}

void AddUpdatedToRxQueueLN()
{
   AddASCIIStrToRxQueueLN("Updated");
}

void AddDHCPEnDisToRxQueueLN()
{
   AddASCIIStrToRxQueue(" DHCP: ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddASCIIStrToRxQueueLN("Enabled");
   else AddASCIIStrToRxQueueLN("Disabled");
}
  
void AddDHCPTimeoutToRxQueueLN()
{
   uint16_t invalU16;
   char buf[50];
   EEPROM.get(eepAdDHCPTimeout, invalU16);
   sprintf(buf, " DHCP Timeout: %dmS", invalU16);
   AddASCIIStrToRxQueueLN(buf);
}
  
void AddDHCPRespTOToRxQueueLN()
{
   uint16_t invalU16;
   char buf[50];
   EEPROM.get(eepAdDHCPRespTO, invalU16);
   sprintf(buf, " DHCP Response Timeout: %dmS", invalU16);
   AddASCIIStrToRxQueueLN(buf);
} 
  
void StrToIPToEE(char* Arg, uint8_t EEPaddress)
{
   uint8_t octnum =1;
   IPAddress ip;   
   
   AddASCIIStrToRxQueueLN(" IP Addr");
   ip[0]=atoi(Arg);
   while(octnum<4)
   {
      Arg=strchr(Arg, '.');
      if(Arg==NULL)
      {
         AddInvalidFormatToRxQueueLN();
         return;
      }
      ip[octnum++]=atoi(++Arg);
   }
   EEPROM.put(EEPaddress, (uint32_t)ip);
   AddUpdatedToRxQueueLN();
   AddASCIIStrToRxQueue("to ");
   AddIPaddrToRxQueueLN(ip);
}


//_____________________________________AT Commands_____________________________________________________

void AT_BROWSE(char* CmdArg)
{  //ATBROWSE   Enter Browser mode
   AddBrowserCommandsToRxQueue();
   UsedLinkBuffs = 0;
   BrowserMode = true;
}

void AT_DT(char* CmdArg)
{  //ATDT<HostName>:<Port>   Connect telnet
   uint16_t  Port = 6400; //default if not defined
   char* Delim = strstr(CmdArg, ":");


   if (Delim != NULL) //port defined, read it
   {
      Delim[0]=0; //terminate host name
      Port = atol(Delim+1);
      //if (Port==0) AddASCIIStrToRxQueueLN("invalid port #");
   }
   
   char Buf[100];
   sprintf(Buf, "Trying %s\r\non port %d...", CmdArg, Port);
   AddASCIIStrToRxQueueLN(Buf);
   FlushRxQueue();
   //Printf_dbg("Host name: %s  Port: %d\n", CmdArg, Port);
   
   if (client.connect(CmdArg, Port)) AddASCIIStrToRxQueueLN("Done");
   else AddASCIIStrToRxQueueLN("Failed!");
}

void AT_C(char* CmdArg)
{  //ATC: Connect Ethernet
   AddASCIIStrToRxQueue("Connect Ethernet ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddASCIIStrToRxQueue("via DHCP...");
   else AddASCIIStrToRxQueue("using Static...");
   FlushRxQueue();
   
   if (EthernetInit()==true)
   {
      AddASCIIStrToRxQueueLN("Done");
      
      byte mac[6]; 
      Ethernet.MACAddress(mac);
      AddMACToRxQueueLN(mac);
      
      uint32_t ip = Ethernet.localIP();
      AddASCIIStrToRxQueue(" Local IP: ");
      AddIPaddrToRxQueueLN(ip);

      ip = Ethernet.subnetMask();
      AddASCIIStrToRxQueue(" Subnet Mask: ");
      AddIPaddrToRxQueueLN(ip);

      ip = Ethernet.gatewayIP();
      AddASCIIStrToRxQueue(" Gateway IP: ");
      AddIPaddrToRxQueueLN(ip);
   }
   else
   {
      AddASCIIStrToRxQueueLN("Failed!");
      if (Ethernet.hardwareStatus() == EthernetNoHardware) AddASCIIStrToRxQueueLN(" HW was not found");
      else if (Ethernet.linkStatus() == LinkOFF) AddASCIIStrToRxQueueLN(" Cable is not connected");
   }
}

void AT_S(char* CmdArg)
{
   uint32_t ip;
   uint8_t  mac[6];
   
   AddASCIIStrToRxQueueLN("General Settings:");

   EEPreadBuf(eepAdMyMAC, mac, 6);
   AddMACToRxQueueLN(mac);
   
   AddDHCPEnDisToRxQueueLN();
   
   AddASCIIStrToRxQueueLN("DHCP only:");    
   AddDHCPTimeoutToRxQueueLN();
   AddDHCPRespTOToRxQueueLN();
   
   AddASCIIStrToRxQueueLN("Static only:");    
   AddASCIIStrToRxQueue(" My IP: ");
   EEPROM.get(eepAdMyIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddASCIIStrToRxQueue(" DNS IP: ");
   EEPROM.get(eepAdDNSIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddASCIIStrToRxQueue(" Gateway IP: ");
   EEPROM.get(eepAdGtwyIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddASCIIStrToRxQueue(" Subnet Mask: ");
   EEPROM.get(eepAdMaskIP, ip);
   AddIPaddrToRxQueueLN(ip);

}

void AT_RNDMAC(char* CmdArg)
{
   uint8_t mac[6];   
   
   AddASCIIStrToRxQueueLN("Random MAC Addr");
   for(uint8_t octnum =0; octnum<6; octnum++) mac[octnum]=random(0,256);
   mac[0] &= 0xFE; //Unicast
   mac[0] |= 0x02; //Local Admin
   EEPwriteBuf(eepAdMyMAC, mac, 6);
   AddUpdatedToRxQueueLN();
   AddMACToRxQueueLN(mac);
}

void AT_MAC(char* CmdArg)
{
   uint8_t octnum =1;
   uint8_t mac[6];   
   
   AddASCIIStrToRxQueueLN("MAC Addr");
   mac[0]=strtoul(CmdArg, NULL, 16);
   while(octnum<6)
   {
      CmdArg=strchr(CmdArg, ':');
      if(CmdArg==NULL)
      {
         AddInvalidFormatToRxQueueLN();
         return;
      }
      mac[octnum++]=strtoul(++CmdArg, NULL, 16);     
   }
   EEPwriteBuf(eepAdMyMAC, mac, 6);
   AddUpdatedToRxQueueLN();
   AddMACToRxQueueLN(mac);
}

void AT_DHCP(char* CmdArg)
{
   if(CmdArg[1]!=0 || CmdArg[0]<'0' || CmdArg[0]>'1')
   {
      AddInvalidFormatToRxQueueLN();
      return;
   }
   EEPROM.write(eepAdDHCPEnabled, CmdArg[0]-'0');
   AddUpdatedToRxQueueLN();
   AddDHCPEnDisToRxQueueLN();
}

void AT_DHCPTIME(char* CmdArg)
{
   uint16_t NewTime = atol(CmdArg);
   if(NewTime==0)
   {
      AddInvalidFormatToRxQueueLN();
      return;
   }   
   EEPROM.put(eepAdDHCPTimeout, NewTime);
   AddUpdatedToRxQueueLN();
   AddDHCPTimeoutToRxQueueLN();
}

void AT_DHCPRESP(char* CmdArg)
{
   uint16_t NewTime = atol(CmdArg);
   if(NewTime==0)
   {
      AddInvalidFormatToRxQueueLN();
      return;
   }
   EEPROM.put(eepAdDHCPRespTO, NewTime);
   AddUpdatedToRxQueueLN();
   AddDHCPRespTOToRxQueueLN();
}

void AT_MYIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("My");
   StrToIPToEE(CmdArg, eepAdMyIP);
}

void AT_DNSIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("DNS");
   StrToIPToEE(CmdArg, eepAdDNSIP);
}

void AT_GTWYIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("Gateway");
   StrToIPToEE(CmdArg, eepAdGtwyIP);
}

void AT_MASKIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("Subnet Mask");
   StrToIPToEE(CmdArg, eepAdMaskIP);
}

void AT_DEFAULTS(char* CmdArg)
{
   AddUpdatedToRxQueueLN();
   SetEthEEPDefaults();
   AT_S(NULL);
}

void AT_HELP(char* CmdArg)
{  //                      1234567890123456789012345678901234567890
   AddASCIIStrToRxQueueLN("General AT Commands:");
   AddASCIIStrToRxQueueLN(" AT?   This help menu");
   AddASCIIStrToRxQueueLN(" AT    Ping");
   AddASCIIStrToRxQueueLN(" ATC   Connect Ethernet, display info");
   AddASCIIStrToRxQueueLN(" ATDT<HostName>:<Port>  Connect to host");

   AddASCIIStrToRxQueueLN("Modify saved parameters:");
   AddASCIIStrToRxQueueLN(" AT+S  Display stored Ethernet settings");
   AddASCIIStrToRxQueueLN(" AT+DEFAULTS  Set defaults for all ");
   AddASCIIStrToRxQueueLN(" AT+RNDMAC  MAC address to random value");
   AddASCIIStrToRxQueueLN(" AT+MAC=<XX:XX:XX:XX:XX:XX>  Set MAC");
   AddASCIIStrToRxQueueLN(" AT+DHCP=<0:1>  DHCP On(1)/Off(0)");

   AddASCIIStrToRxQueueLN("DHCP mode only: ");
   AddASCIIStrToRxQueueLN(" AT+DHCPTIME=<D>  DHCP Timeout in mS");
   AddASCIIStrToRxQueueLN(" AT+DHCPRESP=<D>  DHCP Response Timeout");

   AddASCIIStrToRxQueueLN("Static mode only: ");
   AddASCIIStrToRxQueueLN(" AT+MYIP=<D.D.D.D>   Local IP address");
   AddASCIIStrToRxQueueLN(" AT+DNSIP=<D.D.D.D>  DNS IP address");
   AddASCIIStrToRxQueueLN(" AT+GTWYIP=<D.D.D.D> Gateway IP address");
   AddASCIIStrToRxQueueLN(" AT+MASKIP=<D.D.D.D> Subnet Mask");

   AddASCIIStrToRxQueueLN("When in connected/on-line mode:");
   AddASCIIStrToRxQueueLN(" +++   Disconnect from host");
}

#define MaxATcmdLength   20

struct stcATCommand
{
  char Command[MaxATcmdLength];
  void (*Function)(char*); 
};

stcATCommand ATCommands[] =
{
   "dt"        , &AT_DT,
   "c"         , &AT_C,
   "+s"        , &AT_S,
   "+rndmac"   , &AT_RNDMAC,
   "+mac="     , &AT_MAC,
   "+dhcp="    , &AT_DHCP,
   "+dhcptime=", &AT_DHCPTIME,
   "+dhcpresp=", &AT_DHCPRESP,
   "+myip="    , &AT_MYIP,
   "+dnsip="   , &AT_DNSIP,
   "+gtwyip="  , &AT_GTWYIP,
   "+maskip="  , &AT_MASKIP,
   "+defaults" , &AT_DEFAULTS,
   "?"         , &AT_HELP,
   "browse"    , &AT_BROWSE,
};
   
void ProcessATCommand()
{

   char* CmdMsg = TxMsg; //local copy for manipulation
      
   if (strstr(CmdMsg, "at")!=CmdMsg)
   {
      AddASCIIStrToRxQueueLN("AT not found");
      return;
   }
   CmdMsg+=2; //move past the AT
   if(CmdMsg[0]==0) return;  //ping
   
   uint16_t Num = 0;
   while(Num < sizeof(ATCommands)/sizeof(ATCommands[0]))
   {
      if (strstr(CmdMsg, ATCommands[Num].Command) == CmdMsg)
      {
         CmdMsg+=strlen(ATCommands[Num].Command); //move past the Command
         while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after AT command
         ATCommands[Num].Function(CmdMsg);
         return;
      }
      Num++;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddASCIIStrToRxQueue("unknown command: ");
   AddASCIIStrToRxQueueLN(TxMsg);
}

void WebConnect(char *WebPage)
{
   client.stop();
   Printf_dbg("Connecting to: \"%s\"\n", WebPage);
   RxQueueHead = RxQueueTail = 0; //dump the queue
   SendASCIIStrImmediate("\rConnecting to: ");
   SendASCIIStrImmediate(WebPage);
   SendPETSCIICharImmediate(PETSCIIreturn);
   
   if (client.connect("www.frogfind.com", 80)) //filter all through FrogFind
   {
      client.printf("GET %s HTTP/1.1\r\n", WebPage);
      client.println("Host: www.frogfind.com");
      client.println("Connection: close");
      client.println();   
      
	  //Debug: Read full page now to see full size
      //while (client.connected()) 
      //{
      //   while (client.available()) 
      //   {
      //      uint8_t c = client.read();
      //      if(BrowserMode) c = ToPETSCII(c); //incoming browser data is ascii
      //      AddPETSCIICharToRxQueue(c);
      //   }
      //   Printf_dbg("None available: %lu\n", RxQueueUsed);
      //}
      //Printf_dbg("Page size: %lu\n", RxQueueUsed);
   }
   else AddASCIIStrToRxQueueLN("Connect Failed");

}

void ProcessBrowserCommand()
{
   char* CmdMsg = TxMsg; //local copy for manipulation
   
   if(strcmp(CmdMsg, "x") ==0)
   {
      client.stop();
      BrowserMode = false;
      RxQueueHead = RxQueueTail = 0; //dump the queue
      AddASCIIStrToRxQueueLN("\rBrowser mode exit");
   }
   else if(CmdMsg[0] >= '1' && CmdMsg[0] <= '9') //Hyperlink
   {
      uint8_t LinkNum = CmdMsg[0] - '1';  //now zero based
      if (LinkNum < UsedLinkBuffs) WebConnect(LinkBuf[LinkNum]);
   }
   else if(CmdMsg[0] == 's') //search
   {
      CmdMsg++; //past the 's'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command
      
      char WebPage[MaxTagSize];
      strcpy(WebPage, "/?q=");
      uint16_t WPChar = strlen(WebPage);
      //replace space with %20, could do this to all special characters, or entire string...
      //https://www.eso.org/~ndelmott/url_encode.html
      for(uint16_t CharNum=0; CharNum<=strlen(CmdMsg);CharNum++) //include terminator
      {
         if(CmdMsg[CharNum] == ' ') 
         {
            WebPage[WPChar++] = '%';
            WebPage[WPChar++] = '2';
            WebPage[WPChar++] = '0';
         }
         else WebPage[WPChar++] = CmdMsg[CharNum];
      }
      WebConnect(WebPage);
   }
   else if(PagePaused) //unrecognized or no command, and paused
   {
      SendPETSCIICharImmediate(PETSCIIcursorUp); //Cursor up to overwrite prompt & scroll on
      //SendPETSCIICharImmediate(PETSCIIclearScreen); //clear screen for next page
   }
   
   SendPETSCIICharImmediate(PETSCIIwhite); 
   PageCharsReceived = 0; //un-pause on any command, or just return key
   PagePaused = false;
   UsedLinkBuffs = 0;
}

//_____________________________________Handlers_____________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = SwiftCmndDefault;
   SwiftRegControl = 0;
   CycleCountdown=0;
   PlusCount=0;
   PageCharsReceived = 0;
   NMIassertMicros = 0;
   PlusCount=0;
   ConnectedToHost = false;
   BrowserMode = false;
   PagePaused = false;
   PrintingHyperlink = false;
   
   RxQueueHead = RxQueueTail = TxMsgOffset =0;
   RxQueue = (uint8_t*)malloc(RxQueueSize);
   TxMsg = (char*)malloc(TxMsgMaxSize);
   for(uint8_t cnt=0; cnt<NumLinkBuffs; cnt++) LinkBuf[cnt] = (char*)malloc(MaxTagSize);
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
         if (!ConnectedToHost) AddStrToRxQueue("*End of Page*<eoftag>");  //add special tag to catch when complete
      }
      else
      {
         AddASCIIStrToRxQueue("\r\r\r*** ");
         if (ConnectedToHost) AddASCIIStrToRxQueueLN("connected to host");
         else AddASCIIStrToRxQueueLN("not connected");
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
            AddPETSCIICharToRxQueue(client.read());
            Cnt++;
         }
         //Serial.printf("%d=%d\n", Cnt, RxQueueUsed);
         if (RxQueueUsed>3000) Serial.printf("Lrg RxQueue add: %d  total: %d\n", Cnt, RxQueueUsed);
      }
   #else
      while (client.available()) AddPETSCIICharToRxQueue(client.read());
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
         else AddPETSCIICharToRxQueue(SwiftTxBuf); //echo it at end of buffer
         
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
               if (!BrowserMode) AddASCIIStrToRxQueueLN("ok\r");
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
      AddASCIIStrToRxQueueLN("\r*click*");
   }

   if (PageCharsReceived < 880 || PrintingHyperlink) CheckSendRxQueue();
   else
   {
      if (!PagePaused)
      {
         PagePaused = true;
         SendPETSCIICharImmediate(PETSCIIrvsOn);
         SendPETSCIICharImmediate(PETSCIIpurple);
         SendASCIIStrImmediate("\rPause (#,S[],U[],X,Ret)");
         SendPETSCIICharImmediate(PETSCIIrvsOff);
         SendPETSCIICharImmediate(PETSCIIlightGreen);
      }
   }
}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}


