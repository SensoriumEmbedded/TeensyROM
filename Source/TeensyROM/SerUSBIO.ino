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
   if (inByte == 0x64) //command from app
   {
      inByte = Serial.read();
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
   }
   else if (inByte == 'l') PrintDebugLog();
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
      
   if (IOHandler == IOH_Debug)
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
         Serial.printf("%s 0xde%02x : ", (BigBuf[Cnt] & IOTLRead) ? "Read" : "Write", BigBuf[Cnt] & 0xff);
         
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
  
void getNtpTime() 
{
   Serial.print("\nEthernet init... ");
   uint32_t beginWait = millis();

   if (Ethernet.begin(mac, 9000, 4000) == 0)  //reduce timeout from 60 to 9 sec, should be longer, or option to skip
   {
      Serial.printf("***Failed!*** took %d mS\n", (millis() - beginWait));
      // Check for Ethernet hardware present
      if (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("Ethernet HW was not found.");
      else if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");
      
      IO1[rRegLastSecBCD]  = 0;      
      IO1[rRegLastMinBCD]  = 0;      
      IO1[rRegLastHourBCD] = 0;      
      return;
   }
   Udp.begin(localPort);
   Serial.printf("passed. took %d mS\nIP: ", (millis() - beginWait));
   Serial.println(Ethernet.localIP());
   
   const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
   byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
   
   Serial.printf("Updating time from: %s\n", timeServer);
   while (Udp.parsePacket() > 0) ; // discard any previously received packets
   
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
    Udp.beginPacket(timeServer, 123); // NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();

   beginWait = millis();
   while (millis() - beginWait < 1500) 
   {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) 
      {
         Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
         uint32_t secsSince1900;
         // convert four bytes starting at location 40 to a long integer
         secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
         secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
         secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
         secsSince1900 |= (unsigned long)packetBuffer[43];
         Serial.printf("Received NTP Response in %d mS\n", (millis() - beginWait));

         //since we don't need the date, leaving out TimeLib.h all together
         IO1[rRegLastSecBCD] =DecToBCD(secsSince1900 % 60);
         secsSince1900 /=60; //to  minutes
         IO1[rRegLastMinBCD] =DecToBCD(secsSince1900 % 60);
         secsSince1900 = (secsSince1900/60 + (int8_t)IO1[rwRegTimezone]) % 24; //to hours, offset timezone
         if (secsSince1900 >= 12) IO1[rRegLastHourBCD] = 0x80 | DecToBCD(secsSince1900-12); //change to 0 based 12 hour and add pm flag
         else IO1[rRegLastHourBCD] =DecToBCD(secsSince1900); //default to AM (bit 7 == 0)
   
         Serial.printf("Time: %02x:%02x:%02x %sm\n", (IO1[rRegLastHourBCD] & 0x7f) , IO1[rRegLastMinBCD], IO1[rRegLastSecBCD], (IO1[rRegLastHourBCD] & 0x80) ? "p" : "a");        
         return;
      }
   }
   Serial.println("NTP Response timeout!");
}

uint8_t DecToBCD(uint8_t DecVal)
{
   return (int(DecVal/10)<<4) | (DecVal%10);
}

