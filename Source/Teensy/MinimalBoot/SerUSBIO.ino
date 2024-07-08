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

FLASHMEM void ServiceSerial()
{  //Serial.available() confirmed before calling
   uint16_t inVal = Serial.read();
   switch (inVal)
   {
      case 0x64: //'d' command from app
         if(!SerialAvailabeTimeout()) return;
         inVal = (inVal<<8) | Serial.read(); //READ NEXT BYTE
         //only commands available when busy:
         if (inVal == ResetC64Token) //Reset C64
         {
            Serial.println("Reset cmd received");
            BtnPressed = true;
            return;
         }
         //else if (inVal == LaunchFileToken) //Launch File
         //{
         //   LaunchFile();
         //   return;
         //}
         //
         //if (CurrentIOHandler != IOH_TeensyROM)
         //{
            SendU16(FailToken);
            Serial.print("Busy!\n");
            return;
         //}
         //TeensyROM IO Handler is active...
         
         //switch (inVal)
         //{
         //   case PingToken:  //ping
         //      Serial.printf("TeensyROM %s ready!\n", strVersionNumber);
         //      break;
         //   case SendFileToken: //file x-fer pc->TR
         //   case PostFileToken:  // v2 file x-fer pc->TR.  For use with v2 UI.
         //      PostFileCommand();
         //      break;
         //   case CopyFileToken:
         //       CopyFileCommand();
         //       break;
         //   case DeleteFileToken:
         //       DeleteFileCommand();
         //       break;
         //   case GetDirectoryToken:  // v2 directory listing from TR
         //      GetDirectoryCommand();
         //      break;
         //   case PauseSIDToken: //Pause SID
         //      if(RemotePauseSID()) SendU16(AckToken);
         //      else SendU16(FailToken);
         //      break;
         //   case DebugToken: //'dg'Test/debug
         //      //for (int a=0; a<256; a++) Serial.printf("\n%3d, // %3d   '%c'", ToPETSCII(a), a, a);
         //      //PrintDebugLog();
         //      break;
         //   default:
         //      Serial.printf("Unk cmd: 0x%04x\n", inVal); 
         //      break;
         //}
         //break;
      //case 'e': //Reset EEPROM to defaults
      //   SetEEPDefaults();
      //   Serial.println("Applied upon reboot");
      //   break;
      case 'u': //Jump to upper image (full build)
         runApp(UpperAddr);  
         break;

   // l, c
   #ifdef Dbg_SerLog 
      case 'l':  //Show Debug Log
         PrintDebugLog();
         break;
      case 'c': //Clear Debug Log buffer
         BigBufCount = 0;
         Serial.println("Buffer Reset");
         break;       
   #endif
         
   // x
   #ifdef Dbg_SerMem 
      case 'x': 
         { 
            //FreeDriveDirMenu(); //Will mess up navigation if not on TR menu!
            
            uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            Serial.printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);
            
            //uint32_t RAM2Free = (RAM2BytesFree() & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            //Serial.printf("RAM2 Free: %luK (%lu blks)\n", RAM2Free, RAM2Free/8);
            
            uint8_t NumChips = RAM2blocks();
            //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
            NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
            Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
           
            CrtMax += NumChips*8;
            Serial.printf("  CRT Max: %luK (%lu blks) ~%luK file\n", CrtMax, CrtMax/8, (uint32_t)(CrtMax*1.004));
            //larger File size due to header info.
         }
         break;
   #endif
   
   
   // t...
   #ifdef Dbg_SerTimChg
      case 't': //timing commands, 2 letters and 3-4 numbers
         Serial.printf("-----\n");
         switch (Serial.read())
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
               Serial.printf("Defaults set\n");
               break;
            default:
               Serial.printf("No changes\n");
               break;
         }
         Serial.printf("Current:    Variable  Val (Command)\n");
         Serial.printf("\t   nS_MaxAdj %04d (tm####)\n", nS_MaxAdj);
         Serial.printf("\t nS_RWnReady  %03d (tr###)\n", nS_RWnReady);
         Serial.printf("\t  nS_PLAprop  %03d (tp###)\n", nS_PLAprop);
         Serial.printf("\tnS_DataSetup  %03d (ts###)\n", nS_DataSetup);
         Serial.printf("\t nS_DataHold  %03d (th###)\n", nS_DataHold);
         Serial.printf("\t nS_VICStart  %03d (tv###)\n", nS_VICStart);
         Serial.printf("\tSet Defaults      (td)\n");
         Serial.printf("\tList current vals (t)\n-----\n");
         break;  
   #endif
   
   }
}

FLASHMEM void GetDigits(uint8_t NumDigits, uint32_t *SetInt)
{
   char inStr[NumDigits+1];
   
   for(uint8_t DigNum=0; DigNum<NumDigits; DigNum++)
   {
      if(!Serial.available())
      {
         Serial.println("\nNot enough Digits!\n");
         return;
      }
      inStr[DigNum] = Serial.read(); 
   }
   inStr[NumDigits]=0;
   *SetInt = atol(inStr);
   Serial.printf("\nVal Set to: %d\n\n", *SetInt);
}

FLASHMEM void PrintDebugLog()
{
   bool LogDatavalid = false;
   
   #ifdef DbgIOTraceLog
      Serial.println("DbgIOTraceLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgCycAdjLog
      Serial.println("DbgCycAdjLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgSpecial
      Serial.println("DbgSpecial enabled");
      LogDatavalid = true;
   #endif
      
   //if (CurrentIOHandler == IOH_Debug)
   //{
   //   Serial.println("Debug IO Handler enabled");
   //   LogDatavalid = true;
   //}               
   
   if (!LogDatavalid)
   {
      Serial.println("No logging enabled");
      return;
   }
   
   bool BufferFull = (BigBufCount == BigBufSize);
   
   if  (BufferFull) BigBufCount--; //last element invalid
   
   for(uint16_t Cnt=0; Cnt<BigBufCount; Cnt++)
   {
      Serial.printf("#%04d ", Cnt);
      if (BigBuf[Cnt] & DbgSpecialData)
      {
         //BigBuf[Cnt] &= ~DbgSpecialData;
         Serial.printf("DbgSpecialData %04x : %02x\n", BigBuf[Cnt] & 0xFFFF, (BigBuf[Cnt] >> 24));
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
         Serial.printf("skip %lu ticks = %lu nS, adj = %lu nS\n", BigBuf[Cnt], CycTonS(BigBuf[Cnt]), CycTonS(BigBuf[Cnt])-nS_MaxAdj);
      }
      else
      {
         Serial.printf("%s 0xde%02x : ", (BigBuf[Cnt] & IOTLRead) ? "Read" : "\t\t\t\tWrite", BigBuf[Cnt] & 0xff);

         if (BigBuf[Cnt] & IOTLDataValid) Serial.printf("%02x\n", (BigBuf[Cnt]>>8) & 0xff); //data is valid
         else Serial.printf("n/a\n");
      }
   }
   
   if (BigBufCount == 0) Serial.println("Buffer empty");
   if (BufferFull) Serial.println("Buffer was full");
   Serial.println("Buffer Reset");
   BigBufCount = 0;
}

FLASHMEM void SendU16(uint16_t SendVal)
{
   Serial.write((uint8_t)(SendVal & 0xff));
   Serial.write((uint8_t)((SendVal >> 8) & 0xff));
}
   
FLASHMEM bool SerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!Serial.available() && (millis() - StartTOMillis) < SerialTimoutMillis); // timeout loop
   if (Serial.available()) return(true);
   
   SendU16(FailToken);
   Serial.print("Timeout!\n");  
   return(false);
}

