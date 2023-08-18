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


uint8_t NumCrtChips = 0;
StructCrtChip CrtChips[MAX_CRT_CHIPS];

//these functions triggered from ISR and use current menu selection information while c64 code waits

void HandleExecution()
{
   StructMenuItem MenuSel = MenuSource[SelItemFullIdx]; //Condensed pointer to selected menu item
   
   if (MenuSel.ItemType == rtNone) 
   {
      SendMsgPrintfln("%s\r\nis not a valid item", MenuSel.Name);
      return;
   }
   if (MenuSel.ItemType == rtUnknown)
   {
      SendMsgPrintfln("%s\r\nUnknown Type", MenuSel.Name);
      return;
   }
   
   //if SD card or USB Drive,  update path & load dir   or   load file to RAM
   if (IO1[rWRegCurrMenuWAIT] == rmtSD || IO1[rWRegCurrMenuWAIT] == rmtUSBDrive) 
   {
      bool SD_nUSBDrive = (IO1[rWRegCurrMenuWAIT] == rmtSD);
      
      if (MenuSel.ItemType == rtFileHex)  //FW update from hex file
      {
         char FullFilePath[MaxPathLength+MaxItemNameLength+2];
         
         if (strlen(DriveDirPath) == 1 && DriveDirPath[0] == '/') sprintf(FullFilePath, "/%s", MenuSel.Name);  // at root
         else sprintf(FullFilePath, "%s/%s", DriveDirPath, MenuSel.Name);

         DoFlashUpdate(SD_nUSBDrive, FullFilePath);
         return;  //we're done here...
      }
      
      if (MenuSel.ItemType == rtDirectory)
      {  //edit path as needed and load the new directory from SD/USB
         
         if(strcmp(MenuSel.Name, UpDirString)==0)
         {  //up dir
            char * LastSlash = strrchr(DriveDirPath, '/'); //find last slash
            if (LastSlash != NULL) LastSlash[0] = 0;  //terminate it there 
         }
         else strcat(DriveDirPath, MenuSel.Name); //append selected dir name
         
         LoadDirectory(SD_nUSBDrive); 
         return;  //we're done here...
      }
      
      if(!LoadFile(&MenuSel, SD_nUSBDrive)) MenuSel.ItemType=rtUnknown; //mark unknown if error      

      MenuSel.Code_Image = RAM_Image;
   }
   else //Print name for Teensy Mem or USB Host item since they are already loaded
   {
      SendMsgPrintfln(MenuSel.Name);      
   }
    
   if (IO1[rWRegCurrMenuWAIT] == rmtUSBHost)
   {
      MenuSel.Code_Image = HOST_Image; 
   }
   
   if (MenuSel.ItemType == rtFileCrt) ParseCRTFile(&MenuSel); //will update MenuSel.ItemType & .Code_Image, if checks ok
 
   if (MenuSel.ItemType == rtFileP00) ParseP00File(&MenuSel); //will update MenuSel.ItemType & .Code_Image, if checks ok

   //has to be distilled down to one of these by this point, only ones supported so far.
   //Emulate ROM or prep PRG tranfer
   uint8_t CartLoaded = false;
   switch(MenuSel.ItemType)
   {
      case rtBin16k:
         SetGameAssert;
         SetExROMAssert;
         LOROM_Image = MenuSel.Code_Image;
         HIROM_Image = MenuSel.Code_Image+0x2000;
         CartLoaded=true;
         break;
      case rtBin8kHi:
         SetGameAssert;
         SetExROMDeassert;
         LOROM_Image = NULL;
         HIROM_Image = MenuSel.Code_Image;
         CartLoaded=true;
         NVIC_DISABLE_IRQ(IRQ_ENET); //disable ethernet interrupt when emulating VIC cycles
         NVIC_DISABLE_IRQ(IRQ_PIT);
         EmulateVicCycles = true;
         break;
      case rtBin8kLo:
         SetGameDeassert;
         SetExROMAssert;
         LOROM_Image = MenuSel.Code_Image;
         HIROM_Image = NULL;
         CartLoaded=true;
         break;
      case rtBinC128:
         SetGameDeassert;
         SetExROMDeassert;
         LOROM_Image = MenuSel.Code_Image;
         HIROM_Image = NULL;
         CartLoaded=true;
         break;      
      case rtFilePrg:
         //set up for transfer
         SendMsgPrintfln("PRG xfer %luK to $%04x:$%04x\n", 
            MenuSel.Size/1024,
            256*MenuSel.Code_Image[1]+MenuSel.Code_Image[0], 
            MenuSel.Size + 256*MenuSel.Code_Image[1]+MenuSel.Code_Image[0]);
         MenuSource[SelItemFullIdx].Code_Image = MenuSel.Code_Image; 
         MenuSource[SelItemFullIdx].Size = MenuSel.Size; //only copy the pointer & Size back, not type
         IO1[rRegStrAddrLo]=MenuSel.Code_Image[0];
         IO1[rRegStrAddrHi]=MenuSel.Code_Image[1];
         IO1[rRegStrAvailable]=0xff;
         StreamOffsetAddr = 2; //set to start of data
         break;
      case rtUnknown: //had to have been marked unknown after check at start
         //SendMsgFailed();
         SendMsgPrintfln(" :(");
         break;
      default:
         SendMsgPrintfln("Unk Item Type: %d", MenuSel.ItemType);
         break;
   }
   
   if (CartLoaded)
   {
      doReset=true;
      IOHandlerInitToNext();
   }

}

