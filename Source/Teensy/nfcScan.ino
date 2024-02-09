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
 
USBHIDParser hid1(myusbHost);
USBHIDParser hid2(myusbHost);
USBHIDParser hid3(myusbHost);

USBSerial userial(myusbHost);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer userial(myusbHost, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusbHost); // Handles up to 512 but by default only for those > 64 bytes
PN532_HSU pn532hsu(userial);
PN532 nfc(pn532hsu);

void nfcInit() //called once in setup()
{


   nfc.begin();
  
   uint32_t versiondata = nfc.getFirmwareVersion();
   if (versiondata) 
   {
      // Got ok data, print it out!
      Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
      Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
      Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);      
   }
   else
   {
     Serial.print("Didn't find PN53x board");
   }
  
   // Set the max number of retry attempts to read from a card
   // This prevents us from waiting forever for a card, which is
   // the default behaviour of the PN532.
   //nfc.setPassiveActivationRetries(0xFF);
   nfc.setPassiveActivationRetries(0); //set 0 retries to spped this up
   
   // configure board to read RFID tags
   nfc.SAMConfig();
     
}

void nfcCheck() //called in loop() must be fast if no change 
{
   boolean success;
   uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
   uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
   
   // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
   // 'uid' will be populated with the UID, and uidLength will indicate
   // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
   success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
   
   if (success) {
      //Serial.println("Found a card!");
      //Serial.print("UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      Serial.print("UID Val ");
      for (uint8_t i=0; i < uidLength; i++) 
      {
        Serial.print(" 0x");Serial.print(uid[i], HEX); 
      }
      Serial.println();
      // Wait 1 second before continuing
      //delay(1000);
   }
   //else
   //{
      // PN532 probably timed out waiting for a card
      // Serial.println("Timed out waiting for a card");
   //}

}

#endif
