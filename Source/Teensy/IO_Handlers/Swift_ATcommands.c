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

enum enATResponseCodes
{  //http://www.messagestick.net/modem/Hayes_Ch1-2.html
   //match spec and Verbose_RCs below
   ATRC_OK          , // 0
   ATRC_CONNECT     , // 1
   ATRC_RING        , // 2
   ATRC_NO_CARRIER  , // 3
   ATRC_ERROR       , // 4
   ATRC_CONNECT_1200, // 5
   ATRC_NO_DIALTONE , // 6
   ATRC_BUSY        , // 7
   ATRC_NO_ANSWER   , // 8
   NumATResponseCodes
};

#define MaxATcmdLength   20

struct stcATCommand
{
  char Command[MaxATcmdLength];
  enATResponseCodes (*Function)(char*); 
};

FLASHMEM void SendATresponse(enATResponseCodes ResponseCode)
{ 
   const char Verbose_RCs[NumATResponseCodes][15] = 
   {//match enATResponseCodes:
      "OK"          , // 0
      "CONNECT"     , // 1
      "RING"        , // 2
      "NO_CARRIER"  , // 3
      "ERROR"       , // 4
      "CONNECT_1200", // 5
      "NO_DIALTONE" , // 6
      "BUSY"        , // 7
      "NO_ANSWER"   , // 8
   };
   
   AddToPETSCIIStrToRxQueueLN(Verbose_RCs[ResponseCode]);
}

FLASHMEM enATResponseCodes CheckEthConn()
{
   if (Ethernet.hardwareStatus() == EthernetNoHardware) 
   {
      AddToPETSCIIStrToRxQueueLN(" HW was not found");
      return ATRC_NO_DIALTONE;
   }
   if (Ethernet.linkStatus() == LinkOFF) 
   {
      AddToPETSCIIStrToRxQueueLN(" Cable is not connected");
      return ATRC_NO_CARRIER;
   }
   return ATRC_OK;
}

FLASHMEM enATResponseCodes StrToIPToEE(char* Arg, uint8_t EEPaddress)
{  // Arg is an IP address string, decode it and write it to EEPROM at EEPaddress
   IPAddress ip;   
   
   AddToPETSCIIStrToRxQueueLN(" IP Addr");
   if (!inet_aton(Arg, ip)) 
   {
      AddInvalidFormatToRxQueueLN();
      return ATRC_ERROR;
   }
   
   EEPROM.put(EEPaddress, (uint32_t)ip);
   AddUpdatedToRxQueueLN();
   AddToPETSCIIStrToRxQueue("to ");
   AddIPaddrToRxQueueLN(ip);
   
   return ATRC_OK;
}


// Swiftlink AT Commands