void MenuChange()
{
   switch(IO1[rWRegCurrMenuWAIT])
   {
      case rmtTeensy:
         MenuSource = TeensyROMMenu; 
         SetNumItems(sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]));
         break;
      case rmtSD:
         stpcpy(DriveDirPath, "/");
         // SD.begin takes 3 seconds for fail/unpopulated, 20-200mS populated
         if (SD.begin(BUILTIN_SDCARD)) LoadDirectory(true);
         else SetNumItems(0);
         MenuSource = DriveDirMenu; 
         break;
      case rmtUSBDrive:
         stpcpy(DriveDirPath, "/");
         LoadDirectory(false);
         MenuSource = DriveDirMenu; 
         break;
      case rmtUSBHost:
         MenuSource = &USBHostMenu; 
         SetNumItems(NumUSBHostItems);
         break;
   }
}

bool LoadFile(StructMenuItem* MyMenuItem, bool SD_nUSBDrive) 
{
   char FullFilePath[MaxPathLength+MaxItemNameLength+2];

   if (strlen(DriveDirPath) == 1 && DriveDirPath[0] == '/') sprintf(FullFilePath, "%s%s", DriveDirPath, MenuSource[SelItemFullIdx].Name);  // at root
   else sprintf(FullFilePath, "%s/%s", DriveDirPath, MenuSource[SelItemFullIdx].Name);
      
   SendMsgPrintfln("Loading:\r\n%s", FullFilePath);

   File myFile;
   if (SD_nUSBDrive) myFile= SD.open(FullFilePath, FILE_READ);
   else myFile= firstPartition.open(FullFilePath, FILE_READ);
   
   if (!myFile) 
   {
      SendMsgPrintfln("File Not Found");
      return false;
   }
   
   uint32_t FileSize = myFile.size();
   SendMsgPrintfln("Size: %lu bytes", FileSize);
   free(RAM_Image);
   
   if(RAM2BytesFree() <= FileSize)
   {
      SendMsgPrintfln("Max size: %lu bytes", RAM2BytesFree());
      myFile.close();
      return false;
   }
   
   RAM_Image = (uint8_t*)malloc(FileSize);

   uint32_t count=0;
   while (myFile.available() && count < FileSize) RAM_Image[count++]=myFile.read();

   if (count != FileSize)
   {
      SendMsgPrintfln("Size Mismatch");
      myFile.close();
      return false;
   }

   MyMenuItem->Size = count;
   myFile.close();
   SendMsgPrintfln("Done");
   return true;
}

