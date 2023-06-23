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

uint8_t* RxQueue;  //circular queue to pipe data to the c64 
char* TxMsg;  //to hold messages (AT commands) when off line
uint16_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost = false;
uint32_t NMIassertMicros = 0;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;

#define TxMsgSize          128
#define RxQueueSize       8192 
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
   
   Serial.printf("Took %d mS\n", (millis() - beginWait));
   //Serial.println(Ethernet.localIP());
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

void CheckSendRx()
{
   //  if Rx data available to send to C64, IRQ enabled, and ready (not set), 
   //  and enough time has passed, then read/send to C64...
   if (RxQueueUsed > 0 && \
      (SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && \
      (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0 && \
      CycleCountdown == 0)
   {
      SwiftRxBuf = PullFromRxQueue();
      //Printf_dbg("RxBuf=%02x: %c\n", SwiftRxBuf, SwiftRxBuf);
      SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
      SetNMIAssert;
      NMIassertMicros = micros();
   }
      
   //Rx NMI timeout: Isn't needed unless a lot of printing enabled (ie DbgMsgs_IO) causing missed reg reads
   if ((SwiftRegStatus & SwiftStatusIRQ)  && (micros() - NMIassertMicros > NMITimeoutnS))
   {
     Serial.println("Rx NMI Timeout!");
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
   }
}

void FlushRxQueue()
{
   while (RxQueueUsed) CheckSendRx();  
}

void AddCharToRxQueue(uint8_t c)
{
  if (RxQueueUsed >= RxQueueSize-1)
  {
     Serial.println("RxBuff Overflow!");
     RxQueueHead = RxQueueTail = 0;
     //just in case...
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
     return;
  }
  RxQueue[RxQueueHead++] = c; 
  if (RxQueueHead == RxQueueSize) RxQueueHead = 0;
  //Printf_dbg("Push H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
}

void AddASCIIStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   //Printf_dbg("StrToRx(Len=%d): %s\n", strlen(s), s);
   while(s[CharNum] != 0)
   {
      AddCharToRxQueue(ToPETSCII(s[CharNum]));
      CharNum++; //putting this inside the above statment breaks it due to petscii macro multiple references
   }  
}

void AddASCIIStrToRxQueueLN(const char* s)
{
   AddASCIIStrToRxQueue(s);
   AddASCIIStrToRxQueue("\r\n");
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

#define MaxATcmdLength   20

struct stcATCommand
{
  char Command[MaxATcmdLength];
  void (*Function)(char*); 
};

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
   
   AddASCIIStrToRxQueueLN("Default Settings:");

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
};

void ProcessATCommand()
{
   char* CmdMsg = TxMsg; //local copy for manipulation
   uint16_t Num=0;
   
   while (CmdMsg[Num])
   {  //conv to lower case ASCII
      CmdMsg[Num] &= 127;
      if(CmdMsg[Num] >= 'A') CmdMsg[Num] ^= 32;
      Num++;
   }
   Printf_dbg("AT Msg recvd: %s\n", CmdMsg);
   
   if (strstr(CmdMsg, "at")!=CmdMsg)
   {
      AddASCIIStrToRxQueueLN("AT not found");
      return;
   }
   CmdMsg+=2; //past the AT
   if(CmdMsg[0]==0) return;  //ping
   
   Num=0;
   while(Num < sizeof(ATCommands)/sizeof(ATCommands[0]))
   {
      if (strstr(CmdMsg, ATCommands[Num].Command) == CmdMsg)
      {
         CmdMsg+=strlen(ATCommands[Num].Command); //move past the Command
         ATCommands[Num].Function(CmdMsg);
         return;
      }
      Num++;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddASCIIStrToRxQueue("unknown command: ");
   AddASCIIStrToRxQueueLN(TxMsg);
}




//_____________________________________Handlers_____________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = SwiftCmndDefault;
   SwiftRegControl = 0;
   CycleCountdown=0;
   
   RxQueueHead = RxQueueTail = TxMsgOffset =0;
   free(RxQueue);
   RxQueue = (uint8_t*)malloc(RxQueueSize);
   free(TxMsg);
   TxMsg = (char*)malloc(TxMsgSize);
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
   if (ConnectedToHost != client.connected())
   {
      ConnectedToHost = client.connected();
      AddASCIIStrToRxQueue("\r\n\r\n\r\n*** ");
      if (ConnectedToHost) AddASCIIStrToRxQueueLN("connected to host");
      else AddASCIIStrToRxQueueLN("not connected");
   }
   
   //if client data available, add to Rx Queue
   #ifdef DbgMsgs_IO
      if(client.available())
      {
         uint16_t Cnt = 0;
         //Serial.printf("RxIn %d+", RxQueueUsed);
         while (client.available())
         {
            uint8_t c=client.read();
            AddCharToRxQueue(c);
            Cnt++;
         }
         //Serial.printf("%d=%d\n", Cnt, RxQueueUsed);
         if (RxQueueUsed>3000) Serial.printf("RxQueue added: %d  total: %d\n", Cnt, RxQueueUsed);
      }
   #else
      while (client.available()) AddCharToRxQueue(client.read());
   #endif
   
   if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) //Tx data available from C64
   {
      if (client.connected()) //send Tx data to host
      {
         //Printf_dbg("send %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         client.print((char)SwiftTxBuf);
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
      }
      else  //off-line, at commands, etc..................................
      {
         //Printf_dbg("echo %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         AddCharToRxQueue(SwiftTxBuf); //echo it
         
         TxMsg[TxMsgOffset++] = SwiftTxBuf;
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
         if (TxMsg[TxMsgOffset-1] == 13 || TxMsgOffset == TxMsgSize) //return hit or max size
         {
            TxMsg[TxMsgOffset-1] = 0; //terminate it
            //Printf_dbg("TxMsg: %s\n", TxMsg);
            ProcessATCommand();
            AddASCIIStrToRxQueueLN("ok\r\n");
            TxMsgOffset = 0;
         }
      }
   }

   CheckSendRx();
}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}