FLASHMEM enATResponseCodes AT_BROWSE(char* CmdArg)
{  //ATBROWSE   Enter Browser mode
   SendBrowserCommandsImmediate();
   UnPausePage();
   BrowserMode = true;
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_DT(char* CmdArg)
{  //ATDT<HostName>:<Port>   Connect telnet
   uint16_t  Port = 6400; //default if not defined
   char* Delim = strstr(CmdArg, ":");


   SwiftRegStatus |= SwiftStatusDCD; //disconnected, in case CD is already asserted prior to ATDT
   //ConnectedToHost = false;
   
   if (Delim != NULL) //port defined, read it
   {
      Delim[0]=0; //terminate host name
      Port = atol(Delim+1);
      //if (Port==0) AddToPETSCIIStrToRxQueueLN("invalid port #");
   }
   
   char Buf[100];
   sprintf(Buf, "Trying %s\r\non port %d...", CmdArg, Port);
   AddToPETSCIIStrToRxQueueLN(Buf);
   FlushRxQueue();
   //Printf_dbg("Host name: %s  Port: %d\n", CmdArg, Port);
   
   enATResponseCodes resp = CheckEthConn();
   if (resp!=ATRC_OK) return resp;
   
   if (!client.connect(CmdArg, Port)) 
   {
      //AddToPETSCIIStrToRxQueueLN("Failed!");
      return ATRC_NO_ANSWER;
   }
   //AddToPETSCIIStrToRxQueueLN("Done");
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_C(char* CmdArg)
{  //ATC: Connect Ethernet
   AddToPETSCIIStrToRxQueue("Connect Ethernet ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddToPETSCIIStrToRxQueueLN("via DHCP.");
   else AddToPETSCIIStrToRxQueueLN("using Static IP.");
   FlushRxQueue();
   
   if (!EthernetInit())
   {
      //AddToPETSCIIStrToRxQueueLN("Failed!");
      enATResponseCodes resp = CheckEthConn();
      if(resp==ATRC_OK) resp = ATRC_ERROR;
      return resp;
   }

   //AddToPETSCIIStrToRxQueueLN("Done");
   byte mac[6]; 
   Ethernet.MACAddress(mac);
   AddMACToRxQueueLN(mac);
   
   uint32_t ip = Ethernet.localIP();
   AddToPETSCIIStrToRxQueue(" Local IP: ");
   AddIPaddrToRxQueueLN(ip);

   ip = Ethernet.subnetMask();
   AddToPETSCIIStrToRxQueue(" Subnet Mask: ");
   AddIPaddrToRxQueueLN(ip);

   ip = Ethernet.gatewayIP();
   AddToPETSCIIStrToRxQueue(" Gateway IP: ");
   AddIPaddrToRxQueueLN(ip);
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_S(char* CmdArg)
{
   uint32_t ip;
   uint8_t  mac[6];
   
   AddToPETSCIIStrToRxQueueLN("General Settings:");

   EEPreadNBuf(eepAdMyMAC, mac, 6);
   AddMACToRxQueueLN(mac);
   
   AddDHCPEnDisToRxQueueLN();
   
   AddToPETSCIIStrToRxQueueLN("DHCP only:");    
   AddDHCPTimeoutToRxQueueLN();
   AddDHCPRespTOToRxQueueLN();
   
   AddToPETSCIIStrToRxQueueLN("Static only:");    
   AddToPETSCIIStrToRxQueue(" My IP: ");
   EEPROM.get(eepAdMyIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddToPETSCIIStrToRxQueue(" DNS IP: ");
   EEPROM.get(eepAdDNSIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddToPETSCIIStrToRxQueue(" Gateway IP: ");
   EEPROM.get(eepAdGtwyIP, ip);
   AddIPaddrToRxQueueLN(ip);

   AddToPETSCIIStrToRxQueue(" Subnet Mask: ");
   EEPROM.get(eepAdMaskIP, ip);
   AddIPaddrToRxQueueLN(ip);

   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_RNDMAC(char* CmdArg)
{
   uint8_t mac[6];   
   
   AddToPETSCIIStrToRxQueueLN("Random MAC Addr");
   for(uint8_t octnum =0; octnum<6; octnum++) mac[octnum]=random(0,256);
   mac[0] &= 0xFE; //Unicast
   mac[0] |= 0x02; //Local Admin
   EEPwriteNBuf(eepAdMyMAC, mac, 6);
   AddUpdatedToRxQueueLN();
   AddMACToRxQueueLN(mac);
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_MAC(char* CmdArg)
{
   uint8_t octnum =1;
   uint8_t mac[6];   
   
   AddToPETSCIIStrToRxQueueLN("MAC Addr");
   mac[0]=strtoul(CmdArg, NULL, 16);
   while(octnum<6)
   {
      CmdArg=strchr(CmdArg, ':');
      if(CmdArg==NULL)
      {
         AddInvalidFormatToRxQueueLN();
         return ATRC_ERROR;
      }
      mac[octnum++]=strtoul(++CmdArg, NULL, 16);     
   }
   EEPwriteNBuf(eepAdMyMAC, mac, 6);
   AddUpdatedToRxQueueLN();
   AddMACToRxQueueLN(mac);
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_DHCP(char* CmdArg)
{
   if(CmdArg[1]!=0 || CmdArg[0]<'0' || CmdArg[0]>'1')
   {
      AddInvalidFormatToRxQueueLN();
      return ATRC_ERROR;
   }
   EEPROM.write(eepAdDHCPEnabled, CmdArg[0]-'0');
   AddUpdatedToRxQueueLN();
   AddDHCPEnDisToRxQueueLN();
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_DHCPTIME(char* CmdArg)
{
   uint16_t NewTime = atol(CmdArg);
   if(NewTime==0)
   {
      AddInvalidFormatToRxQueueLN();
      return ATRC_ERROR;
   }   
   EEPROM.put(eepAdDHCPTimeout, NewTime);
   AddUpdatedToRxQueueLN();
   AddDHCPTimeoutToRxQueueLN();
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_DHCPRESP(char* CmdArg)
{
   uint16_t NewTime = atol(CmdArg);
   if(NewTime==0)
   {
      AddInvalidFormatToRxQueueLN();
      return ATRC_ERROR;
   }
   EEPROM.put(eepAdDHCPRespTO, NewTime);
   AddUpdatedToRxQueueLN();
   AddDHCPRespTOToRxQueueLN();
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_MYIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("My");
   return StrToIPToEE(CmdArg, eepAdMyIP);
}

FLASHMEM enATResponseCodes AT_DNSIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("DNS");
   return StrToIPToEE(CmdArg, eepAdDNSIP);
}

FLASHMEM enATResponseCodes AT_GTWYIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Gateway");
   return StrToIPToEE(CmdArg, eepAdGtwyIP);
}

FLASHMEM enATResponseCodes AT_MASKIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Subnet Mask");
   return StrToIPToEE(CmdArg, eepAdMaskIP);
}

FLASHMEM enATResponseCodes AT_DEFAULTS(char* CmdArg)
{
   AddUpdatedToRxQueueLN();
   SetEthEEPDefaults();
   return AT_S(NULL);
}

FLASHMEM enATResponseCodes AT_HELP(char* CmdArg)
{  //                      1234567890123456789012345678901234567890
   AddToPETSCIIStrToRxQueueLN("General AT Commands:");
   AddToPETSCIIStrToRxQueueLN(" AT?   This help menu");
   AddToPETSCIIStrToRxQueueLN(" AT    Ping");
   AddToPETSCIIStrToRxQueueLN(" ATC   Connect Ethernet, display info");
   AddToPETSCIIStrToRxQueueLN(" ATDT<HostName>:<Port>  Connect to host");
   AddToPETSCIIStrToRxQueueLN(" ATBROWSE  Enter Web Browser");

   AddToPETSCIIStrToRxQueueLN("Modify saved parameters:");
   AddToPETSCIIStrToRxQueueLN(" AT+S  Display stored Ethernet settings");
   AddToPETSCIIStrToRxQueueLN(" AT+DEFAULTS  Set defaults for all ");
   AddToPETSCIIStrToRxQueueLN(" AT+RNDMAC  MAC address to random value");
   AddToPETSCIIStrToRxQueueLN(" AT+MAC=<XX:XX:XX:XX:XX:XX>  Set MAC");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCP=<0:1>  DHCP On(1)/Off(0)");

   AddToPETSCIIStrToRxQueueLN("DHCP mode only: ");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCPTIME=<D>  DHCP Timeout in mS");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCPRESP=<D>  DHCP Response Timeout");

   AddToPETSCIIStrToRxQueueLN("Static mode only: ");
   AddToPETSCIIStrToRxQueueLN(" AT+MYIP=<D.D.D.D>   Local IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+DNSIP=<D.D.D.D>  DNS IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+GTWYIP=<D.D.D.D> Gateway IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+MASKIP=<D.D.D.D> Subnet Mask");

   AddToPETSCIIStrToRxQueueLN("When in connected/on-line mode:");
   AddToPETSCIIStrToRxQueueLN(" +++   Disconnect from host");
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_Info(char* CmdArg)
{ 
   AddToPETSCIIStrToRxQueue("TeensyROM\rFirmware ");
   AddToPETSCIIStrToRxQueueLN(strVersionNumber);
   return ATRC_OK;
}

FLASHMEM enATResponseCodes AT_Echo(char* CmdArg)
{ 
   if(CmdArg[1]!=0 || CmdArg[0]<'0' || CmdArg[0]>'1')
   {
      AddInvalidFormatToRxQueueLN();
      return ATRC_ERROR;
   }
   EchoOn = (CmdArg[0]=='1');
   return ATRC_OK;
}

FLASHMEM enATResponseCodes ProcessATCommand()
{
   char* CmdMsg = TxMsg; //local pointer for manipulation
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
      "i"         , &AT_Info,
      "e"         , &AT_Echo,
   };
      
      
   if (strstr(CmdMsg, "at")!=CmdMsg)
   {
      AddToPETSCIIStrToRxQueueLN("AT not found");
      return ATRC_ERROR;
   }
   CmdMsg+=2; //move past the AT
   if(CmdMsg[0]==0)  //ping
   {
      return ATRC_OK;
   }
   
   uint16_t Num = 0;
   while(Num < sizeof(ATCommands)/sizeof(ATCommands[0]))
   {
      if (strstr(CmdMsg, ATCommands[Num].Command) == CmdMsg)
      {
         CmdMsg+=strlen(ATCommands[Num].Command); //move past the Command
         while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after AT command
         return ATCommands[Num].Function(CmdMsg);
      }
      Num++;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddToPETSCIIStrToRxQueue("unknown command: ");
   AddToPETSCIIStrToRxQueueLN(TxMsg);
   return ATRC_ERROR;
}
