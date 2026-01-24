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


//void   getFreeITCM();

FLASHMEM void ServiceSerial(Stream *ThisCmdChannel)
{  //ThisCmdChannel->available() confirmed before calling
   CmdChannel = ThisCmdChannel;
   uint16_t inVal = CmdChannel->read();
   switch (inVal)
   {
      case 0x64: //'d' command from app
         if(!SerialAvailabeTimeout()) return;
         inVal = (inVal<<8) | CmdChannel->read(); //READ NEXT BYTE
         //only commands available when busy:
         if (inVal == ResetC64Token) //Reset C64
         {
            CmdChannel->println("Reset cmd received");
            runMainTRApp_FromMin(); 
            return;
         }
         else if (inVal == VersionInfoToken) //Version Info
         {
            SendU16(AckToken);
            CmdChannel->printf("\nTeensyROM minimal %s\n\n", strVersionNumber);
            return;
         }
         else if (inVal == LaunchFileToken) //Launch File
         {
            LaunchFile();
            return;
         }
         else if (inVal == FWCheckToken) //Check firmware type
         {
            SendU16(FWMinimalToken);
            return;
         }
         SendU16(FailToken);
         CmdChannel->print("Busy!\n");
         return;

        
      //case 'u': //Jump to upper image (full build)
      //   EEPROM.write(eepAdDHCPEnabled, 0); //DHCP disabled
      //   EEPROM.put(eepAdMyIP, (uint32_t)IPAddress(192,168,1,222));
      //   CmdChannel->printf("Static IP updated\n"); 
      //   runMainTRApp();  
      //   break;

   // l, c
   #ifdef Dbg_SerLog 
      case 'l':  //Show Debug Log
         PrintDebugLog();
         break;
      case 'c': //Clear Debug Log buffer
         BigBufCount = 0;
         CmdChannel->println("Buffer Reset");
         break;       
   #endif
         
   // x
   #ifdef Dbg_SerMem 
      case 'x': 
         { 
           
            uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            CmdChannel->printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);
            
            //uint32_t RAM2Free = (RAM2BytesFree() & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            //CmdChannel->printf("RAM2 Free: %luK (%lu blks)\n", RAM2Free, RAM2Free/8);
            
            uint8_t NumChips = RAM2blocks();
            //CmdChannel->printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
            NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
            CmdChannel->printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
           
            CrtMax += NumChips*8;
            CmdChannel->printf("  CRT Max: %luK (%lu blks) ~%luK file\n", CrtMax, CrtMax/8, (uint32_t)(CrtMax*1.004));
            //larger File size due to header info.
         }
         break;
   #endif
   
   
   // t...
   #ifdef Dbg_SerTimChg
      case 't': //timing commands, 2 letters and 3-4 numbers
         CmdChannel->printf("-----\n");
         switch (CmdChannel->read())
         {
            case 'm': //nS_MaxAdj change
               GetDigits(4, &nS_MaxAdj);
               break;
            case 'r': //nS_RWnReady change
               GetDigits(3, &nS_RWnReady);
               break;
            case 'p': //nS_PLAprop change
               GetDigits(3, &nS_PLAprop);
               break;
            case 's': //nS_DataSetup change
               GetDigits(3, &nS_DataSetup);
               break;
            case 'h': //nS_DataHold change
               GetDigits(3, &nS_DataHold);
               break;
            case 'v': //VIC timing change
               GetDigits(3, &nS_VICStart);
               break;
            case 'd': //Set Defaults
               nS_MaxAdj    = Def_nS_MaxAdj; 
               nS_RWnReady  = Def_nS_RWnReady;  
               nS_PLAprop   = Def_nS_PLAprop;  
               nS_DataSetup = Def_nS_DataSetup;  
               nS_DataHold  = Def_nS_DataHold;  
               nS_VICStart  = Def_nS_VICStart;  
               CmdChannel->printf("Defaults set\n");
               break;
            default:
               CmdChannel->printf("No changes\n");
               break;
         }
         CmdChannel->printf("Current:    Variable  Val (Command)\n");
         CmdChannel->printf("\t   nS_MaxAdj %04d (tm####)\n", nS_MaxAdj);
         CmdChannel->printf("\t nS_RWnReady  %03d (tr###)\n", nS_RWnReady);
         CmdChannel->printf("\t  nS_PLAprop  %03d (tp###)\n", nS_PLAprop);
         CmdChannel->printf("\tnS_DataSetup  %03d (ts###)\n", nS_DataSetup);
         CmdChannel->printf("\t nS_DataHold  %03d (th###)\n", nS_DataHold);
         CmdChannel->printf("\t nS_VICStart  %03d (tv###)\n", nS_VICStart);
         CmdChannel->printf("\tSet Defaults      (td)\n");
         CmdChannel->printf("\tList current vals (t)\n-----\n");
         break;  
   #endif
   
   }
}

FLASHMEM void GetDigits(uint8_t NumDigits, uint32_t *SetInt)
{
   char inStr[NumDigits+1];
   
   for(uint8_t DigNum=0; DigNum<NumDigits; DigNum++)
   {
      if(!CmdChannel->available())
      {
         CmdChannel->println("\nNot enough Digits!\n");
         return;
      }
      inStr[DigNum] = CmdChannel->read(); 
   }
   inStr[NumDigits]=0;
   *SetInt = atol(inStr);
   CmdChannel->printf("\nVal Set to: %d\n\n", *SetInt);
}