void LoadDirectory(bool SD_nUSBDrive) 
{
   uint16_t ItemNum = 0;
   File dir;
      
   if (SD_nUSBDrive) dir = SD.open(DriveDirPath);//SD card
   else dir = firstPartition.open(DriveDirPath); //USB Drive
   
   if (!(strlen(DriveDirPath) == 1 && DriveDirPath[0] == '/'))
   {  // *not* at root, add up dir option
      ItemNum++;
      strcpy(DriveDirMenu[0].Name, UpDirString);
      DriveDirMenu[0].ItemType = rtDirectory;
   }
   
   const char *filename;
   
   while (File entry = dir.openNextFile()) 
   {
      filename = entry.name();
      if (entry.isDirectory())
      {
         DriveDirMenu[ItemNum].Name[0] = '/';
         memcpy(DriveDirMenu[ItemNum].Name+1, filename, MaxItemNameLength-1);
      }
      else memcpy(DriveDirMenu[ItemNum].Name, filename, MaxItemNameLength);
      
      DriveDirMenu[ItemNum].Name[MaxItemNameLength-1]=0; //terminate in case too long. 
      //if (strlen(filename)>MaxItemNameLength-1) DriveDirMenu[ItemNum].Name[MaxItemNameLength-2]='*';
      
      if (entry.isDirectory()) DriveDirMenu[ItemNum].ItemType = rtDirectory;
      else 
      {
         //char* Extension = (filename + strlen(filename) - 4);
         //this way marks too long names as unknown:
         char* Extension = (DriveDirMenu[ItemNum].Name + strlen(DriveDirMenu[ItemNum].Name) - 4);
         for(uint8_t cnt=1; cnt<=3; cnt++) if(Extension[cnt]>='A' && Extension[cnt]<='Z') Extension[cnt]+=32;
         
         if (strcmp(Extension, ".prg")==0) DriveDirMenu[ItemNum].ItemType = rtFilePrg;
         else if (strcmp(Extension, ".crt")==0) DriveDirMenu[ItemNum].ItemType = rtFileCrt;
         else if (strcmp(Extension, ".hex")==0) DriveDirMenu[ItemNum].ItemType = rtFileHex;
         else if (strcmp(Extension, ".p00")==0) DriveDirMenu[ItemNum].ItemType = rtFileP00;
         else DriveDirMenu[ItemNum].ItemType = rtUnknown;
      }
      
      //Serial.printf("%d- %s\n", ItemNum, DriveDirMenu[ItemNum].Name); 
      entry.close();
      if (ItemNum++ == MaxMenuItems)
      {
         Serial.println("Too many files!"); //no messaging in dir load
         break;
      }
   }
   
   SetNumItems(ItemNum);
}

void ParseP00File(StructMenuItem* MyMenuItem)   
{  //update .ItemType(rtUnknown or rtFilePrg) & .Code_Image
   //Sources:
   // https://www.infinite-loop.at/Power64/Documentation/Power64-ReadMe/AE-File_Formats.html
   
   SendMsgPrintfln("Parsing P00 File ");
   if(strcmp((char*)MyMenuItem->Code_Image, "C64File") == 0)
   {
      MyMenuItem->Code_Image += 26;
      MyMenuItem->ItemType = rtFilePrg;
   }
   else
   {
      SendMsgPrintfln("\"C64File\" not found");
      MyMenuItem->ItemType = rtUnknown;
   }
   SendMsgOK();
}

