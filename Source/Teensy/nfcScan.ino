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

#include <PN532_HSU.h>  //#define SeriaType added
#include <PN532.h>
 
#define maxNfcDataSize     504

USBHIDParser hid1(myusbHost);
USBHIDParser hid2(myusbHost);
USBHIDParser hid3(myusbHost);

USBSerial userial(myusbHost);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer userial(myusbHost, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusbHost); // Handles up to 512 but by default only for those > 64 bytes
PN532_HSU pn532hsu(userial);
PN532 nfc(pn532hsu);

uint8_t Lastuid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the last UID read

FLASHMEM void nfcInit() //called once in setup()
{
   nfc.begin();
   
   uint32_t versiondata = nfc.getFirmwareVersion();
   if (!versiondata) 
   {
      Serial.println("PN53x board not found");
      return;
   }

   Serial.printf("Found PN5%x nfc chip, FW v%d.%d\n", 
      (versiondata>>24) & 0xFF, 
      (versiondata>>16) & 0xFF,
      (versiondata>> 8) & 0xFF );      

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
   Serial.printf("nfcCheck: %lu mS ", beginWait);
   if(beginWait<11) Serial.println("Too fast!");
   else Serial.println("(OK)");
}


void nfcCheck()
{
   uint8_t uid[7];  // Buffer to store the returned UID
   uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
   
   if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) 
   {  //succesful read
      //Serial.print("*");
      if (uidLength == 7)
      { // We probably have a Mifare Ultralight card ...
         if(memcmp(uid, Lastuid, 7) == 0) return;  //same as previous
         memcpy(Lastuid, uid, 7); //update previous
         //Serial.printf(" %d dig UID: ", uidLength);
         //nfc.PrintHex(uid, uidLength);  //for (uint8_t i=0; i < uidLength; i++) Serial.print(" 0x");Serial.print(uid[i], HEX); 
         ReadTagLaunch();
      }
   }
}

FLASHMEM void ReadTagLaunch()
{
   uint16_t PageNum = 4; // Start with the first general-purpose user page (#4)
   bool MoreData = true;
   uint8_t TagData[maxNfcDataSize];
   uint16_t CharNum = 0;
   
   while (MoreData)
   {
      //Serial.printf("Reading page %d\n", PageNum);
      uint8_t success = nfc.mifareultralight_ReadPage (PageNum, TagData+CharNum);
      if (success)
      {
         for(uint8_t Num = 0; Num<4; Num++)
         {
            //Serial.printf("  pg %d, char %03d: 0x%02x %c\n", PageNum, CharNum, TagData[CharNum], TagData[CharNum]);
            if(TagData[CharNum] == 0 || TagData[CharNum] == 0xfe) MoreData = false;
            else 
            {
               if (++CharNum >= maxNfcDataSize) 
               {
                  Serial.println("Tag too large");
                  return;
               }
            }
         }
      }
      else
      {
         Serial.printf("Unable to read page %d\n", PageNum);
         return;
      }
      PageNum++;
   }
   TagData[CharNum] = 0; //terminate it
   
   //check for 0x03 start and size, skip it
   
   
   //check for android pre-pended data, skip it
   char T_enStart[] = {0x54, 0x02, 0x65, 0x6e};
   char* pDataStart = (char*)memmem((char*)TagData, CharNum, T_enStart, 4);
   if(pDataStart == NULL) 
   {
      pDataStart = (char*)TagData;
      Serial.printf("No T_en found\n");
   }
   else pDataStart += 4; 
   Serial.printf("Payload: %s\n\n", pDataStart);
   
   bool SD_nUSB = true;
   for(uint8_t num=0; num<3; num++) pDataStart[num]=toupper(pDataStart[num]);
   if(memcmp(pDataStart, "SD:", 3) == 0)
   {
      pDataStart += 3;
   }
   else if(memcmp(pDataStart, "USB:", 4) == 0)
   {
      SD_nUSB = false;
      pDataStart += 4;      
   }
   else
   {
      Serial.println("SD:/USB: not found");
      return;
   }
   
   RemoteLaunch(SD_nUSB, pDataStart);
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
FLASHMEM const char* memmem(const char *haystack, size_t hlen, const char *needle, size_t nlen)
{
    int needle_first;
    const char *p = haystack;
    size_t plen = hlen;

    if (!nlen) return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = (char*)memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen)) return p;
        p++;
        plen = hlen - (p - haystack);
    }
    return NULL;
}

#endif
