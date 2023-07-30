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


void ServiceSerial()
{
   uint8_t inByte = Serial.read();
   switch (inByte)
   {
      case 0x64: //'d' command from app
         inByte = Serial.read(); //READ NEXT BYTE
         switch (inByte)
         {
            case 0x55:  //ping
               Serial.println("TeensyROM Ready!");
               break;
            case 0xAA: //file x-fer pc->TR
               ReceiveFile();        
               break;
            case 0xEE: //Reset C64
               Serial.println("Reset cmd received");
               SetUpMainMenuROM();
               break;
            case 0x67: //Test/debug
               PrintDebugLog();
               break;
            default:
               Serial.printf("Unk cmd: %02x\n", inByte); 
               break;
         }
         break;
      case 'l':  //Show Debug Log
         PrintDebugLog();
         break;
      case 'c': //Clear Debug Log buffer
         BigBufCount = 0;
         Serial.println("Buffer Reset");
         break;
      case 'f': //show build info
         MakeBuildCPUInfoStr();
         Serial.println(BuildCPUInfoStr);
         //Serial.printf("Build Date/Time: %s  %s\nCPU Freq: %lu MHz   Temp: %.1f°C\n", __DATE__, __TIME__, (F_CPU_ACTUAL/1000000), tempmonGetTemp());
         break;
         
   // m, p, k, r, s
   #ifdef Dbg_SerSwift
      case 'm':
         {  
            //RxIn 497+551=1048
            //big 8-bit playground buffer for testing
            uint8_t inbuf[] ={
            168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,13,32,32,
            18,28,220,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,162,162,162,32,172,162,162,162,172,162,162,162,
            187,32,44,146,166,13,32,32,18,220,32,32,161,146,223,18,32,32,32,32,162,146,169,18,32,172,161,32,146,223,18,32,
            146,161,18,152,190,153,162,30,188,28,161,146,161,18,32,150,162,129,188,146,32,18,154,32,156,32,5,32,28,161,206,223,
            146,13,32,32,18,220,146,190,18,162,146,158,58,46,28,184,188,183,158,58,46,58,28,188,32,158,46,18,28,162,146,158,
            58,32,28,190,18,153,187,146,30,162,18,151,172,146,129,172,28,187,18,150,32,146,129,162,18,31,172,146,32,156,187,18,
            5,32,146,156,172,18,28,190,180,146,219,165,13,32,32,18,156,220,163,163,163,163,163,163,163,163,163,163,163,163,163,163,
            163,163,146,161,18,30,32,146,32,18,129,32,146,28,188,150,190,18,129,32,146,32,18,154,32,5,161,146,161,18,152,32,
            156,161,167,175,175,146,13,32,32,18,220,32,220,32,32,32,32,32,32,32,32,32,32,46,32,32,32,188,146,151,188,18,
            129,162,146,28,190,18,156,190,146,161,18,31,162,154,162,146,156,190,5,188,151,190,18,162,154,161,172,31,190,146,220,13,
            32,32,154,188,18,185,185,185,185,168,175,175,175,164,164,164,164,39,185,185,185,185,185,185,185,185,164,175,175,175,175,185,
            185,185,146,156,184,31,168,59,13,32,32,151,167,18,31,32,32,32,32,163,163,220,163,183,183,183,183,184,32,32,32,32,
            32,166,220,32,183,163,163,163,163,168,32,32,32,168,146,151,165,13,32,32,167,18,5,32,32,146,161,18,32,31,32,32,
            5,190,32,167,32,31,32,5,32,31,32,5,190,32,167,32,32,31,32,5,190,32,188,165,31,32,5,167,32,31,166,5,
            167,32,32,188,146,151,165,13,32,32,167,18,5,32,31,163,5,167,32,31,32,32,5,32,31,163,5,167,32,31,32,5,
            32,31,32,5,32,31,34,163,5,32,31,163,5,167,32,31,163,5,167,32,31,32,5,167,32,146,187,18,167,32,31,163,
            5,32,146,151,165,13,32,32,170,18,5,32,31,32,5,167,32,31,32,32,5,32,31,166,5,167,187,32,172,31,32,5,
            32,31,33,32,5,32,31,32,5,167,32,31,32,5,167,32,31,58,5,167,32,32,167,32,31,166,5,32,146,151,165,13,
            32,32,167,18,5,32,32,146,190,18,32,31,166,32,5,32,32,167,31,182,5,32,31,32,32,5,32,146,182,18,182,32,
            32,146,161,18,32,31,46,5,167,32,31,32,5,167,32,146,188,18,167,32,31,161,5,32,146,151,165,13,32,32,167,18,
            5,32,146,31,166,18,166,5,32,146,31,58,18,166,5,32,146,31,33,18,5,32,146,31,221,18,5,32,31,187,166,5,
            32,31,46,5,167,32,31,166,5,167,32,31,187,5,167,32,31,172,5,167,32,146,31,58,18,5,167,32,146,31,39,18,
            5,32,146,151,165,13,32,32,167,18,5,32,146,31,33,166,18,5,32,32,170,32,146,31,58,18,5,32,146,31,33,18,
            5,32,146,32,18,31,182,5,32,32,167,32,146,32,18,167,187,32,172,146,188,18,32,146,190,18,32,146,151,164,18,5,
            167,32,32,172,146,151,165,13,32,32,32,163,163,163,152,172,187,151,163,163,163,163,163,152,191,151,163,163,18,152,191,146,
            151,163,163,163,163,163,152,191,18,191,146,151,163,163,163,163,163,152,172,18,32,146,187,151,163,163,13,32,32,32,32,172,
            18,162,162,152,162,162,146,187,32,32,151,172,18,172,32,152,32,187,146,187,32,32,18,151,190,187,162,152,162,172,188,146,
            32,32,151,172,18,172,32,152,187,146,187,13,32,32,32,32,18,149,161,146,151,185,18,188,190,146,152,185,161,32,32,18,
            149,172,151,32,32,32,152,32,187,146,32,32,18,151,172,32,32,32,152,32,187,146,32,32,32,18,151,187,185,172,146,13,
            32,32,32,32,32,18,149,191,146,32,32,151,191,32,32,32,32,18,149,188,146,32,32,18,151,190,146,32,32,32,149,190,
            18,190,151,162,162,188,146,152,188,32,32,32,18,149,191,151,32,146,191,13,13,32,32,32,195,85,82,82,69,78,84,76,
            89,32,68,79,87,78,32,70,79,82,32,77,65,73,78,84,69,78,65,78,67,69,44,13,32,32,32,32,32,32,80,76,
            69,65,83,69,32,67,65,76,76,32,66,65,67,75,32,76,65,84,69,82,46,13,13,13,13,13,13
            };
            
            for(uint16_t Cnt=0; Cnt<sizeof(inbuf); Cnt++) AddCharToRxQueue(inbuf[Cnt]);
         }
         break;
      case 'p':
         AddASCIIStrToRxQueueLN("0123456789abcdef");
         break;
      case 'k': //kill client connection
         client.stop();
         Serial.printf("Client stopped\n");
         break;
      case 'r': //reset status/NMI
         SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
         SwiftRegCommand = SwiftCmndDefault;
         SwiftRegControl = 0;
         RxQueueHead = RxQueueTail = 0;
         //SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
         SetNMIDeassert;
         Serial.printf("Swiftlink Reset\n"); 
         break;
      case 's': //status
         {
            char stNot[] = " Not";
            
            Serial.printf("Swiftlink status:\n"); 
            Serial.printf("  client is");
            if (!client.connected()) Serial.printf(stNot);
            Serial.printf(" connected\n");
            
            Serial.printf("  Rx Queue Used: %d\n", RxQueueUsed); 
            
            Serial.printf("  RxIRQ is"); 
            if((SwiftRegCommand & SwiftCmndRxIRQEn) != 0) Serial.printf(stNot); 
            Serial.printf(" enabled\n"); 
            
            Serial.printf("  RxIRQ is");
            if((SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0) Serial.printf(stNot); 
            Serial.printf(" set\n");
         }
         break;
   #endif
   
   // v, a   
   #ifdef Dbg_SerTimChg
      case 'v': //VIC timing change
         GetDigits(3, &nS_VICStart);
         PrintTimingParams();
         break;
      case 'a': //nS_MaxAdj change
         GetDigits(4, &nS_MaxAdj);
         PrintTimingParams();
         break;
   #endif

   }
}

void PrintTimingParams()
{
   Serial.printf("   nS_MaxAdj: %d\n", nS_MaxAdj);
   Serial.printf(" nS_RWnReady: %d\n", nS_RWnReady);
   Serial.printf("  nS_PLAprop: %d\n", nS_PLAprop);
   Serial.printf("nS_DataSetup: %d\n", nS_DataSetup);
   Serial.printf(" nS_DataHold: %d\n", nS_DataHold);
   Serial.printf(" nS_VICStart: %d\n", nS_VICStart);
}

void GetDigits(uint8_t NumDigits, uint32_t *SetInt)
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

void PrintDebugLog()
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
      
   if (CurrentIOHandler == IOH_Debug)
   {
      Serial.println("Debug IO Handler enabled");
      LogDatavalid = true;
   }               
   
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
   
   if (BufferFull) Serial.println("Buffer was full");
   Serial.println("Buffer Reset");
   BigBufCount = 0;
}


