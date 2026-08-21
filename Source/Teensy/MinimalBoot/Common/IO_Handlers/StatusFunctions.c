// MIT License
//
// Copyright (c) 2026 Travis Smith
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


// Split out of IOH_TeensyROM.c: functions dispatched via StatusFunction[], plus helpers
// used only by them. Included directly from IOH_TeensyROM.c after the extern block and
// after the few helpers shared with other files (GetCurrentFilePathName, RAM2blocks,
// FindTRMenuItem), which stay there and remain visible here via the same translation unit.
// StatusFunction[] is declared last so every entry above it is already defined --  no
// prototypes needed.

FLASHMEM void SendStrPrintfln(const char *Msg)
{
   SendMsgPrintfln(Msg); //printf style, throws warning if used as callback in EthernetInit
}

FLASHMEM void NetListenInit()
{  //called on main menu start when rpud2TRTCPListen
   NetListenEnable = EthernetInit(SendStrPrintfln);
}

FLASHMEM void ForceEthInit()
{  //called on settings general info
   if(ForceEthernetInit(SendStrPrintfln)) SendMsgPrintfln("Success!\n");
   else SendMsgPrintfln("Failed!\n");
}

FLASHMEM void SetRTCfromNet()
{
   //called from TR Startup or settings with messaging

   if (EthernetInit(SendStrPrintfln))
   {
      IPAddress ip = Ethernet.localIP();
      SendMsgPrintfln("My IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

      unsigned int localPort = 8888;       // local port to listen for UDP packets
      const char timeServer[] = "us.pool.ntp.org"; // time.nist.gov     NTP server

      udp.begin(localPort);

      const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
      byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

      SendMsgPrintfln("Updating time from: %s", timeServer);
      while (udp.parsePacket() > 0) ; // discard any previously received packets

      // send an NTP request to the time server at the given address
      // set all bytes in the buffer to 0
      memset(packetBuffer, 0, NTP_PACKET_SIZE);
      // Initialize values needed to form NTP request
      packetBuffer[0] = 0b11100011;   // LI, Version, Mode
      packetBuffer[1] = 0;     // Stratum, or type of clock
      packetBuffer[2] = 6;     // Polling Interval
      packetBuffer[3] = 0xEC;  // Peer Clock Precision
      // 8 bytes of zero for Root Delay & Root Dispersion
      packetBuffer[12]  = 49;
      packetBuffer[13]  = 0x4E;
      packetBuffer[14]  = 49;
      packetBuffer[15]  = 52;
      // all NTP fields have been given values, now send a packet requesting a timestamp:
      udp.beginPacket(timeServer, 123); // NTP requests are to port 123
      udp.write(packetBuffer, NTP_PACKET_SIZE);
      udp.endPacket();

      uint32_t beginWait = millis();
      while (millis() - beginWait < 2500)
      {
         int size = udp.parsePacket();
         if (size >= NTP_PACKET_SIZE)
         {
            udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            uint32_t secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            SendMsgPrintfln("Received NTP Response in %d mS", (millis() - beginWait));

            //time_t t = secsSince1900 - 2208988800UL + (int8_t)IO1[rwRegTimezone] * SECS_PER_HOUR/2; //timezone in 30 min increments
            time_t t = secsSince1900 - 2208988800UL; //sec since 1970, leave RTC at UTC for now, adjust when C64 asks for time
            Teensy3Clock.set(t); // set the RTC

            //setTime(t); //set/update the local time system (Not used)
            tmElements_t tm;  // time elements
            breakTime(t, tm); // break seconds down to elements
            SendMsgPrintfln("RTC Set to %d/%02d/%02d at %d:%02d:%02d UTC",tmYearToCalendar(tm.Year),tm.Month,tm.Day,tm.Hour,tm.Minute,tm.Second);

            return;
         }
      }
      SendMsgPrintfln("NTP Response timeout!");
   }
   SendMsgPrintfln("RTC not updated");
}

FLASHMEM void C64TODfromRTC()
{  //Get RTC time and update IO1 regs using CIA TOD Reg format

   //method #1, many constants used elsewhere...
   //uint32_t secsSince1970 = (uint32_t)Teensy3Clock.get(); //read the RTC time
   //IO1[rRegLastSecBCD] = DecToBCD(secsSince1970 % 60);
   //secsSince1970 = secsSince1970/60 + 30*(int8_t)IO1[rwRegTimezone]; //to  minutes, offset timezone (30 min increments)
   //IO1[rRegLastMinBCD] = DecToBCD(secsSince1970 % 60);
   //secsSince1970 = (secsSince1970/60) % 24; //to hours
   //if (secsSince1970 >= 12) IO1[rRegLastHourBCD] = 0x80 | DecToBCD(secsSince1970-12); //change to 0 based 12 hour and add pm flag
   //else IO1[rRegLastHourBCD] =DecToBCD(secsSince1970); //default to AM (bit 7 == 0)

   //better, uses existing code...
   int8_t tz = (int8_t)IO1[rwRegTimezone]; //time zone to signed
   time_t t = Teensy3Clock.get() + (int16_t)tz*30*60; //read the RTC time, offset timezone (sign extended, 30 min increments)
   tmElements_t tm;  // time elements
   breakTime(t, tm); // break seconds down to elements
   IO1[rRegLastSecBCD] = DecToBCD(tm.Second);
   IO1[rRegLastMinBCD] = DecToBCD(tm.Minute);
   if (tm.Hour >= 12) IO1[rRegLastHourBCD] = 0x80 | DecToBCD(tm.Hour-12); //change to 0 based 12 hour and add pm flag
   else IO1[rRegLastHourBCD] =DecToBCD(tm.Hour); //default to AM (bit 7 == 0)
   //Serial.printf("RTC read %d/%02d/%02d at %d:%02d:%02d (local)\n",tmYearToCalendar(tm.Year),tm.Month,tm.Day,tm.Hour,tm.Minute,tm.Second);

   //SendMsgPrintfln (not here in case of tz change)
   Printf_dbg ("TOD Time: %02x:%02x:%02x %sm\n", (IO1[rRegLastHourBCD] & 0x7f) , IO1[rRegLastMinBCD], IO1[rRegLastSecBCD], (IO1[rRegLastHourBCD] & 0x80) ? "p" : "a");
}

FLASHMEM void RTCAdjust()
{
   //Adjust RTC up/down manually

   time_t adj = 0; //default to no adjustment

   switch (IO1[wRegControl])
   {
      case rCtlRTCAdj_Hrs_Up_WAIT:
         adj = SECS_PER_HOUR;
         break;
      case rCtlRTCAdj_Hrs_Dn_WAIT:
         adj = -SECS_PER_HOUR;
         break;
      case rCtlRTCAdj_Min_Up_WAIT:
         adj = SECS_PER_MIN;
         break;
      case rCtlRTCAdj_Min_Dn_WAIT:
         adj = -SECS_PER_MIN;
         break;
      case rCtlRTCAdj_Sec_Up_WAIT:
         adj = 1;
         break;
      case rCtlRTCAdj_Sec_Dn_WAIT:
         adj = -1;
         break;
   }

   time_t t = Teensy3Clock.get() + adj; //read the RTC time, add adjustment
   Teensy3Clock.set(t); // set the RTC
   C64TODfromRTC(); //also update IO1 current time regs
}

FLASHMEM void WriteEEPROM()
{
   Printf_dbg("Wrote $%02x to EEP addr %d\n", eepDataToWrite, eepAddrToWrite);
   EEPROM.write(eepAddrToWrite, eepDataToWrite);
}

FLASHMEM void MakeBuildInfo()
{
   uint32_t serialNum = HW_OCOTP_MAC0 & 0xFFFFFF; // Read the unique 24-bit identifier from the hardware fuse
   if (serialNum < 10000000) serialNum *= 10; // Replicate the OS-X CDC-ACM driver work-around used by PJRC core
   sprintf(SerialStringBuf, "  FW: %s\r\n      %s, %s\r\n  Teensy: %luMHz  %.1fC  UID: %lu\r", strVersionNumber, __DATE__, __TIME__, (F_CPU_ACTUAL/1000000), tempmonGetTemp(), serialNum);
}

FLASHMEM void MakeIPSSBfromIP(IPAddress ip)
{
   sprintf(SerialStringBuf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

FLASHMEM void MakeIPSSBfromEEPAddr(uint32_t EEPAddress)
{
   uint32_t ip32;
   EEPROM.get(EEPAddress, ip32);
   MakeIPSSBfromIP(ip32);
}

FLASHMEM void MakeFilenameStr()
{
   //Get filename/value from EEPROM (selected in IO1[wRegControl])
   uint16_t invalU16;

   switch (IO1[wRegControl])
   {
      case rCtlMakeKernalStrWAIT:
         EEPreadStr(eepAdKERNALBinName, SerialStringBuf);
         break;
      case rCtlMakeREUStrWAIT:
         EEPreadStr(eepAdREUFilename, SerialStringBuf);
         break;
      case rCtlMakeSIDStrWAIT:
      {
         char SIDSourcePathName[MaxPathLength];
         EEPreadNBuf(eepAdDefaultSID, (uint8_t*)SIDSourcePathName, MaxPathLength); //load the source/path/name from EEPROM
         char* SIDName = SIDSourcePathName+strlen(SIDSourcePathName+1)+2;

         sprintf(SerialStringBuf, "%s:/%s/%s",
            (SIDSourcePathName[0] == rmtUSBDrive ? "USB" : (SIDSourcePathName[0] == rmtSD ? "SD" : "TR")),
            SIDSourcePathName+1, SIDName);
      }
         break;
      case rCtlMakeAutoLStrWAIT:
         EEPreadStr(eepAdAutolaunchName, SerialStringBuf);
         break;
      case rCtlMakeHotKey1WAIT...rCtlMakeHotKey5WAIT:
         EEPreadStr(eepAdHotKeyPaths + (IO1[wRegControl]-rCtlMakeHotKey1WAIT)*MaxPathLength , SerialStringBuf);
         break;

      case rCtlMakeEthMACWAIT:
      {
         uint8_t  mac[6];
         EEPreadNBuf(eepAdMyMAC, mac, 6);
         sprintf(SerialStringBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      }
         break;
      case rCtlMakeEthIPAcqTypeWAIT:
         if (EEPROM.read(eepAdDHCPEnabled)) sprintf(SerialStringBuf, "DHCP");
         else sprintf(SerialStringBuf, "Static");
         break;
      case rCtlMakeEthDHCPTOWAIT:
         EEPROM.get(eepAdDHCPTimeout, invalU16);
         sprintf(SerialStringBuf, "%dmS", invalU16);
         break;
      case rCtlMakeEthDHCPRespTOWAIT:
         EEPROM.get(eepAdDHCPRespTO, invalU16);
         sprintf(SerialStringBuf, "%dmS", invalU16);
         break;
      case rCtlMakeEthStatIPWAIT:
         MakeIPSSBfromEEPAddr(eepAdMyIP);
         break;
      case rCtlMakeEthStatDNSIPWAIT:
         MakeIPSSBfromEEPAddr(eepAdDNSIP);
         break;
      case rCtlMakeEthStatGatewWAIT:
         MakeIPSSBfromEEPAddr(eepAdGtwyIP);
         break;
      case rCtlMakeEthStatSubMskWAIT:
         MakeIPSSBfromEEPAddr(eepAdMaskIP);
         break;
       case rCtlMakeEthLocalIPWAIT:
         MakeIPSSBfromIP(Ethernet.localIP());
         break;
       case rCtlMakeEthLocalSubMskWAIT:
         MakeIPSSBfromIP(Ethernet.subnetMask());
         break;
       case rCtlMakeEthLocalGatewWAIT:
         MakeIPSSBfromIP(Ethernet.gatewayIP());
         break;
      default:
         //*SerialStringBuf = 0; //default blank
         strcpy(SerialStringBuf, "Error");
         break;
   }
   //Create printable filename for C64 display in SerialStringBuf
   const uint16_t MaxLength = 37; //  allow for indent (2) and 1 space at the end
   const uint8_t  SourceLength = 3; //first x chars for source info
   const uint8_t  SeparateLength = 3; //Num of chars in string separator

   uint16_t Length = strlen(SerialStringBuf);
   if (Length>MaxLength)
   {  //assume it's a filename if more than MaxLength
      uint16_t CharNum = SourceLength;
      //while (CharNum<SourceLength+SeparateLength) SerialStringBuf[CharNum++] = '.'; //add "..." after source media
      //SerialStringBuf[CharNum++] = '}'; //string sep char #1  |-
      //SerialStringBuf[CharNum++] = '{'; //string sep char #2  -|
      SerialStringBuf[CharNum++] = '<';
      SerialStringBuf[CharNum++] = '.';
      SerialStringBuf[CharNum++] = '>';
      uint16_t StartChar = Length+SourceLength+SeparateLength-MaxLength;
      //CharNum == SourceLength+SeparateLength
      while (StartChar<=Length) //include the term
      {
         SerialStringBuf[CharNum++] = SerialStringBuf[StartChar++];
      }
   }

   if (Length == 0) strcpy(SerialStringBuf, "<none selected>");

   //Serial.printf("\nx%sx\n", SerialStringBuf);
   //set print buffer for PrintSerialString and reset counter
   ptrSerialString = SerialStringBuf;
   StringOffset = 0;
}

FLASHMEM void UpDirectory()
{
   //non-root of Teensy, SD or USB drive only
   if(PathIsRoot()) return;

   if(IO1[rWRegCurrMenuWAIT] == rmtTeensy) MenuChange(); //back to root, only 1 dir level
   else
   {
      char * LastSlash = strrchr(DriveDirPath, '/'); //find last slash
      if (LastSlash == NULL) return;
      LastSlash[0] = 0;  //terminate it there
      if (IO1[rWRegCurrMenuWAIT] == rmtSD) LoadDirectory(&SD);
      else LoadDirectory(&firstPartition);
      IO1[rwRegCursorItemOnPg] = 0;
      IO1[rwRegPageNumber]     = 1;
   }
}

FLASHMEM void SetCursorToItemNum(uint16_t ItemNum)
{
   IO1[rwRegPageNumber] = ItemNum/MaxItemsPerPage +1;
   IO1[rwRegCursorItemOnPg] = ItemNum % MaxItemsPerPage;
   IO1[rRegNumItemsOnPage] = (NumItemsFull > IO1[rwRegPageNumber]*MaxItemsPerPage ? MaxItemsPerPage : NumItemsFull-(IO1[rwRegPageNumber]-1)*MaxItemsPerPage);
}

FLASHMEM void NextFileType(uint8_t FileType1, uint8_t FileType2)
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg] + (IO1[rwRegPageNumber]-1) * MaxItemsPerPage;
   uint16_t InitItemNum = SelItemFullIdx;
   do
   {
      if (++SelItemFullIdx >= NumItemsFull) SelItemFullIdx = 0;
      if (MenuSource[SelItemFullIdx].ItemType == FileType1 ||
          MenuSource[SelItemFullIdx].ItemType == FileType2)
      {
         SetCursorToItemNum(SelItemFullIdx);
         return;
      }
   } while (SelItemFullIdx != InitItemNum); //just 1 time through, but should stop on same initial one unless changed externally
}

FLASHMEM void LastFileType(uint8_t FileType1, uint8_t FileType2)
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg] + (IO1[rwRegPageNumber]-1) * MaxItemsPerPage;
   uint16_t InitItemNum = SelItemFullIdx;

   do
   {
      if (SelItemFullIdx == 0) SelItemFullIdx = NumItemsFull-1;
      else SelItemFullIdx--;

      if (MenuSource[SelItemFullIdx].ItemType == FileType1 ||
          MenuSource[SelItemFullIdx].ItemType == FileType2)
      {
         SetCursorToItemNum(SelItemFullIdx);
         return;
      }
   } while (SelItemFullIdx != InitItemNum); //just 1 time through, but should stop on same initial one unless changed externally
}

