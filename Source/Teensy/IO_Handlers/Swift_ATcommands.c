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

enum ATRespCode
{  //http://www.messagestick.net/modem/Hayes_Ch1-2.html
   //http://www.messagestick.net/modem/hayes_modem.html
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
  ATRespCode (*Function)(char*); 
};

FLASHMEM void SendATresponse(ATRespCode ResponseCode)
{ 
   const char Verbose_RCs[NumATResponseCodes][15] = 
   {//match enum ATRespCode
    //sent in ASCII
      "OK"          , // 0
      "CONNECT"     , // 1
      "RING"        , // 2
      "NO CARRIER"  , // 3
      "ERROR"       , // 4
      "CONNECT 1200", // 5
      "NO DIALTONE" , // 6
      "BUSY"        , // 7
      "NO ANSWER"   , // 8
   };
   
   if (Verbose) 
   {
      // Send as upper case ASCII (which is the same as lower case PETSCII)
      AddRawStrToRxQueue(Verbose_RCs[ResponseCode]);
      AddRawCharToRxQueue('\r');
   }
   else 
   {
      char buf[10];
      sprintf(buf, "%d", ResponseCode);
      AddToPETSCIIStrToRxQueueLN(buf);
   }
}

FLASHMEM bool VerifySingleBinArg(const char* CmdArg)
{
   if(CmdArg[1]!=0 || CmdArg[0]<'0' || CmdArg[0]>'1')
   {
      if (Verbose) AddInvalidFormatToRxQueueLN();
      return false;
   }
   return true;
}

void AddVerboseToPETSCIIStrToRxQueueLN(const char* s)
{
   if (Verbose) AddToPETSCIIStrToRxQueueLN(s);
}

void AddVerboseToPETSCIIStrToRxQueue(const char* s)
{
   if (Verbose) AddToPETSCIIStrToRxQueue(s);
}

FLASHMEM ATRespCode StrToIPToEE(char* Arg, uint8_t EEPaddress)
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

FLASHMEM ATRespCode AT_BROWSE(char* CmdArg)
{  //ATBROWSE   Enter Browser mode
   EthernetInit();
   SendBrowserCommandsImmediate();
   UnPausePage();
   BrowserMode = true;
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_C(char* CmdArg)
{  //ATC: Connect Ethernet
   AddVerboseToPETSCIIStrToRxQueue("Connect Ethernet ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddVerboseToPETSCIIStrToRxQueueLN("via DHCP.");
   else AddVerboseToPETSCIIStrToRxQueueLN("using Static IP.");
   FlushRxQueue();
   
   if (!EthernetInit())
   {
      //Was CheckEthConn()...
      if (Ethernet.hardwareStatus() == EthernetNoHardware) 
      {
         AddVerboseToPETSCIIStrToRxQueueLN(" HW was not found");
         //return ATRC_NO_DIALTONE;
      }
      else if (Ethernet.linkStatus() == LinkOFF) 
      {
         AddVerboseToPETSCIIStrToRxQueueLN(" Cable not connected");
         //return ATRC_NO_DIALTONE;
      }
      
      return ATRC_NO_DIALTONE;
   }

   if (Verbose)
   {
      //AddToPETSCIIStrToRxQueueLN("Done");
      byte mac[6]; 
      Ethernet.MACAddress(mac); //read back mac address
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
   }
   
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_DT(char* CmdArg)
{  //ATDT<HostName>:<Port>   Connect telnet
   uint16_t  Port = 6400; //default if not defined

   SwiftRegStatus |= SwiftStatusDCD; //disconnected, in case CD is already asserted prior to ATDT(?)
   //ConnectedToHost = false;
   
   //initialize ethernet connection
   ATRespCode resp = AT_C(NULL);
   if (resp!=ATRC_OK) return resp;
 
   while(*CmdArg=='\"') CmdArg++; //Remove leading quote(s)
   
   char* Delim = strstr(CmdArg, ":");
   if (Delim != NULL) //port defined, read it
   {
      Delim[0]=0; //terminate host name
      Port = atol(Delim+1);
      //if (Port==0) AddToPETSCIIStrToRxQueueLN("invalid port #");
   }

   uint16_t cmdlen = strlen(CmdArg);
   if (cmdlen) if (CmdArg[cmdlen-1]=='\"') CmdArg[cmdlen-1]=0; //Remove trailing quote
   
   char Buf[100];
   sprintf(Buf, "Trying \"%s\"\r\n on port %d...", CmdArg, Port);
   AddVerboseToPETSCIIStrToRxQueueLN(Buf);
   FlushRxQueue();
   //Printf_dbg_sw("Host name: %s  Port: %d\n", CmdArg, Port);
   
   //resp = CheckEthConn();
   //if (resp!=ATRC_OK) return resp;
   
   if (!client.connect(CmdArg, Port)) return ATRC_NO_ANSWER;

   return ATRC_CONNECT;
}

FLASHMEM ATRespCode AT_S(char* CmdArg)
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

FLASHMEM ATRespCode AT_RNDMAC(char* CmdArg)
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

FLASHMEM ATRespCode AT_MAC(char* CmdArg)
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

FLASHMEM ATRespCode AT_DHCP(char* CmdArg)
{
   if(!VerifySingleBinArg(CmdArg)) return ATRC_ERROR;

   EEPROM.write(eepAdDHCPEnabled, CmdArg[0]-'0');
   AddUpdatedToRxQueueLN();
   AddDHCPEnDisToRxQueueLN();
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_DHCPTIME(char* CmdArg)
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

FLASHMEM ATRespCode AT_DHCPRESP(char* CmdArg)
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

FLASHMEM ATRespCode AT_MYIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("My");
   return StrToIPToEE(CmdArg, eepAdMyIP);
}

FLASHMEM ATRespCode AT_DNSIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("DNS");
   return StrToIPToEE(CmdArg, eepAdDNSIP);
}

FLASHMEM ATRespCode AT_GTWYIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Gateway");
   return StrToIPToEE(CmdArg, eepAdGtwyIP);
}

FLASHMEM ATRespCode AT_MASKIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Subnet Mask");
   return StrToIPToEE(CmdArg, eepAdMaskIP);
}

