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


// Swiftlink AT Commands


FLASHMEM void AT_BROWSE(char* CmdArg)
{  //ATBROWSE   Enter Browser mode
   AddBrowserCommandsToRxQueue();
   UsedPageLinkBuffs = 0;
   BrowserMode = true;
}

FLASHMEM void AT_DT(char* CmdArg)
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

FLASHMEM void AT_C(char* CmdArg)
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

FLASHMEM void AT_S(char* CmdArg)
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

FLASHMEM void AT_RNDMAC(char* CmdArg)
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

FLASHMEM void AT_MAC(char* CmdArg)
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

FLASHMEM void AT_DHCP(char* CmdArg)
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

FLASHMEM void AT_DHCPTIME(char* CmdArg)
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

FLASHMEM void AT_DHCPRESP(char* CmdArg)
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

FLASHMEM void AT_MYIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("My");
   StrToIPToEE(CmdArg, eepAdMyIP);
}

FLASHMEM void AT_DNSIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("DNS");
   StrToIPToEE(CmdArg, eepAdDNSIP);
}

FLASHMEM void AT_GTWYIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("Gateway");
   StrToIPToEE(CmdArg, eepAdGtwyIP);
}

FLASHMEM void AT_MASKIP(char* CmdArg)
{
   AddASCIIStrToRxQueue("Subnet Mask");
   StrToIPToEE(CmdArg, eepAdMaskIP);
}

FLASHMEM void AT_DEFAULTS(char* CmdArg)
{
   AddUpdatedToRxQueueLN();
   SetEthEEPDefaults();
   AT_S(NULL);
}

FLASHMEM void AT_HELP(char* CmdArg)
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