FLASHMEM void NextTextFile()
{
   NextFileType(rtFileTxt, rtFilePETSCII);
}

FLASHMEM void LastTextFile()
{
   LastFileType(rtFileTxt, rtFilePETSCII);
}

FLASHMEM void NextPicture()
{
   NextFileType(rtFileKla, rtFileArt);
}

FLASHMEM void LastPicture()
{
   LastFileType(rtFileKla, rtFileArt);
}

FLASHMEM void SearchForLetter()
{
   uint16_t ItemNum = 0;
   uint8_t SearchFor = IO1[wRegSearchLetterWAIT];

   //ascii upper case (toupper) matches petscii lower case ('a'=65)
   while (ItemNum < NumItemsFull)
   {
      if (toupper(MenuSource[ItemNum].Name[0]) >= SearchFor)
      {
         SetCursorToItemNum(ItemNum);
         return;
      }
      ItemNum++;
   }
}

FLASHMEM void WriteNFCTagCheck()
{
   //IO1[rwRegScratch] 1=rand dir, 0=single file
   IO1[rRegLastHourBCD] = 0; //using this reg as scratch to communicate outcome

   if (nfcState != nfcStateEnabled)
   {
      SendMsgPrintfln(" NFC not enabled/found\r");
      return;
   }

   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;

   if (!IO1[rwRegScratch] && MenuSource[SelItemFullIdx].ItemType < rtFilePrg) //single file but not executable
   {
      SendMsgPrintfln(" Invalid File Type (%d)\r", MenuSource[SelItemFullIdx].ItemType);
      return;
   }

   char PathMsg[MaxPathLength];
   GetCurrentFilePathName(PathMsg);
   SendMsgPrintfln("File Selected:\r%s\r", PathMsg);

   nfcState |= nfcStateBitDisabled; //keep if from triggering if re-using prev programmed tag
   IO1[rRegLastHourBCD] = 0xff; //checks look good!
}

