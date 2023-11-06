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
   SendBrowserCommandsImmediate();
   UnPausePage();
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
      //if (Port==0) AddToPETSCIIStrToRxQueueLN("invalid port #");
   }
   
   char Buf[100];
   sprintf(Buf, "Trying %s\r\non port %d...", CmdArg, Port);
   AddToPETSCIIStrToRxQueueLN(Buf);
   FlushRxQueue();
   //Printf_dbg("Host name: %s  Port: %d\n", CmdArg, Port);
   
   if (client.connect(CmdArg, Port)) AddToPETSCIIStrToRxQueueLN("Done");
   else AddToPETSCIIStrToRxQueueLN("Failed!");
}

FLASHMEM void AT_C(char* CmdArg)
{  //ATC: Connect Ethernet
   AddToPETSCIIStrToRxQueue("Connect Ethernet ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddToPETSCIIStrToRxQueue("via DHCP...");
   else AddToPETSCIIStrToRxQueue("using Static...");
   FlushRxQueue();
   
   if (EthernetInit()==true)
   {
      AddToPETSCIIStrToRxQueueLN("Done");
      
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
   }
   else
   {
      AddToPETSCIIStrToRxQueueLN("Failed!");
      if (Ethernet.hardwareStatus() == EthernetNoHardware) AddToPETSCIIStrToRxQueueLN(" HW was not found");
      else if (Ethernet.linkStatus() == LinkOFF) AddToPETSCIIStrToRxQueueLN(" Cable is not connected");
   }
}

FLASHMEM void AT_S(char* CmdArg)
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

}

FLASHMEM void AT_RNDMAC(char* CmdArg)
{
   uint8_t mac[6];   
   
   AddToPETSCIIStrToRxQueueLN("Random MAC Addr");
   for(uint8_t octnum =0; octnum<6; octnum++) mac[octnum]=random(0,256);
   mac[0] &= 0xFE; //Unicast
   mac[0] |= 0x02; //Local Admin
   EEPwriteNBuf(eepAdMyMAC, mac, 6);
   AddUpdatedToRxQueueLN();
   AddMACToRxQueueLN(mac);
}

FLASHMEM void AT_MAC(char* CmdArg)
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
         return;
      }
      mac[octnum++]=strtoul(++CmdArg, NULL, 16);     
   }
   EEPwriteNBuf(eepAdMyMAC, mac, 6);
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
   AddToPETSCIIStrToRxQueue("My");
   StrToIPToEE(CmdArg, eepAdMyIP);
}

FLASHMEM void AT_DNSIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("DNS");
   StrToIPToEE(CmdArg, eepAdDNSIP);
}

FLASHMEM void AT_GTWYIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Gateway");
   StrToIPToEE(CmdArg, eepAdGtwyIP);
}

FLASHMEM void AT_MASKIP(char* CmdArg)
{
   AddToPETSCIIStrToRxQueue("Subnet Mask");
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
   AddToPETSCIIStrToRxQueueLN("General AT Commands:");
   AddToPETSCIIStrToRxQueueLN(" AT?   This help menu");
   AddToPETSCIIStrToRxQueueLN(" AT    Ping");
   AddToPETSCIIStrToRxQueueLN(" ATC   Connect Ethernet, display info");
   AddToPETSCIIStrToRxQueueLN(" ATDT<HostName>:<Port>  Connect to host");

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
      AddToPETSCIIStrToRxQueueLN("AT not found");
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
   AddToPETSCIIStrToRxQueue("unknown command: ");
   AddToPETSCIIStrToRxQueueLN(TxMsg);
}