void ParseCRTFile(StructMenuItem* MyMenuItem)   
{  //update .ItemType(rtUnknown or rtBin*) & .Code_Image
   //Sources:
   // https://codebase64.org/doku.php?id=base:crt_file_format
   // https://rr.pokefinder.org/wiki/CRT_ID
   // https://vice-emu.sourceforge.io/vice_17.html#SEC369
   // http://ist.uwaterloo.ca/~schepers/formats/CRT.TXT
   
   uint8_t* CRT_Image = MyMenuItem->Code_Image;
   MyMenuItem->ItemType = rtUnknown; //default to fail

   SendMsgPrintfln("Parsing CRT File");
   SendMsgPrintfln("CRT image size: %luK  $%08x", MyMenuItem->Size/1024, MyMenuItem->Size);
   
   uint8_t  C128Cart = (memcmp(CRT_Image, "C128 CARTRIDGE", 14)==0);
   if (C128Cart) SendMsgPrintfln("C128 crt");
   else if (memcmp(CRT_Image, "C64 CARTRIDGE", 13)==0) SendMsgPrintfln("C64 crt");
   else
   {
      SendMsgPrintfln("\"C64/128 CARTRIDGE\" not found");
      return;
   }
   
   uint32_t HeaderLen = toU32(CRT_Image+0x10);
   SendMsgPrintfln("Header len: $%08x", HeaderLen);
   if (HeaderLen < 0x40) 
   {
      SendMsgPrintfln(" adjusted to $40");
      HeaderLen = 0x40;
   }

   SendMsgPrintfln("Ver: %02x.%02x", CRT_Image[0x14], CRT_Image[0x15]);
   
   int16_t HWType = (int16_t)toU16(CRT_Image+0x16);
   SendMsgPrintfln("HW Type: %d ($%04x)", HWType, (uint16_t)HWType);
   switch (HWType)
      {
      case Cart_Generic:
         //IO1[rwRegNextIOHndlr] = IOH_None;  //leave IOH as default/user set for generic
         break;
      case Cart_MIDI_Datel:
         IO1[rwRegNextIOHndlr] = IOH_MIDI_Datel;
         break;
      case Cart_MIDI_Sequential:
         IO1[rwRegNextIOHndlr] = IOH_MIDI_Sequential;
         break;
      case Cart_MIDI_Passport:
         IO1[rwRegNextIOHndlr] = IOH_MIDI_Passport;
         break;
      case Cart_MIDI_Namesoft:
         IO1[rwRegNextIOHndlr] = IOH_MIDI_NamesoftIRQ;
         break;
      case Cart_SwiftLink:
         IO1[rwRegNextIOHndlr] = IOH_Swiftlink;
         break;
      case Cart_EpyxFastload:
         IO1[rwRegNextIOHndlr] = IOH_EpyxFastLoad;
         break;
      case Cart_MagicDesk:
         IO1[rwRegNextIOHndlr] = IOH_MagicDesk;
         break;
      case Cart_Dinamic:   
         IO1[rwRegNextIOHndlr] = IOH_Dinamic;
         break;
      default:
         SendMsgPrintfln("Unsupported HW Type (so far)");
         return;
      }
   
   uint8_t EXROM = CRT_Image[0x18];
   uint8_t GAME = CRT_Image[0x19];
   SendMsgPrintfln("EXROM: %d   GAME: %d", EXROM, GAME);
   
   SendMsgPrintfln("Name: %s", (CRT_Image+0x20));
   
   //On to CHIP packet(s)...
   uint32_t PacketLength;
   NumCrtChips = 0;
   uint8_t *ChipImage = CRT_Image + HeaderLen;
   
   Serial.printf("\n Chp# Length    Type  Bank  Addr  Size\n");
   while ((uint32_t)ChipImage-(uint32_t)CRT_Image < MyMenuItem->Size)
   {   
      if (memcmp(ChipImage, "CHIP", 4)!=0)
      {
         SendMsgPrintfln("\"CHIP\" not found in #%d", NumCrtChips);
         return;
      }
     
      CrtChips[NumCrtChips].LoadAddress = toU16(ChipImage+0x0C);
      CrtChips[NumCrtChips].ROMSize = toU16(ChipImage+0x0E);
      CrtChips[NumCrtChips].ChipROM = ChipImage+0x10;
      PacketLength = toU32(ChipImage+0x04);
      
      Serial.printf(" #%03d $%08x $%04x $%04x $%04x $%04x\n", 
         NumCrtChips, PacketLength, toU16(ChipImage+0x08), toU16(ChipImage+0x0A), 
         CrtChips[NumCrtChips].LoadAddress, CrtChips[NumCrtChips].ROMSize);

      ChipImage += PacketLength;
      NumCrtChips++;
   }
       
   SendMsgPrintfln("CRT Image verified, %d Chip(s) found", NumCrtChips); 
   //We have a good CRT image, is it a config we support?
   
   MyMenuItem->Code_Image = CrtChips[0].ChipROM;
   
   if(HWType==Cart_EpyxFastload && CrtChips[0].LoadAddress == 0x8000 && CrtChips[0].ROMSize == 0x2000) //sets EXROM & GAME high in crt
   {
      MyMenuItem->ItemType = rtBin8kLo;
      SendMsgPrintfln(" EpyxFastload 8kLo config");
      return;
   }
   
   if(EXROM==0                  && CrtChips[0].LoadAddress == 0x8000 && CrtChips[0].ROMSize == 0x2000) //GAME is usually==1, Centiped calls for low but doesn't use it
   {
      MyMenuItem->ItemType = rtBin8kLo;
      SendMsgPrintfln(" 8kLo config");
      return;
   }      

   if(EXROM==1 && GAME==0 && CrtChips[0].LoadAddress == 0xe000 && CrtChips[0].ROMSize == 0x2000)
   {
      MyMenuItem->ItemType = rtBin8kHi;
      SendMsgPrintfln(" 8kHi/Ultimax config");
      return;
   }      

   if(EXROM==0 && GAME==0 && CrtChips[0].LoadAddress == 0x8000 && CrtChips[0].ROMSize == 0x4000)
   {
      MyMenuItem->ItemType = rtBin16k;
      SendMsgPrintfln(" 16k config");
      return;
   }      
   
   if(EXROM==0 && GAME==0 && CrtChips[0].LoadAddress == 0x0000 && CrtChips[0].ROMSize == 0x2000 && C128Cart)
   {
      MyMenuItem->ItemType = rtBinC128;
      SendMsgPrintfln(" C128 config");
      return;
   }      
   
   SendMsgPrintfln("HW config unknown!");
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


void SendMsgOK()
{
   SendMsgPrintf("OK");
}

void SendMsgFailed()
{
   SendMsgPrintf("Failed!");
}

void SendMsgPrintfln(const char *Fmt, ...)
{
   va_list ap;
   va_start(ap,Fmt);
   vsprintf(SerialStringBuf, Fmt, ap); 
   va_end(ap);
   
   //add \r\n to the beginning:
   for(uint16_t pos=strlen(SerialStringBuf)+2; pos>1; pos--) SerialStringBuf[pos]=SerialStringBuf[pos-2];
   SerialStringBuf[0] = '\r';
   SerialStringBuf[1] = '\n';
   
   SendMsgSerialStringBuf();
}

void SendMsgPrintf(const char *Fmt, ...)
{
   va_list ap;
   va_start(ap,Fmt);
   vsprintf(SerialStringBuf, Fmt, ap); 
   va_end(ap);
   SendMsgSerialStringBuf() ;
}

void SendMsgSerialStringBuf() 
{  //SerialStringBuf already populated
   Serial.printf("%s<--", SerialStringBuf);
   Serial.flush();
   IO1[rwRegStatus] = rsC64Message; //tell C64 there's a message
   uint32_t beginWait = millis();
   //wait up to 3 sec for C64 to read message:
   while (millis()-beginWait<3000) if(IO1[rwRegStatus] == rsContinue) return;
   Serial.printf("\nTimeout!\n");
}

