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
USBHIDParser hid3(myusbHost);

USBSerial userial(myusbHost);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer userial(myusbHost, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusbHost); // Handles up to 512 but by default only for those > 64 bytes
PN532_UHSU pn532uhsu(userial);
PN532 nfc(pn532uhsu);

#define MaxNfcConfRetries    20

FLASHMEM void nfcInit() //called once in setup(), if NFC enabled
{
   Serial.println("nfc init");
   
   memset(Lastuid, 0, sizeof Lastuid);
   nfc.begin();
   
   uint32_t versiondata = nfc.getFirmwareVersion();
   if (!versiondata) 
   {
      Serial.println(" PN53x board not found\n");
      //todo: disable NFC
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
         //todo: disable NFC 
         return;
      }
   }

   Serial.printf(" ready in %d retries\n", TryNum);
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
   //Printf_dbg("nfcCheck: %lu mS ", beginWait);
   //if(beginWait<11) 
   //{
   //   Printf_dbg("Too fast!\n");
   //   return false;
   //}
   //Printf_dbg("(OK)\n");
   //return true;
   return (beginWait>10);
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
         if (ReadTagLaunch()) memcpy(Lastuid, uid, 7); //update previous
      }
   }
}

bool ReadTagLaunch()
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
   //for(uint16_t cnt=DataStart; cnt<=CharNum; cnt++) Serial.printf("Chr %d: 0x%02x '%c'\n", cnt, TagData[cnt], TagData[cnt]);

   //check for android pre-pended data, skip it
   char T_enStart[] = {0x54, 0x02, 0x65, 0x6e};
   char* pDataStart = (char*)memmem((char*)TagData+DataStart, CharNum, T_enStart, 4);
   if(pDataStart == NULL) 
   {
      pDataStart = (char*)TagData+DataStart;
      Printf_dbg("No T_en found\n");
   }
   else pDataStart += 4; //offset by T_enStart length
   Printf_dbg("Final Payload: %s\n\n", pDataStart);
   
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
      Printf_dbg("SD:/USB: not found\n");
      return false;
   }
   
   RemoteLaunch(SD_nUSB, pDataStart);
   return true;
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
const char* memmem(const char *haystack, size_t hlen, const char *needle, size_t nlen)
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
