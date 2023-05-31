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
      case 0x64: //command from app
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
      case 'l': 
         PrintDebugLog();
         break;
         
      case 'p':
         AddStringToRxQueue("0123456789abcdef\r");
         break;
      case 'k': //kill client connection
         client.stop();
         Serial.printf("Client stopped\n");
         break;
      case 'r': //reset status/NMI
         SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
         SwiftRegCommand = 0;
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
   }
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
      if (BigBuf[Cnt] & AdjustedCycleTiming)
      {
         BigBuf[Cnt] &= ~AdjustedCycleTiming;
         Serial.printf("skip %lu ticks = %lu nS, adj = %lu nS\n", BigBuf[Cnt], CycTonS(BigBuf[Cnt]), CycTonS(BigBuf[Cnt])-nS_MaxAdjThresh);
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

bool EthernetInit()
{
   uint32_t beginWait = millis();

   Serial.print("\nEthernet init... ");
   if (Ethernet.begin(mac, 9000, 4000) == 0)  //reduce timeout from 60 to 9 sec, should be longer, or option to skip
   {
      Serial.printf("***Failed!*** took %d mS\n", (millis() - beginWait));
      // Check for Ethernet hardware present
      if (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("Ethernet HW was not found.");
      else if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");
      
      IO1[rRegLastSecBCD]  = 0;      
      IO1[rRegLastMinBCD]  = 0;      
      IO1[rRegLastHourBCD] = 0;      
      return false;
   }
   Serial.printf("passed. took %d mS\nIP: ", (millis() - beginWait));
   Serial.println(Ethernet.localIP());
   return true;
}
   