FLASHMEM ATRespCode AT_DEFAULTS(char* CmdArg)
{
   AddUpdatedToRxQueueLN();
   SetEthEEPDefaults();
   return AT_S(NULL);
}

FLASHMEM ATRespCode AT_HELP(char* CmdArg)
{  //                          1234567890123456789012345678901234567890
   AddToPETSCIIStrToRxQueueLN("General AT Commands:");
   AddToPETSCIIStrToRxQueueLN(" AT   Ping      ATV<0:1> Verbose On/Off");
   AddToPETSCIIStrToRxQueueLN(" AT?  This List  ATE<0:1> Echo On/Off");
   AddToPETSCIIStrToRxQueueLN(" ATI  TR ID/versionInfo");
   AddToPETSCIIStrToRxQueueLN(" ATC  Connect Ethernet, display info");
   AddToPETSCIIStrToRxQueueLN(" ATDT<HostName>:<Port>  Connect to host");
   AddToPETSCIIStrToRxQueueLN(" ATBROWSE  Enter Web Browser");

   AddToPETSCIIStrToRxQueueLN("Modify saved parameters:");
   AddToPETSCIIStrToRxQueueLN(" AT+S  Display stored Ethernet settings");
   AddToPETSCIIStrToRxQueueLN(" AT+DEFAULTS  Set defaults for all ");
   AddToPETSCIIStrToRxQueueLN(" AT+RNDMAC  MAC address to random value");
   AddToPETSCIIStrToRxQueueLN(" AT+MAC=<XX:XX:XX:XX:XX:XX>  Set MAC");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCP=<0:1>  DHCP On(1)/Off(0)");

   AddToPETSCIIStrToRxQueueLN("DHCP mode settings: ");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCPTIME=<D>  DHCP Timeout in mS");
   AddToPETSCIIStrToRxQueueLN(" AT+DHCPRESP=<D>  DHCP Response Timeout");

   AddToPETSCIIStrToRxQueueLN("Static mode settings: ");
   AddToPETSCIIStrToRxQueueLN(" AT+MYIP=<D.D.D.D>   Local IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+DNSIP=<D.D.D.D>  DNS IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+GTWYIP=<D.D.D.D> Gateway IP address");
   AddToPETSCIIStrToRxQueueLN(" AT+MASKIP=<D.D.D.D> Subnet Mask");

   AddToPETSCIIStrToRxQueueLN("When connected/on-line mode:");
   AddToPETSCIIStrToRxQueueLN(" +++   Disconnect from host");
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_Info(char* CmdArg)
{ 
   AddToPETSCIIStrToRxQueue("TeensyROM\rFirmware ");
   AddToPETSCIIStrToRxQueueLN(strVersionNumber);
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_Echo(char* CmdArg)
{ 
   if(!VerifySingleBinArg(CmdArg)) return ATRC_ERROR;

   EchoOn = (CmdArg[0]=='1');
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_Verbose(char* CmdArg)
{ 
   if(!VerifySingleBinArg(CmdArg)) return ATRC_ERROR;

   Verbose = (CmdArg[0]=='1');
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_ZSoftReset(char* CmdArg)
{ 
   //ignoring argument, would require profile support
   Verbose = true;
   EchoOn = true;
   return ATRC_OK;
}

FLASHMEM ATRespCode AT_Hook(char* CmdArg)
{ 
   //ignoring argument, supposed to be able to use this after +++ returns to command mode
   //but TR implementation of +++ drops connection at some time
   //using this dummy to return OK instead of error
   return ATRC_OK;
}


FLASHMEM ATRespCode ProcessATCommand()
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
      "v"         , &AT_Verbose,
      "z"         , &AT_ZSoftReset,
      "h"         , &AT_Hook,
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
   
   Printf_dbg_sw("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddToPETSCIIStrToRxQueue("unknown command: ");
   AddToPETSCIIStrToRxQueueLN(TxMsg);
   return ATRC_ERROR;
}