FLASHMEM void WriteNFCTag()
{
   //checks have been done, ready to write tag
   //nfc polling not Enabled here

   char PathMsg[MaxPathLength];
   GetCurrentFilePathName(PathMsg);

   SendMsgPrintfln("Preparing...");
   //Serial.printf("WriteNFCTag: %s\n", PathMsg);

   nfcWriteTag(PathMsg);

   //pause for removal (in assy)
}

FLASHMEM void NFCReEnable()
{
   // nfc not currently enabled (just wrote a tag)
   nfcInit(); //this should pass, was enabled/initialized previously...
}

FLASHMEM void HotKeySetLaunch()
{
   uint8_t HotKeyNumSL = IO1[rwRegScratch];  //zero based HK num + bit 7 high = set HK, low = launch

   if (HotKeyNumSL & 0x80)
   {
      //set
      char PathFilename[MaxPathLength];

      HotKeyNumSL &= 0x7f;  // strip SL bit
      //get/print path+filename
      SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;
      IO1[rwRegScratch] = 0; //needed for GetCurrentFilePathName, also indicates success of this function
      GetCurrentFilePathName(PathFilename);
      SendMsgPrintfln("\rSet Hot Key #%d to this file:\r%s\r", HotKeyNumSL+1, PathFilename);

      if(MenuSource[SelItemFullIdx].ItemType < rtFilePrg)
      {
         SendMsgPrintfln("Invalid File Type (%d)\r\rHot Key *not* updated\r", MenuSource[SelItemFullIdx].ItemType);
         return;
      }

      EEPwriteStr(eepAdHotKeyPaths+HotKeyNumSL*MaxPathLength, PathFilename);  //set autolaunch in EEPROM:
      SendMsgPrintfln("Hot Key updated\r");
   }
   else
   {
      //launch, no messaging/not waiting...
      EEPRemoteLaunch(eepAdHotKeyPaths+HotKeyNumSL*MaxPathLength);
   }
}

