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


#include <PN532.h>      //From: https://github.com/elechouse/PN532
#include "PN532_UHSU.h" //Customized for USBSerial instead of HardwareSerial
 
PN532_UHSU pn532uhsu(USBHostSerial);
PN532 nfc(pn532uhsu);

#define MaxNfcConfRetries    20
#define NFCReReadTimeout   1000  // mS since of no scan to re-accept same tag

uint8_t  Lastuid[7];  // Buffer to store the last UID read
uint8_t  LastuidLength  = 7;
uint32_t LastTagMillis  = 0; //stores last good tag time for Lastuid timeout
bool     AllowTagRescan = false; //same card rescan only allowed for random cards

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
      if (uidLength == 7 || uidLength == 4)
      { // Mifare Ultralight (7) or Classic 1k(4)
         if(uidLength!=LastuidLength || memcmp(uid, Lastuid, LastuidLength) != 0) //not the same as previous
         {
            //Printf_dbg(" %d dig UID: ", uidLength);
            //nfc.PrintHex(uid, uidLength);  //for (uint8_t i=0; i < uidLength; i++) Serial.print(" 0x");Serial.print(uid[i], HEX); 
            if (nfcReadTagLaunch(uid, uidLength)) 
            {
               LastuidLength=uidLength;
               memcpy(Lastuid, uid, uidLength); //update previous
            }
         }
      }
      LastTagMillis = millis(); //do this after launch in case it's "random" and takes >NFCReReadTimeout
   }
   else
   { //no card detected, check for timeout & clear Lastuid
      if(AllowTagRescan)
      {
         if(millis()-LastTagMillis > NFCReReadTimeout)
         {
            memset(Lastuid, 0, sizeof Lastuid); //clear Lastuid to allow re-tag
            AllowTagRescan = false; //Only timeout/clear once until next tag is scanned
         }
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

FS *FSfromSourceID(RegMenuTypes SourceID)
{
   if(SourceID == rmtSD) 
   {
      SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
      return &SD;
   }
   else if(SourceID == rmtUSBDrive) return &firstPartition;    
   else return NULL;
}

bool nfcReadTagLaunch(uint8_t* uid, uint8_t uidLength)
{
   uint16_t PageNum = 4; // Start with the first general-purpose user page (#4)
   bool MoreData = true;
   uint8_t DataStart, messageLength, TagData[MaxPathLength];
   uint16_t CharNum = 0;
   uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };


   while (MoreData)
   {
      Printf_dbg("Tag page %d\n", PageNum);
      
      if(uidLength==4 && (PageNum%4)==0)  //Authentication only needed on classic/1K, on pg 4, 8, etc
      {
         if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, PageNum, 0, keya))
         {
            Printf_dbg("Couldn't Authenticate pg %d\n", PageNum);
            return false;
         }   
         else Printf_dbg("Authenticated pg %d\n", PageNum);
      }
      
      
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
         else if (TagData[2] == 0x03) //mifare classic 1k
         {
            messageLength = TagData[3];
            DataStart = 4;
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

      if(uidLength==7)
      { //Ultralight
         PageNum+=4;
      }
      else
      { //classic/1K
         PageNum++;
         if((PageNum%4)==3)PageNum++; //skip key pages
      }
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

   // "C64" could be used to specify system type for TapTo, only valid for SD/USB
   if(memcmp(pDataStart, "C64", 3) == 0)
   { 
      Printf_dbg("C64 found\n");
      FS *sourceFS = FSfromSourceID(MenuSourceID);
      
      if (sourceFS != NULL)
      {
         if (!sourceFS->exists((char*)pDataStart)) //make sure there's not a file named "C64"
         {
            pDataStart+=3; //skip it if file not found with it present
            Printf_dbg(" & removed\n");
         }
      }
   }
   
   //check for & set up random launch
   if(strcmp((char*)pDataStart+strlen((char*)pDataStart)-2, "/?")==0)
   {  
      uint16_t LocalNumItems = 0;
      StructMenuItem *LocalDirMenu;
      
      Printf_dbg("Random requested\n");
      AllowTagRescan = true; //allow rescan of random tags.
      SetRandomSeed();
      
      pDataStart[strlen((char*)pDataStart)-1]=0; //remove the "?"

      //point to or load the dir contents:
      if(MenuSourceID == rmtTeensy)
      {  //look up/point to TR dir struct
         LocalDirMenu = TeensyROMMenu;  //default to root menu
         LocalNumItems = sizeof(TeensyROMMenu)/sizeof(StructMenuItem);
         
         if(strcmp((char*)pDataStart, "//") !=0)
         {  // not root, find dir menu
            pDataStart[strlen((char*)pDataStart)-1]=0; //remove the "/"
            int16_t MenuNum = FindTRMenuItem(LocalDirMenu, LocalNumItems, (char*)pDataStart);
            pDataStart[strlen((char*)pDataStart)]='/'; //put the "/" back
            if(MenuNum<0)
            {
               Printf_dbg("No TR Dir \"%s\"\n", pDataStart);
               //Somehow notify user?  
               //return; //leave at default (root) and probably fail trying to execute
            }
            else
            {
               //point at sub-dir
               LocalDirMenu = (StructMenuItem*)TeensyROMMenu[MenuNum].Code_Image;
               LocalNumItems = TeensyROMMenu[MenuNum].Size/sizeof(StructMenuItem);
            }
            Printf_dbg("TR Dir Num = %d\n", MenuNum);
         }
      }
      else
      {  //it's SD or USB, read dir from media
         FS *sourceFS = FSfromSourceID(MenuSourceID);
      
         if (sourceFS != NULL)  //only valid for SD/USB
         {  //load the full directory
            strcpy(DriveDirPath, (char*)pDataStart); //set up for LoadDirectory
            LoadDirectory(sourceFS); //loads DriveDirMenu, NumDrvDirMenuItems
            LocalDirMenu = DriveDirMenu;
            LocalNumItems = NumDrvDirMenuItems;
         }   
      }

      
      //remove dirs and unknown file types
      Printf_dbg("%d files found\n", LocalNumItems);
      uint16_t CleanLocalNumItems = 0;
      StructMenuItem *CleanLocalDirMenu[MaxMenuItems];

      for(uint16_t FNum=0; FNum<LocalNumItems; FNum++)
      {
         //Printf_dbg("%4d %2d %s\n", FNum, LocalDirMenu[FNum].ItemType, LocalDirMenu[FNum].Name);
         if (LocalDirMenu[FNum].ItemType >= rtFilePrg && LocalDirMenu[FNum].ItemType != rtFileHex)
         {
            CleanLocalDirMenu[CleanLocalNumItems] = &LocalDirMenu[FNum];
            Printf_dbg("%4d %2d %s\n", CleanLocalNumItems, CleanLocalDirMenu[CleanLocalNumItems]->ItemType, CleanLocalDirMenu[CleanLocalNumItems]->Name);
            CleanLocalNumItems++;
         }
      }
      if (CleanLocalNumItems)  //if there are valid files
      {  //pick a random one and append to path
         strcat((char*)pDataStart, CleanLocalDirMenu[random(0, CleanLocalNumItems)]->Name);
      }  //else return false; //let it try to launch, throw error
      Printf_dbg("Picked: %s\n", pDataStart);
   }
   else 
   {
      AllowTagRescan = false; //don't allow rescan of non-random tags.
   }
   
   //Printf_dbg("Launching...\n");
   RemoteLaunch(MenuSourceID, (char*)pDataStart, false);
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
         SendMsgPrintfln(" Couldn't verify tag :(");
         return;
      }
   }

   if (TryNum) SendMsgPrintfln("Verify took %d retries", TryNum);
   Printf_dbg("Verify read took %d retries\nUID Length = %d\n", TryNum, uidLength);
   
   if (uidLength != 7 && uidLength != 4)
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

   if (uidLength == 7)
   {  //Ultralight
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
   }
   else
   {  //Classic 1k
      uint16_t PageNum = 4, CharsWritten = 0;
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      
      while(CharsWritten<messageLength)
      {
         if(/*uidLength==4 &&*/ (PageNum%4)==0)  //Authentication only needed on classic/1K, on pg 4, 8, etc
         {
            if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, PageNum, 0, keya))
            {
               SendMsgPrintfln("Couldn't Authenticate pg %d", PageNum);
               return;
            }   
            else Printf_dbg("Authenticated pg %d\n", PageNum);
         }
            
         if (!nfc.mifareclassic_WriteDataBlock(PageNum, TagData+CharsWritten))
         {
            SendMsgPrintfln("Write Failed!");
            return;
         }

         PageNum++;
         if((PageNum%4)==3)PageNum++; //skip key pages         
         CharsWritten+=16;
      }
      
   }
   
   SendMsgPrintfln("Success!");
}

