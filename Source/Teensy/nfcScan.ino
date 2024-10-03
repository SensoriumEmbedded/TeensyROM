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

//  TeensyROM: A C64 ROM emulator and loader/interface cartidge based on the Teensy 4.1
//  Copyright (c) 2023 Travis Smith <travis@sensoriumembedded.com> 

#ifdef nfcScanner

#include <PN532.h>      //From: https://github.com/elechouse/PN532
#include "PN532_UHSU.h" //Customized for USBSerial instead of HardwareSerial
 
USBHIDParser hid1(myusbHost);
USBHIDParser hid2(myusbHost);
USBHIDParser hid3(myusbHost);  //need all 3?

USBSerial userial(myusbHost);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer userial(myusbHost, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusbHost); // Handles up to 512 but by default only for those > 64 bytes
PN532_UHSU pn532uhsu(userial);
PN532 nfc(pn532uhsu);

#define MaxNfcConfRetries    20

FLASHMEM void nfcInit()
{  
   Serial.println("nfc init");
   
   nfcState = nfcStateBitDisabled; // set default
   memset(Lastuid, 0, sizeof Lastuid);
   nfc.begin();
   
   uint32_t versiondata = nfc.getFirmwareVersion();
   if (!versiondata) 
   {
      Serial.println(" PN53x board not found\n");
      return;
   }
   
   Printf_dbg("Found PN5%x nfc chip, FW v%d.%d\n", 
      (versiondata>>24) & 0xFF, 
      (versiondata>>16) & 0xFF,
      (versiondata>> 8) & 0xFF );      

   uint8_t TryNum = 0;
   while (!nfcConfigCheck())
   {
      if (TryNum++ == MaxNfcConfRetries)
      {
         Serial.println(" config failed\n");   
         return;
      }
   }

   Serial.printf(" ready in %d retries\n", TryNum);
   nfcState = nfcStateEnabled;
}

FLASHMEM bool nfcConfigCheck()
{   
   nfc.begin();
   
   if (!nfc.getFirmwareVersion()) 
   {
      Printf_dbg("Lost PN53x chip\n");     
      return false;
   }

   // Set the max number of retry attempts to read from a card
   // This prevents us from waiting forever (PN532 default)
   //nfc.setPassiveActivationRetries(0xFF); //example, takes about 1s
   nfc.setPassiveActivationRetries(0); //reduce retries to speed this up
   // 0 fails every other read, 19mS;    1 is consistent, but 25mS
   
   nfc.SAMConfig(); // configure board to read RFID tags

   //aliexpress module can init in a weird state: reads too fast, never succesfully
   uint32_t beginWait = millis();
   nfcCheck();
   beginWait = millis() - beginWait;
   return (beginWait>10);
   //Printf_dbg("nfcCheck: %lu mS ", beginWait);
   //if(beginWait<=10) 
   //{
   //   Printf_dbg("Too fast!\n");
   //   return false;
   //}
   //Printf_dbg("(OK)\n");
   //return true;
}

void nfcCheck()
{
   uint8_t uid[7];  // Buffer to store the returned UID
   uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
   
   if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) 
   {  //succesful read
      //Printf_dbg("*");
      if (uidLength == 7)
      { // We probably have a Mifare Ultralight card ...
         if(memcmp(uid, Lastuid, 7) == 0) return;  //same as previous
         //Printf_dbg(" %d dig UID: ", uidLength);
         //nfc.PrintHex(uid, uidLength);  //for (uint8_t i=0; i < uidLength; i++) Serial.print(" 0x");Serial.print(uid[i], HEX); 
         if (nfcReadTagLaunch()) memcpy(Lastuid, uid, 7); //update previous
      }
   }
}

RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName)
{  //returns file RegMenuTypes (default SD) and changes pointer to skip USB:/SD:

   char *ptrFileName = *ptrptrFileName;
   RegMenuTypes MenuSourceID = rmtSD;
   
   for(uint8_t num=0; num<3; num++) ptrFileName[num]=toupper(ptrFileName[num]);
   
   if(memcmp(ptrFileName, "SD:", 3) == 0)
   {
      ptrFileName += 3;
   }
   else if(memcmp(ptrFileName, "USB:", 4) == 0)
   {
      MenuSourceID = rmtUSBDrive;
      ptrFileName += 4;      
   }
   else if(memcmp(ptrFileName, "TR:", 3) == 0)
   {
      MenuSourceID = rmtTeensy;
      ptrFileName += 3;      
   }
   else
   {
      Printf_dbg("SD:/USB: not found\n");
      //default to SD if not specified, allows display on c64
   }
   
   *ptrptrFileName = ptrFileName; //update the pointer
   return MenuSourceID;
}