FLASHMEM void KERNALPreStart()
{
   //called before BASIC init and program load/launch
#ifdef Fab04_KernalReplace

#ifdef Fab04_GlobalKernalReplace
   //check if enabled?
   InitHndlr_KERNALReplace_PreStart(); //separate function so it doesn't get called twice
#else

   //If kernal replace is selected for Special IO
   //   and not overridden by Teensy Menu:
   //Serial.println("Hi from KERNALPreStart");
   //Which IO Handler will be started?
   uint8_t NextIOHndlr = IO1[rwRegNextIOHndlr];
   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy && MenuSource[SelItemFullIdx].IOHndlrAssoc != IOH_None)
   {
      //Serial.println("IO Handler set by Teensy Menu\n");
      NextIOHndlr = MenuSource[SelItemFullIdx].IOHndlrAssoc;
   }

   if (NextIOHndlr == IOH_KernalReplace)
   {
      InitHndlr_KERNALReplace_PreStart(); //separate function so it doesn't get called twice
      //Serial.println("Bye from KERNALPreStart");
   }
   //else
   //{
   //   Serial.printf("Kernal replace not enabled (%d)\n", NextIOHndlr);
   //   delay(250);
   //}
#endif
#endif
}

FLASHMEM void TRPlusOnlyMsg()
{
   SendMsgPrintfln("\rThis feature is only\r  available on TeensyROM+\r");
}