void ReceiveFile()
{ 
      //   App: SendFileToken 0x64AA
      //Teensy: ack 0x6464
      //   App: Send Length(2), CS(2), Name(MaxItemNameLength 25, incl term), file(length)
      //Teensy: Pass 0x6480 or Fail 0x9b7f

      //send file token has been received, only 2 byte responses until final response
   
   Serial.write(0x64);  //ack
   Serial.write(0x64);  
   //USBHostMenu.ItemType = rtNone;  
   NumUSBHostItems = 0; //in case we fail
   
   if(!SerialAvailabeTimeout()) return;
   uint16_t len = Serial.read();
   len = len + 256 * Serial.read();
   if(!SerialAvailabeTimeout()) return;
   uint16_t CheckSum = Serial.read();
   CheckSum = CheckSum + 256 * Serial.read();
   
   for (int i = 0; i < MaxItemNameLength; i++) 
   {
      if(!SerialAvailabeTimeout()) return;
      USBHostMenu.Name[i] = Serial.read();
   }
   
   free(HOST_Image);
   HOST_Image = (uint8_t*)malloc(len);
   uint16_t bytenum = 0;
   while(bytenum < len)
   {
      if(!SerialAvailabeTimeout())
      {
         Serial.printf("(PayL) Rec %d of %d, RCS:%d, Name:%s\n", bytenum, len, CheckSum, USBHostMenu.Name);
         return;
      }
      HOST_Image[bytenum] = Serial.read();
      CheckSum-=HOST_Image[bytenum++];

   }  
   if (CheckSum!=0)
   {  //Failed
     Serial.write(0x9B);  // 155
     Serial.write(0x7F);  // 127
     
     Serial.printf("Failed! Len:%d, RCS:%d, Name:%s\n", len, CheckSum, USBHostMenu.Name);
     //for (int i = 0; i < MaxItemNameLength; i++) Serial.printf("%02d-%d\n", i, USBHostMenu.Name[i]);
     return;
   }   

   //success!
   Serial.write(0x64);  
   Serial.write(0x80);  
   Serial.printf("%s received succesfully\n", USBHostMenu.Name);
   
   USBHostMenu.Size = len;  
   
   //check extension
   char* Extension = (USBHostMenu.Name + strlen(USBHostMenu.Name) - 4);
   for(uint8_t cnt=1; cnt<=3; cnt++) if(Extension[cnt]>='A' && Extension[cnt]<='Z') Extension[cnt]+=32;
   
   if (strcmp(Extension, ".prg")==0)
   {
      USBHostMenu.ItemType = rtFilePrg;
      Serial.println(".PRG file detected");
      NumUSBHostItems = 1;
      return;
   }
   
   if (strcmp(Extension, ".crt")==0)
   {
      USBHostMenu.ItemType = rtFileCrt;
      Serial.println(".CRT file detected"); 
      NumUSBHostItems = 1;
      return;
   }
   
   //NumUSBHostItems = 0, set at start
   Serial.println("File type unknown!");
   return;
 
}

   
uint32_t toU32(uint8_t* src)
{
   return
      ((uint32_t)src[0]<<24) + 
      ((uint32_t)src[1]<<16) + 
      ((uint32_t)src[2]<<8 ) + 
      ((uint32_t)src[3]    ) ;
}

uint16_t toU16(uint8_t* src)
{
   return
      ((uint16_t)src[0]<<8 ) + 
      ((uint16_t)src[1]    ) ;
}

bool SerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!Serial.available() && (millis() - StartTOMillis) < SerialTimoutMillis); // timeout loop
   if (Serial.available()) return(true);
   
   Serial.write(0x9B);  // 155
   Serial.write(0x7F);  // 127
   Serial.print("Timeout!\n");  
   return(false);
}