bool nfcReadTagLaunch()
{
   uint16_t PageNum = 4; // Start with the first general-purpose user page (#4)
   bool MoreData = true;
   uint8_t DataStart, messageLength, TagData[MaxPathLength];
   uint16_t CharNum = 0;
   
   while (MoreData)
   {
      if (!nfc.mifareclassic_ReadDataBlock (PageNum, TagData+CharNum)) //read 4 page block
      {
         Printf_dbg("Couldn't read pg %d\n", PageNum);
         return false;
      }
       
      if(PageNum == 4)
      { //extract start/length info when read (first pass through)
         if(TagData[0] == 0x03)
         {
            messageLength = TagData[1];
            DataStart = 2;
         }
         else if (TagData[5] == 0x03) 
         {
            messageLength = TagData[6];
            DataStart = 7;
         }
         else
         {
            Printf_dbg("Length token not found\n");
            return false;
         }
      }         

      for(uint8_t Num = 0; Num<16; Num++)
      {
         if(TagData[CharNum] == 0xfe || CharNum >= DataStart+messageLength) MoreData = false;
         else CharNum++;
      }
      PageNum+=4;
   }
   TagData[CharNum] = 0; //terminate it

   Printf_dbg("\nRec %d pgs, %d msg len, %d chars read:\n%s\n", PageNum-4, messageLength, CharNum, TagData+DataStart);
   //for(uint16_t cnt=0; cnt<=CharNum; cnt++) Serial.printf("Chr %d: 0x%02x '%c'\n", cnt, TagData[cnt], TagData[cnt]);
 
 
   //*** Finished receiving data, now parse & check it ***

   // https://www.oreilly.com/library/view/beginning-nfc/9781449324094/ch04.html
   // https://www.eet-china.com/d/file/archives/ARTICLES/2006AUG/PDF/NFCForum-TS-NDEF.pdf
   // https://github.com/haldean/ndef/blob/master/docs/NFCForum-TS-NDEF_1.0.pdf
   // bytes 0 and 1 are 0x03 and messageLength
   //                  Short Record Header
   // Chr  2: 0xd1 ''      TNF+Flags (Well known record type, 1101 0xxx MB+ME+SR)
   // Chr  3: 0x01 ''      Type length (1 byte)
   // Chr  4: 0x3c ''      Payload length
   //                      ID Length (if IL flag set)
   // Chr  5: 0x54 'T'     Payload Type (Text)
   // Chr  6: 0x02 ''      Type length (2 bytes)
   // Chr  7: 0x65 'e'     Type Payload
   // Chr  8: 0x6e 'n'     Type Payload
   //                   Record Payload:
   // Chr  9: 0x53 'S'   
   // Chr 10: 0x44 'D'
   // Chr 11: 0x3a ':'
   //      ...
   
   uint8_t* pDataStart = TagData + DataStart;
   
   if((pDataStart[0] & 0x07) != 1) //Well-Known Type Name Format, could enforce SR, MB, ME...
   {
      Printf_dbg("Not Well-Known TNF: %02x\n", pDataStart[0]);
      return false;      
   }
   //assuming short record format...
   pDataStart += 2 + pDataStart[1] + ((pDataStart[0] & 0x08)>>3); //add 1 for ID length, if IL flagged
   
   if(pDataStart[0] != 'T') 
   {
      Printf_dbg("Not Text Type Payload: %02x\n", pDataStart[0]);
      return false;      
   }
   if(pDataStart[1] & 0x80) 
   {
      Printf_dbg("Must be UTF-8: %02x\n", pDataStart[1]);
      return false;      
   }
   pDataStart += 2 + (pDataStart[1] & 0x3f);
  
  
   Printf_dbg("Final Payload: %s\n\n", pDataStart);
   
   RegMenuTypes MenuSourceID = RegMenuTypeFromFileName((char**)&pDataStart);

   if(memcmp(pDataStart, "C64", 3) == 0 && MenuSourceID != rmtTeensy)
   { //could be used to specify system type for TapTo, only valid for SD/USB
      FS *sourceFS;
      if(MenuSourceID == rmtSD) 
      {
         sourceFS = &SD;
         SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
      }
      else sourceFS = &firstPartition;    

      Printf_dbg("C64 found\n");
      if (!sourceFS->exists((char*)pDataStart)) 
      {
         pDataStart+=3; //skip it if file not found with it present
         Printf_dbg(" & removed\n");
      }
   }
      
   //Printf_dbg("Launching...\n");
   RemoteLaunch(MenuSourceID, (char*)pDataStart);
   return true;
}

FLASHMEM void nfcWriteTag(const char* TxtMsg)
{
   uint8_t uid[7];  // Buffer to store the returned UID
   uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type) 
   uint8_t TryNum = 0;
   
   while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength))
   {
      if (TryNum++ == 10) // 10 retries, doesn't help much if out of range or too close
      {
         SendMsgPrintfln(" Couldn't verify tag");
         return;
      }
   }

   if (TryNum) SendMsgPrintfln("Verify took %d retries", TryNum);
   Printf_dbg("Verify read took %d retries\nUID Length = %d\n", TryNum, uidLength);
   
   if (uidLength != 7)
   {
      SendMsgPrintfln(" Unsupported tag type");
      return;
   }      
   
   uint8_t TagData[MaxPathLength] = {0x03, 0x13, 0xd1, 0x01, 0x10, 0x54, 0x02, 0x65, 0x6e};
   uint16_t messageLength = strlen(TxtMsg) + 9;
   
   if (messageLength>254)
   {
      SendMsgPrintfln(" Path too long");
      return;
   }
   strcpy((char*)TagData + 9, TxtMsg);
   TagData[1] = messageLength;
   TagData[messageLength++] = 0xfe; //replace termination w/ NDEF term

   SendMsgPrintfln("Writing NFC Tag...");
   //Printf_dbg("Writing tag\n");
   //for(uint16_t cnt=0; cnt<=messageLength; cnt++) Serial.printf("Chr %d: 0x%02x '%c'\n", cnt, TagData[cnt], TagData[cnt]);

   for(uint16_t PageNum = 0; PageNum < messageLength/4+1; PageNum++)
   {
      Printf_dbg("Writing pg %d\n", PageNum+4);
      //if (!nfc.mifareclassic_WriteDataBlock(4, TagData)) //only writes one page(?)
      if (!nfc.mifareultralight_WritePage(PageNum+4, TagData+PageNum*4))
      {
         //Printf_dbg("Failed!\n");
         SendMsgPrintfln("Write Failed!");
         return;
      }
   }
   SendMsgPrintfln("Success!");
}

#endif