FLASHMEM void SetREUFile()
{
   SendMsgPrintfln("Set REU File to preload\r  and/or uniquely save\r");
   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;

   char PathMsg[MaxPathLength];
   IO1[rwRegScratch] = 0;
   GetCurrentFilePathName(PathMsg);
   SendMsgPrintfln("File Selected:\r%s\r", PathMsg);

#ifdef Fab04_REU
   //check for source=teensy (not supported)
   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy)
   {
      SendMsgPrintfln("Select file from SD or USB only\r");
      return;
   }

   if (MenuSource[SelItemFullIdx].ItemType == rtDirectory)
   {
      SendMsgPrintfln("Invalid File Type\r");
      return;
   }

   //.Size isn't populated for SD/USB
   //if (MenuSource[SelItemFullIdx].Size != 8192)
   //{
   //   SendMsgPrintfln("Wrong Size (%lu), expecting 8192 Bytes\r", MenuSource[SelItemFullIdx].Size);
   //   return;
   //}

   SendMsgPrintfln("REU File selection updated.\r");

   EEPwriteStr(eepAdREUFilename, PathMsg);  //set REU path/file in EEPROM

#else
   TRPlusOnlyMsg();
#endif
}

FLASHMEM void SetKERNALBin()
{
   SendMsgPrintfln("Set KERNAL Replace Binary\r");

   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;

   char PathMsg[MaxPathLength];
   IO1[rwRegScratch] = 0;
   GetCurrentFilePathName(PathMsg);
   SendMsgPrintfln("File Selected:\r%s\r", PathMsg);

#ifdef Fab04_KernalReplace
   //check for source=teensy (not supported)
   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy)
   {
      SendMsgPrintfln("Select file from SD or USB only\r");
      return;
   }

   if (MenuSource[SelItemFullIdx].ItemType == rtDirectory)
   {
      SendMsgPrintfln("Invalid File Type\r");
      return;
   }

   //.Size isn't populated for SD/USB
   //if (MenuSource[SelItemFullIdx].Size != 8192)
   //{
   //   SendMsgPrintfln("Wrong Size (%lu), expecting 8192 Bytes\r", MenuSource[SelItemFullIdx].Size);
   //   return;
   //}

   SendMsgPrintfln("KERNAL Binary selection updated:\r  * Enable via Settings menu/Special IO\r");

   EEPwriteStr(eepAdKERNALBinName, PathMsg);  //set Kernal path in EEPROM

#else
   TRPlusOnlyMsg();
#endif
}

FLASHMEM void SetAutoLaunch()
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;

   char PathMsg[MaxPathLength];
   IO1[rwRegScratch] = 0;
   GetCurrentFilePathName(PathMsg);
   SendMsgPrintfln("File Selected:\r%s\r", PathMsg);

   if(MenuSource[SelItemFullIdx].ItemType < rtFilePrg)
   {
      SendMsgPrintfln("Invalid File Type (%d)\r\rAuto Launch *not* updated\r", MenuSource[SelItemFullIdx].ItemType);
      return;
   }

   SendMsgPrintfln("Auto Launch file updated\r  * Currently %sabled\r  * See Settings menu to enable/disable\r", ((EEPROM.read(eepAdPwrUpDefaults2) & rpud2TRAutoLaunch) ? "En":"Dis"));

   EEPwriteStr(eepAdAutolaunchName, PathMsg);  //set autolaunch filename in EEPROM:

}

FLASHMEM void ClearAutoLaunch()
{  //no longer used (old settings menu)
   uint newval = EEPROM.read(eepAdPwrUpDefaults2) & ~rpud2TRAutoLaunch;  //disable auto Launch
   EEPROM.write(eepAdPwrUpDefaults2, newval);
}

FLASHMEM void SetBackgroundSID()
{
   EEPwriteNBuf(eepAdDefaultSID, (uint8_t*)LatestSIDLoaded, MaxPathLength); //write the source/path/name to EEPROM
}