FLASHMEM void PrintDebugLog()
{
   bool LogDatavalid = false;
   
   #ifdef DbgIOTraceLog
      CmdChannel->println("DbgIOTraceLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgCycAdjLog
      CmdChannel->println("DbgCycAdjLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgSpecial
      CmdChannel->println("DbgSpecial enabled");
      LogDatavalid = true;
   #endif
      
   //if (CurrentIOHandler == IOH_Debug)
   //{
   //   CmdChannel->println("Debug IO Handler enabled");
   //   LogDatavalid = true;
   //}               
   
   if (!LogDatavalid)
   {
      CmdChannel->println("No logging enabled");
      return;
   }
   
   bool BufferFull = (BigBufCount == BigBufSize);
   
   if  (BufferFull) BigBufCount--; //last element invalid
   
   for(uint16_t Cnt=0; Cnt<BigBufCount; Cnt++)
   {
      CmdChannel->printf("#%04d ", Cnt);
      if (BigBuf[Cnt] & DbgSpecialData)
      {
         //BigBuf[Cnt] &= ~DbgSpecialData;
         CmdChannel->printf("DbgSpecialData %04x : %02x\n", BigBuf[Cnt] & 0xFFFF, (BigBuf[Cnt] >> 24));
            //code used previously, in-situ:
               //#ifdef DbgSpecial
               //   if (BigBuf != NULL){
               //     BigBuf[BigBufCount] = Address | (HIROM_Image[Address & 0x1FFF]<<24) | DbgSpecialData;
               //     if (BigBufCount < BigBufSize) BigBufCount++;
               //   }
               //#endif
      }
      else if (BigBuf[Cnt] & AdjustedCycleTiming)
      {
         BigBuf[Cnt] &= ~AdjustedCycleTiming;
         CmdChannel->printf("skip %lu ticks = %lu nS, adj = %lu nS\n", BigBuf[Cnt], CycTonS(BigBuf[Cnt]), CycTonS(BigBuf[Cnt])-nS_MaxAdj);
      }
      else
      {
         CmdChannel->printf("%s 0xde%02x : ", (BigBuf[Cnt] & IOTLRead) ? "Read" : "\t\t\t\tWrite", BigBuf[Cnt] & 0xff);

         if (BigBuf[Cnt] & IOTLDataValid) CmdChannel->printf("%02x\n", (BigBuf[Cnt]>>8) & 0xff); //data is valid
         else CmdChannel->printf("n/a\n");
      }
   }
   
   if (BigBufCount == 0) CmdChannel->println("Buffer empty");
   if (BufferFull) CmdChannel->println("Buffer was full");
   CmdChannel->println("Buffer Reset");
   BigBufCount = 0;
}

FLASHMEM void LaunchFile()
{            
   //   App: LaunchFileToken 0x6444
   //Teensy: AckToken 0x64CC or RetryToken/abort from minimal
   //   App: Send DriveType(1), DestPath/Name(up to MaxNamePathLength, null term)
   //        DriveTypes: (RegMenuTypes)
   //           USBDrive  = 0
   //           SD        = 1
   //           Teensy    = 2
   //Teensy: AckToken 0x64CC
   //   C64: file Launches

   //Launch file token has been received
   SendU16(AckToken);
   RegMenuTypes DriveType;
   char FileNamePath[MaxNamePathLength];
   
   if (ReceiveFileName(&DriveType, FileNamePath))
   {
      SendU16(AckToken);
      //RemoteLaunch(DriveType, FileNamePath, false);
      //Since we're in minimal, save the info to EEPROM and switch to full app for launch.
      const char DriveNames[rmtNumTypes][6] =   //match RegMenuTypes
      {
         "USB:",
         "SD:",
         "TR:",
      };
      EEPwriteStr(eepAdCrtBootName, DriveNames[DriveType]);
      EEPwriteStr(eepAdCrtBootName+strlen(DriveNames[DriveType]), FileNamePath);
      EEPROM.write(eepAdMinBootInd, MinBootInd_LaunchFull);
      delay(10);  //let EEPROM write complete
      runMainTRApp();

   }
}

FLASHMEM bool ReceiveFileName(RegMenuTypes* DriveType, char *FileNamePath)
{
   uint32_t RecDrive;
   if (!GetUInt(&RecDrive, 1)) return false;
   
   if (RecDrive >= rmtNumTypes) return false;
   *DriveType = (RegMenuTypes)RecDrive;

   uint16_t CharNum=0;
   while (1) 
   {
      if(!SerialAvailabeTimeout()) return false;
      FileNamePath[CharNum] = CmdChannel->read();
      if (FileNamePath[CharNum]==0) return true;
      if (++CharNum == MaxNamePathLength)
      {
         SendU16(FailToken);
         CmdChannel->print("Path too long!\n");  
         return false;
      }
   }
}

FLASHMEM bool GetUInt(uint32_t *InVal, uint8_t NumBytes)
{  //Get NumBytes (4 max) raw bytes to construct InVal
   *InVal=0;
   for(int8_t ByteNum=NumBytes-1; ByteNum>=0; ByteNum--)
   {
      if(!SerialAvailabeTimeout()) return false;
      uint32_t ByteIn = CmdChannel->read();
      *InVal += (ByteIn << (ByteNum*8));
   }
   return true;
}

FLASHMEM void SendU16(uint16_t SendVal)
{
   CmdChannel->write((uint8_t)(SendVal & 0xff));
   CmdChannel->write((uint8_t)((SendVal >> 8) & 0xff));
}
   
FLASHMEM bool SerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!CmdChannel->available() && (millis() - StartTOMillis) < SerialTimoutMillis); // timeout loop
   if (CmdChannel->available()) return(true);
   
   SendU16(FailToken);
   CmdChannel->print("Timeout!\n");  
   return(false);
}