FLASHMEM void LoadMainSIDforXfer()
{
   //Load EEPROM default SID into TR RAM and prep for transfer
   //if missing, load default
   //Set XferImage and XferSize

   EEPreadNBuf(eepAdDefaultSID, (uint8_t*)LatestSIDLoaded, MaxPathLength); //load the source/path/name from EEPROM
   char* LatestSIDName = LatestSIDLoaded+strlen(LatestSIDLoaded+1)+2;
   Printf_dbg("Sel SID: %d %s / %s\n", LatestSIDLoaded[0], LatestSIDLoaded+1, LatestSIDName);

   if (LatestSIDLoaded[0] != rmtTeensy) // SD or USB
   {
      StructMenuItem MyMenuItem;
      FS *sourceFS = &firstPartition;
      if(LatestSIDLoaded[0] == rmtSD)
      {
         sourceFS = &SD;
         SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected
      }
      else USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up

      MyMenuItem.Name = LatestSIDName;
      MyMenuItem.ItemType = rtFileSID;

      if(!LoadFile(sourceFS, LatestSIDLoaded+1, &MyMenuItem))
      { //error, load default from TR
         Printf_dbg("Ld Err, Default SID\n");
         LatestSIDLoaded[0] = DefSIDSource;
         strcpy(LatestSIDLoaded+1, DefSIDPath);
         LatestSIDName = LatestSIDLoaded+strlen(DefSIDPath)+2;
         strcpy(LatestSIDName, DefSIDName);
      }
      else
      {
         XferSize = MyMenuItem.Size;
      }
   }

   if (LatestSIDLoaded[0] == rmtTeensy)
   {
      int16_t MenuNum;
      StructMenuItem* DefSIDTRMenu = TeensyROMMenu;  //default to root menu
      uint16_t NumMenuItems = sizeof(TeensyROMMenu)/sizeof(StructMenuItem);

      if(strcmp(LatestSIDLoaded+1, "/") !=0 )
      {//find dir menu
         MenuNum = FindTRMenuItem(DefSIDTRMenu, NumMenuItems, LatestSIDLoaded+1);
         if(MenuNum<0)
         {
            Printf_dbg("No SID Dir\n");
            //empty fields????  Shouldn't happen unless compile change
            return;
         }
         DefSIDTRMenu = (StructMenuItem*)TeensyROMMenu[MenuNum].Code_Image;
         NumMenuItems = TeensyROMMenu[MenuNum].Size/sizeof(StructMenuItem);
      }
      //Printf_dbg("SID Dir#%d, %d items\n", MenuNum, NumMenuItems);

      //find SID name
      MenuNum = FindTRMenuItem(DefSIDTRMenu, NumMenuItems, LatestSIDName);
      if(MenuNum<0)
      {
         Printf_dbg("No SID Name\n");
         //empty fields????  Shouldn't happen unless compile change
         return;
      }
      //Printf_dbg("SID #%d\n", MenuNum);
      XferSize = DefSIDTRMenu[MenuNum].Size;
      memcpy(RAM_Image, DefSIDTRMenu[MenuNum].Code_Image, XferSize);
   }

   //Printf_dbg("Load SID: %d %s / %s\n", LatestSIDLoaded[0], LatestSIDLoaded+1, LatestSIDName);
   XferImage = RAM_Image;
   ParseSIDHeader(LatestSIDName);
}

#ifdef Fab04_FullDMACapable
#define TestPageSize   256
FLASHMEM bool TestDMAPage(uint16_t Address, uint8_t BytePat)
{
   uint8_t PageBuf[TestPageSize];

   //SendMsgPrintfln(" Testing $%02xxx w/ $%02x", (Address >> 8), BytePat);
   //PerformDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer, uint32_t Length, bool FixC64Addr)
   memset(PageBuf, BytePat, TestPageSize);
   PerformDMA(false, Address, PageBuf, TestPageSize, false); //Write the buffer
   CloseDMA();
   PerformDMA(true, Address, PageBuf, TestPageSize, false);  //Read back
   CloseDMA();
   for(uint16_t ByteNum=0; ByteNum<TestPageSize; ByteNum++)
      if (PageBuf[ByteNum] != BytePat)
      {
         SendMsgPrintfln(" Miscompare at $%04x: Exp $%02x, Rd $%02x", Address+ByteNum, BytePat, PageBuf[ByteNum]);
         return false;
      }
   //SendMsgPrintf(" OK");
   return true;
}
#endif

FLASHMEM void ExpPortDMA()
{
   IO1[rwRegScratch] = 0; //default fail

#ifdef Fab04_FullDMACapable
   NVIC_DISABLE_IRQ(IRQ_ENET); //disable ethernet interrupt when testing expansion port
   NVIC_DISABLE_IRQ(IRQ_PIT);

//Walking ones
   SendMsgPrintfln("Walking ones address Test");
   //tests addresses 0002,0004,0008,
   //           0010,0020,0040,0080,
   //           0100,0200,0400,0800,
   //           1000,2000,4000,8000,
   uint8_t OrigValues[16]; //Place to store RAM values for restoration later
   uint8_t FirstAddrBit =1; //skipping A0 as $0001 is CPU ROM switch control and not reliable as RAM
   //read/store original values:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      PerformDMA(true, (1<<AddrBit), &OrigValues[AddrBit], 1, false);  //Read back
      CloseDMA();
   }
   //write address bit num as data:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   //for(uint8_t AddrBit=15; AddrBit>=FirstAddrBit; AddrBit--)
   {
      PerformDMA(false, (1<<AddrBit), &AddrBit, 1, false); //Write the buffer
      CloseDMA();
   }
   //read back for errors:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      uint8_t ReadVal;
      PerformDMA(true, (1<<AddrBit), &ReadVal, 1, false);  //Read back
      CloseDMA();
      if (ReadVal != AddrBit)
      {
         //write back original values
         for(uint8_t AddrBitA=0; AddrBitA<16; AddrBitA++)
         {
            PerformDMA(false, (1<<AddrBitA), &OrigValues[AddrBitA], 1, false); //Write the buffer
            CloseDMA();
         }
         SendMsgPrintfln(" Miscompare at $%04x: Exp $%02x, Rd $%02x", (1<<AddrBit), AddrBit, ReadVal);
         return;
      }
      //causes miscompare at $0400 (screen memory) if text scrolls (Walking Ones only)
      //SendMsgPrintfln(" $%04x(A%02d): $%02x", (1<<AddrBit), AddrBit, AddrBit);
   }
   //write back original values
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      PerformDMA(false, (1<<AddrBit), &OrigValues[AddrBit], 1, false); //Write the buffer
      CloseDMA();
   }
   SendMsgPrintf(" OK");


//Cascading ones
   SendMsgPrintfln("Cascading ones address Test");
   //tests addresses 0003,0007,000f,
   //           001f,003f,007f,00ff,
   //           01ff,03ff,07ff,0fff,
   //           1fff,3fff,7fff

   FirstAddrBit =2; //skipping A0 as $0001 is CPU ROM switch control and not reliable as RAM
   //read/store original values:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      PerformDMA(true, (1<<AddrBit)-1, &OrigValues[AddrBit], 1, false);  //Read back
      CloseDMA();
   }
   //write address bit num as data:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   //for(uint8_t AddrBit=15; AddrBit>=FirstAddrBit; AddrBit--)
   {
      PerformDMA(false, (1<<AddrBit)-1, &AddrBit, 1, false); //Write the buffer
      CloseDMA();
   }
   //read back for errors:
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      uint8_t ReadVal;
      PerformDMA(true, (1<<AddrBit)-1, &ReadVal, 1, false);  //Read back
      CloseDMA();
      if (ReadVal != AddrBit)
      {
         //write back original values
         for(uint8_t AddrBitA=0; AddrBitA<16; AddrBitA++)
         {
            PerformDMA(false, (1<<AddrBitA)-1, &OrigValues[AddrBitA], 1, false); //Write the buffer
            CloseDMA();
         }
         SendMsgPrintfln(" Miscompare at $%04x: Exp $%02x, Rd $%02x", (1<<AddrBit)-1, AddrBit, ReadVal);
         return;
      }
      //SendMsgPrintfln(" $%04x(A%02d): $%02x", (1<<AddrBit)-1, AddrBit, AddrBit);
   }
   //write back original values
   for(uint8_t AddrBit=FirstAddrBit; AddrBit<16; AddrBit++)
   {
      PerformDMA(false, (1<<AddrBit)-1, &OrigValues[AddrBit], 1, false); //Write the buffer
      CloseDMA();
   }
   SendMsgPrintf(" OK");


//DMA Page R/W
   SendMsgPrintfln("DMA Page Read/Write Tests");
   if (!TestDMAPage(0xc000, 0x55)) return;
   if (!TestDMAPage(0xc000, 0xaa)) return;
   if (!TestDMAPage(0xc000, 0x00)) return;
   if (!TestDMAPage(0xc000, 0xff)) return;

   if (!TestDMAPage(0x4000, 0xc3)) return;
   if (!TestDMAPage(0x4000, 0x3c)) return;
   if (!TestDMAPage(0x5000, 0x4b)) return;
   if (!TestDMAPage(0x6000, 0xb4)) return;
   if (!TestDMAPage(0x7000, 0x77)) return;
   if (!TestDMAPage(0x8000, 0x88)) return;

   if (!TestDMAPage(0x3f00, 0xaa)) return;
   if (!TestDMAPage(0x3f00, 0x55)) return;
   if (!TestDMAPage(0x3f00, 0xff)) return;
   if (!TestDMAPage(0x3f00, 0x00)) return;
   SendMsgPrintf(" OK");


//IRQ
   SendMsgPrintfln("IRQ Test");
   IO1[wRegIRQNMITest] = 0;
   uint32_t StartMillis = millis();
   SetIRQAssert;
   while (IO1[wRegIRQNMITest] != 1)
      if (millis() - StartMillis > 250) //Wait for IRQ detection w/timeout
      {
         SetIRQDeassert;
         SendMsgPrintfln(" IRQ not detected");
         return;
      }
   SetIRQDeassert;
   SendMsgPrintf(" OK");


//NMI
   SendMsgPrintfln("NMI Test");
   IO1[wRegIRQNMITest] = 0;
   StartMillis = millis();
   SetNMIAssert;
   while (IO1[wRegIRQNMITest] != 2)
      if (millis() - StartMillis > 250) //Wait for NMI detection w/timeout
      {
         SetNMIDeassert;
         SendMsgPrintfln(" NMI not detected");
         return;
      }
   SetNMIDeassert;
   SendMsgPrintf(" OK");


   IO1[rwRegScratch] = 1; //passed all TR+ sourced tests

//Set up ROMs for HIROM/LOROM/GAME/EXROM test...
   for(uint16_t ByteNum=0; ByteNum<256; ByteNum++) RAM_Image[ByteNum] = 0x55;
   for(uint16_t ByteNum=0; ByteNum<256; ByteNum++) RAM_Image[ByteNum+0x2000] = 0xaa;
   LOROM_Image = RAM_Image;
   HIROM_Image = RAM_Image+0x2000;

#else
   SendMsgPrintfln("For TR+ Only");
#endif
}

FLASHMEM void ExtPortCheck()
{
   //SendMsgPrintfln("\r\rExternal Port check started");
   IO1[rwRegScratch] = 0; //default fail

//Ethernet Init:
   if (!EthernetInit(SendStrPrintfln))
   {
      SendMsgPrintfln("Could not init Ethernet");
      return;
   }

//SD/SD Card:
   if (!SDFullInit())
   {
      SendMsgPrintfln("Could not init SD");
      return;
   }
   File dir = SD.open("/");
   const char *filename;
   File entry = dir.openNextFile();
   if (!entry)
   {
      SendMsgPrintfln("Couldn't open SD file");
      return;
   }
   filename = entry.name();
   entry.close();
   if (filename[0] == 0)
   {
      SendMsgPrintfln("No SD filename");
      return;
   }
   SendMsgPrintfln("\r1st SD file: %s", filename);

//USB Host/thumb drive:
   if (!USBFileSystemWait())
   {
      SendMsgPrintfln("Could not init USB");
      return;
   }
   dir = firstPartition.open("/");
   entry = dir.openNextFile();
   if (!entry)
   {
      SendMsgPrintfln("Couldn't open USB file");
      return;
   }
   filename = entry.name();
   entry.close();
   if (filename[0] == 0)
   {
      SendMsgPrintfln("No USB filename");
      return;
   }
   SendMsgPrintfln("1st USB file: %s", filename);

//Menu button:
   SendMsgPrintfln("\rPress and release the TR Menu Button");
   SendMsgPrintfln("  Waiting for press..");
   while(ReadButton);
   delay(10); //debounce
   BtnPressed = false; //force off so SendMsgPrintf doesn't abort wait
   SendMsgPrintf("detected");
   SendMsgPrintfln("  Waiting for release..");
   while(!ReadButton);
   delay(10); //debounce
   BtnPressed = false; //force off so SendMsgPrintf doesn't abort wait
   SendMsgPrintf("detected");

//Alt Button: (TR+ Only)
   #ifdef Fab04_SpecialButton

   SendMsgPrintfln("\rPress and release the Alt Button");
   SendMsgPrintfln("  Waiting for press..");
   while (1)
   {
      if (SpecialBtnBounce.update())
      {  //Special button Change (rise or fall)
         if (!SpecialBtnBounce.read()) break;
      }
   }
   SendMsgPrintf("detected");
   SendMsgPrintfln("  Waiting for release..");
   while (1)
   {
      if (SpecialBtnBounce.update())
      {  //Special button Change (rise or fall)
         if (SpecialBtnBounce.read()) break;
      }
   }
   SendMsgPrintf("detected");

   #endif

//USB Device
   C64TODfromRTC();
   char TimeStr[100];
   sprintf(TimeStr, "Hello from TR at: %02x:%02x:%02x %sm\n", (IO1[rRegLastHourBCD] & 0x7f) , IO1[rRegLastMinBCD], IO1[rRegLastSecBCD], (IO1[rRegLastHourBCD] & 0x80) ? "p" : "a");
   Serial.println();
   Serial.println(TimeStr);
   SendMsgPrintfln("\rSent to USB Serial:\r   %s", TimeStr);

   IO1[rwRegScratch] = 1; //passed
   //SendMsgPrintfln("External Port check finished");
   BtnPressed = false;  //in case of re-trigger/debounce
}

void (*StatusFunction[rsNumStatusTypes])() = //match RegStatusTypes order
{
   &MenuChange,          // rsChangeMenu
   &HandleExecution,     // rsStartItem
   &SetRTCfromNet,       // rsSetRTCfromNet
   &C64TODfromRTC,       // rsC64TODfromRTC
   &IOHandlerSelectInit, // rsIOHWSelInit
   &WriteEEPROM,         // rsWriteEEPROM
   &MakeBuildInfo,       // rsMakeBuildCPUInfoStr
   &UpDirectory,         // rsUpDirectory
   &SearchForLetter,     // rsSearchForLetter
   &LoadMainSIDforXfer,  // rsLoadSIDforXfer
   &NextPicture,         // rsNextPicture
   &LastPicture,         // rsLastPicture
   &WriteNFCTagCheck,    // rsWriteNFCTagCheck
   &WriteNFCTag,         // rsWriteNFCTag
   &NFCReEnable,         // rsNFCReEnable
   &SetBackgroundSID,    // rsSetBackgroundSID
   &SetAutoLaunch,       // rsSetAutoLaunch
   &ClearAutoLaunch,     // rsClearAutoLaunch
   &NextTextFile,        // rsNextTextFile
   &LastTextFile,        // rsLastTextFile
   &IOHandlerNextInit,   // rsIOHWNextInit, no longer used...
   &MountDxxFile,        // rsMountDxxFile
   &HotKeySetLaunch,     // rsHotKeySetLaunch
   &NetListenInit,       // rsNetListenInit
   &SetKERNALBin,        // rsSetKERNALBin
   &KERNALPreStart,      // rsKERNALPreStart
   &SetREUFile,          // rsSetREUFile
   &MakeFilenameStr,     // rsMakeFilenameStr
   &RTCAdjust,           // rsRTCAdjust
   &ForceEthInit,        // rsForceEthInit
   &ExtPortCheck,        // rsExtPortCheck
   &ExpPortDMA,          // rsExpPortDMA
};
