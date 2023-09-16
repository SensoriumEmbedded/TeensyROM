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
   StructMenuItem MenuSelCpy = MenuSource[SelItemFullIdx]; //local copy selected menu item to modify
   
   if (MenuSelCpy.ItemType == rtNone) //should no longer reach here
   {
      SendMsgPrintfln("%s\r\nis not a valid item", MenuSelCpy.Name);
      return;
   }
   if (MenuSelCpy.ItemType == rtUnknown)
   {
      SendMsgPrintfln("%s\r\nUnknown File Type", MenuSelCpy.Name);
      return;
   }
   
   FS *sourceFS = &firstPartition;
   switch(IO1[rWRegCurrMenuWAIT])
   {
      case rmtSD:
         sourceFS = &SD;
      case rmtUSBDrive:
      
         if (MenuSelCpy.ItemType == rtFileHex)  //FW update from hex file
         {
            char FullFilePath[MaxPathLength+MaxItemNameLength+2];
            
            if (PathIsRoot()) sprintf(FullFilePath, "/%s", MenuSelCpy.Name);  // at root
            else sprintf(FullFilePath, "%s/%s", DriveDirPath, MenuSelCpy.Name);

            DoFlashUpdate(sourceFS, FullFilePath);
            return;  //we're done here...
         }
         
         if (MenuSelCpy.ItemType == rtDirectory)
         {  //edit path as needed and load the new directory from SD/USB
            
            if(strcmp(MenuSelCpy.Name, UpDirString)==0)
            {  //up dir
               UpDirectory();
               return;  //we're done here...
            }
            
            strcat(DriveDirPath, MenuSelCpy.Name); //append selected dir name
            LoadDirectory(sourceFS); 
            return;  //we're done here...
         }
         
         if(!LoadFile(&MenuSelCpy, sourceFS)) return;     

         MenuSelCpy.Code_Image = RAM_Image;
         break;
         
      case rmtTeensy:  //not many size checks as this is loading internally
         
         if (MenuSelCpy.ItemType == rtDirectory)
         {
            if(strcmp(MenuSelCpy.Name, UpDirString)==0) MenuChange(); //only 1 level, returning to root
            else 
            {
               MenuSource = (StructMenuItem*)MenuSelCpy.Code_Image;
               SetNumItems(MenuSelCpy.Size/sizeof(StructMenuItem));
               strcat(DriveDirPath, MenuSelCpy.Name); //append selected dir name
            }
            return;
         }
         
         SendMsgPrintfln(MenuSelCpy.Name); 
         if (MenuSelCpy.ItemType == rtFileCrt)
         {  //load the CRT into RAM
            uint8_t EXROM;
            uint8_t GAME;
            
            //load header and parse (sends error messages)
            if (!ParseCRTHeader(&MenuSelCpy, &EXROM, &GAME)) return;
            
            //IO1[rwRegNextIOHndlr] is now assigned from crt!
            //process Chip Packets
            uint8_t *ptrChipOffset = MenuSelCpy.Code_Image + CRT_MAIN_HDR_LEN; //Skip header
            FreeCrtChips();  //clears any previous and resets NumCrtChips
            Printf_dbg("\n Chp# Length    Type  Bank  Addr  Size\n");
            while (MenuSelCpy.Code_Image + MenuSelCpy.Size - ptrChipOffset > 1) //allow for off by 1 sometimes caused by bin2header
            {
               if (!ParseChipHeader(ptrChipOffset)) //sends error messages
               {
                  FreeCrtChips();
                  IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
                  return;        
               }
               ptrChipOffset += CRT_CHIP_HDR_LEN;
               memcpy(CrtChips[NumCrtChips].ChipROM, ptrChipOffset, CrtChips[NumCrtChips].ROMSize);
               ptrChipOffset += CrtChips[NumCrtChips].ROMSize;
               NumCrtChips++;
            }
            
            //check configuration (sends error messages)
            if (!SetTypeFromCRT(&MenuSelCpy, EXROM, GAME)) 
            {
               IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
               return;
            }
         }
         
         else
         {  //non-CRT: copy the whole thing into the RAM1 buffer
            memcpy(RAM_Image, MenuSelCpy.Code_Image, MenuSelCpy.Size);
            MenuSelCpy.Code_Image = RAM_Image;   
         }            
         SendMsgPrintfln("Copied to RAM"); 
         break;
         
      case rmtUSBHost:
         SendMsgPrintfln(MenuSelCpy.Name);  
         MenuSelCpy.Code_Image = HOST_Image; 
         break;
   }
   
   //if (MenuSelCpy.ItemType == rtFileCrt) ParseCRTFile(&MenuSelCpy); //will update MenuSelCpy.ItemType & .Code_Image, if checks ok
 
   if (MenuSelCpy.ItemType == rtFileP00) ParseP00File(&MenuSelCpy); //will update MenuSelCpy.ItemType & .Code_Image, if checks ok

   //has to be distilled down to one of these by this point, only ones supported so far.
   //Emulate ROM or prep PRG tranfer
   uint8_t CartLoaded = false;
   
   switch(MenuSelCpy.ItemType)
   {
      case rtBin16k:
         SetGameAssert;
         SetExROMAssert;
         LOROM_Image = MenuSelCpy.Code_Image;
         HIROM_Image = MenuSelCpy.Code_Image+0x2000;
         CartLoaded=true;
         break;
      case rtBin8kHi:
         SetGameAssert;
         SetExROMDeassert;
         LOROM_Image = NULL;
         HIROM_Image = MenuSelCpy.Code_Image;
         CartLoaded=true;
         NVIC_DISABLE_IRQ(IRQ_ENET); //disable ethernet interrupt when emulating VIC cycles
         NVIC_DISABLE_IRQ(IRQ_PIT);
         EmulateVicCycles = true;
         break;
      case rtBin8kLo:
         SetGameDeassert;
         SetExROMAssert;
         LOROM_Image = MenuSelCpy.Code_Image;
         HIROM_Image = NULL;
         CartLoaded=true;
         break;
      case rtBinC128:
         SetGameDeassert;
         SetExROMDeassert;
         LOROM_Image = MenuSelCpy.Code_Image;
         HIROM_Image = NULL;
         CartLoaded=true;
         break;      
      case rtFilePrg:
         //set up for transfer
         SendMsgPrintfln("PRG xfer %luK to $%04x:$%04x\n", 
            MenuSelCpy.Size/1024,
            256*MenuSelCpy.Code_Image[1]+MenuSelCpy.Code_Image[0], 
            MenuSelCpy.Size + 256*MenuSelCpy.Code_Image[1]+MenuSelCpy.Code_Image[0]);
         XferImage = MenuSelCpy.Code_Image; 
         XferSize  = MenuSelCpy.Size; 
         IO1[rRegStrAddrLo] = XferImage[0];
         IO1[rRegStrAddrHi] = XferImage[1];
         IO1[rRegStrAvailable] = 0xff;
         StreamOffsetAddr = 2; //set to start of data
         break;
      case rtUnknown: //had to have been marked unknown after check at start
         //SendMsgFailed();
         SendMsgPrintfln(" :(");
         break;
      default:
         SendMsgPrintfln("Unk Item Type: %d", MenuSelCpy.ItemType);
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
   stpcpy(DriveDirPath, "/");
   switch(IO1[rWRegCurrMenuWAIT])
   {
      case rmtTeensy:
         MenuSource = TeensyROMMenu; 
         SetNumItems(sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]));
         break;
      case rmtSD:
         SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
         LoadDirectory(&SD); //do this regardless of SD.begin result to populate one entry w/ message
         MenuSource = DriveDirMenu; 
         break;
      case rmtUSBDrive:
         LoadDirectory(&firstPartition);
         MenuSource = DriveDirMenu; 
         break;
      case rmtUSBHost:
         MenuSource = &USBHostMenu; 
         SetNumItems(NumUSBHostItems);
         break;
   }
   IO1[rwRegCursorItemOnPg] = 0;
}

bool LoadFile(StructMenuItem* MyMenuItem, FS *sourceFS) 
{
   char FullFilePath[MaxPathLength+MaxItemNameLength+2];

   if (PathIsRoot()) sprintf(FullFilePath, "%s%s", DriveDirPath, MyMenuItem->Name);  // at root
   else sprintf(FullFilePath, "%s/%s", DriveDirPath, MyMenuItem->Name);
      
   SendMsgPrintfln("Loading:\r\n%s", FullFilePath);

   File myFile = sourceFS->open(FullFilePath, FILE_READ);
   
   if (!myFile) 
   {
      SendMsgPrintfln("File Not Found");
      return false;
   }
   
   MyMenuItem->Size = myFile.size();
   uint32_t count=0;
   
   if (MyMenuItem->ItemType == rtFileCrt)
   {  //load the CRT
      uint8_t lclBuf[CRT_MAIN_HDR_LEN];
      uint8_t EXROM;
      uint8_t GAME;
      
      if (MyMenuItem->Size < 0x1000)
      {
         SendMsgPrintfln("Too Short for CRT");
         myFile.close();
         return false;        
      }
      
      //load header and parse
      for (count = 0; count < CRT_MAIN_HDR_LEN; count++) lclBuf[count]=myFile.read(); //Read main header
      MyMenuItem->Code_Image = lclBuf;
      if (!ParseCRTHeader(MyMenuItem, &EXROM, &GAME)) //sends error messages
      {
         myFile.close();
         return false;        
      }
      
      //IO1[rwRegNextIOHndlr] is now assigned from crt!
      //process Chip Packets
      FreeCrtChips();  //clears any previous and resets NumCrtChips
      Printf_dbg("\n Chp# Length    Type  Bank  Addr  Size\n");
      while (myFile.available())
      {
         for (count = 0; count < CRT_CHIP_HDR_LEN; count++) lclBuf[count]=myFile.read(); //Read chip header
         if (!ParseChipHeader(lclBuf)) //sends error messages
         {
            myFile.close();
            FreeCrtChips();
            IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
            RedirectEmptyDriveDirMenu();
            return false;        
         }
         for (count = 0; count < CrtChips[NumCrtChips].ROMSize; count++) CrtChips[NumCrtChips].ChipROM[count]=myFile.read();//read in ROM info:
         NumCrtChips++;
      }
      
      //check configuration
      if (!SetTypeFromCRT(MyMenuItem, EXROM, GAME)) //sends error messages
      {
         myFile.close();
         FreeCrtChips();
         IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
         RedirectEmptyDriveDirMenu();
         return false;        
      }
   }
   
   else //non-CRT: Load the whole thing into the RAM1 buffer
   {
      if (MyMenuItem->Size > RAM_ImageSize)
      {
         SendMsgPrintfln("Non-CRT file too large");
         myFile.close();
         return false;
      }
      
      while (myFile.available() && count < MyMenuItem->Size) RAM_Image[count++]=myFile.read();

      myFile.close();
      if (count != MyMenuItem->Size)
      {
         SendMsgPrintfln("Size Mismatch");
         myFile.close();
         return false;
      }
   }
   
   SendMsgPrintfln("Done");
   myFile.close();
   return true;      
}

void LoadDirectory(FS *sourceFS) 
{
   
   if (DriveDirMenu == NULL) 
   {
      DriveDirMenu = (StructMenuItem*)malloc(MaxMenuItems*sizeof(StructMenuItem));
   }
   else
   {
      //free/clear prev loaded directory
      for(uint16_t Num=0; Num < NumDrvDirMenuItems; Num++) free(DriveDirMenu[Num].Name);
   }
   NumDrvDirMenuItems = 0;
   
   File dir = sourceFS->open(DriveDirPath);
   
   if (!PathIsRoot())
   {  // *not* at root, add up dir option
      DriveDirMenu[0].ItemType = rtDirectory;
      AddDirEntry(UpDirString);
   }
   
   const char *filename;
   uint32_t beginWait = millis();
   
   while (File entry = dir.openNextFile()) 
   {
      filename = entry.name();
      if (entry.isDirectory())
      {
         DriveDirMenu[NumDrvDirMenuItems].Name = (char*)malloc(strlen(filename)+2);
         DriveDirMenu[NumDrvDirMenuItems].Name[0] = '/';
         strcpy(DriveDirMenu[NumDrvDirMenuItems].Name+1, filename);
         DriveDirMenu[NumDrvDirMenuItems].ItemType = rtDirectory;
      }
      else //it's a file
      {
         DriveDirMenu[NumDrvDirMenuItems].Name = (char*)malloc(strlen(filename)+1);
         strcpy(DriveDirMenu[NumDrvDirMenuItems].Name, filename);

         //convert extension to lower case:
         char* Extension = DriveDirMenu[NumDrvDirMenuItems].Name + strlen(DriveDirMenu[NumDrvDirMenuItems].Name) - 4;
         for(uint8_t cnt=1; cnt<=3; cnt++) if(Extension[cnt]>='A' && Extension[cnt]<='Z') Extension[cnt]+=32;
         
         if (strcmp(Extension, ".prg")==0) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFilePrg;
         else if (strcmp(Extension, ".crt")==0) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFileCrt;
         else if (strcmp(Extension, ".hex")==0) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFileHex;
         else if (strcmp(Extension, ".p00")==0) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFileP00;
         else DriveDirMenu[NumDrvDirMenuItems].ItemType = rtUnknown;
      }
      
      //Serial.printf("%d- %s\n", NumDrvDirMenuItems, DriveDirMenu[NumDrvDirMenuItems].Name); 
      entry.close();
      if (++NumDrvDirMenuItems == MaxMenuItems)
      {
         Serial.println("Too many files!"); //no messaging in dir load
         break;
      }
   }
   
   Printf_dbg("Loaded %d items in %lumS from %s\n", NumDrvDirMenuItems, (millis()-beginWait), DriveDirPath);
   
   //alphabetize the directory list
   StructMenuItem TempMenuItem;
   for(uint16_t i=0; i<NumDrvDirMenuItems; i++)
      for(uint16_t j=i+1; j<NumDrvDirMenuItems; j++)
      {
           if(strcasecmp(DriveDirMenu[i].Name,DriveDirMenu[j].Name)>0)
           {
              TempMenuItem    = DriveDirMenu[i];
              DriveDirMenu[i] = DriveDirMenu[j];
              DriveDirMenu[j] = TempMenuItem;
           }
      }   
   
   if(NumDrvDirMenuItems == 0) //empty or missing drive, always need at least one entry
   {
      DriveDirMenu[0].ItemType = rtNone;
      AddDirEntry("<Empty>");
   }
   
   SetNumItems(NumDrvDirMenuItems);
}

void AddDirEntry(const char *EntryString)
{
   DriveDirMenu[NumDrvDirMenuItems].Name = (char*)malloc(strlen(EntryString)+1);
   strcpy(DriveDirMenu[NumDrvDirMenuItems].Name, EntryString);
   NumDrvDirMenuItems++;
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
 
bool ParseCRTHeader(StructMenuItem* MyMenuItem, uint8_t *EXROM, uint8_t *GAME)   
{  
   //Sources:
   // https://codebase64.org/doku.php?id=base:crt_file_format
   // https://rr.pokefinder.org/wiki/CRT_ID
   // https://vice-emu.sourceforge.io/vice_17.html#SEC369
   // http://ist.uwaterloo.ca/~schepers/formats/CRT.TXT
   
   uint8_t* CRT_Image = MyMenuItem->Code_Image;

   SendMsgPrintfln("Parsing CRT File");
   SendMsgPrintfln("CRT image size: %luK  $%08x", MyMenuItem->Size/1024, MyMenuItem->Size);
   
   if (memcmp(CRT_Image, "C128 CARTRIDGE", 14)==0) SendMsgPrintfln("C128 crt");
   else if (memcmp(CRT_Image, "C64 CARTRIDGE", 13)==0) SendMsgPrintfln("C64 crt");
   else
   {
      SendMsgPrintfln("\"C64/128 CARTRIDGE\" not found");
      return false;
   }
   
   if (toU32(CRT_Image+0x10) != CRT_MAIN_HDR_LEN) //Header Length
   {
      SendMsgPrintfln("Unexp Header Len: $%08x", toU32(CRT_Image+0x10));
      return false;
   }

   SendMsgPrintfln("Ver: %02x.%02x", CRT_Image[0x14], CRT_Image[0x15]);
   
   uint16_t HWType = toU16(CRT_Image+0x16);
   SendMsgPrintfln("HW Type: %d ($%04x)", (int16_t)HWType, HWType);
   
   if (HWType != Cart_Generic) //leave IOH as default/user set for generic
   {
      if (!AssocHWID_IOH(HWType))
      {
         SendMsgPrintfln("Unsupported HW Type (so far)");
         return false;         
      }
   }
   

   *EXROM = CRT_Image[0x18];
   *GAME = CRT_Image[0x19];
   SendMsgPrintfln("EXROM: %d   GAME: %d", *EXROM, *GAME);
   
   SendMsgPrintfln("Name: %s", (CRT_Image+0x20));
   return true;
}
   
bool ParseChipHeader(uint8_t* ChipHeader)   
{
   static uint8_t *ptrRAM_ImageEnd = NULL;
   
   if (memcmp(ChipHeader, "CHIP", 4)!=0)
   {
      SendMsgPrintfln("\"CHIP\" not found in #%d", NumCrtChips);
      return false;
   }
      
   //clutters C64 disaply, just send to serial/debug
   Printf_dbg(" #%03d $%08x $%04x $%04x $%04x $%04x in RAM", 
      NumCrtChips, toU32(ChipHeader+0x04), toU16(ChipHeader+0x08), toU16(ChipHeader+0x0A), 
      toU16(ChipHeader+0x0C), toU16(ChipHeader+0x0E));
       
   CrtChips[NumCrtChips].BankNum = toU16(ChipHeader+0x0A);
   CrtChips[NumCrtChips].LoadAddress = toU16(ChipHeader+0x0C);
   CrtChips[NumCrtChips].ROMSize = toU16(ChipHeader+0x0E);
   
   //chips in main buffer, then malloc in RAM2.  Drop Directory names if space needed?
   if (NumCrtChips == 0) ptrRAM_ImageEnd = RAM_Image; //init RAM1 Buffer pointer

   //First try RAM1:
   if (CrtChips[NumCrtChips].ROMSize + (uint32_t)ptrRAM_ImageEnd - (uint32_t)RAM_Image <= RAM_ImageSize)
   {
      CrtChips[NumCrtChips].ChipROM = ptrRAM_ImageEnd;
      ptrRAM_ImageEnd += CrtChips[NumCrtChips].ROMSize;
      Printf_dbg("1");
   }
   else //then try RAM2
   {
      while(NULL == (CrtChips[NumCrtChips].ChipROM = (uint8_t*)malloc(CrtChips[NumCrtChips].ROMSize)))
      {
         if (DriveDirMenu == NULL)
         {
            SendMsgPrintfln("Not enough room"); 
            return false;         
         }
         else
         {
            //free/clear prev loaded directory
            Printf_dbg("Dir info removed"); 
            for(uint16_t Num=0; Num < NumDrvDirMenuItems; Num++) free(DriveDirMenu[Num].Name);
            free(DriveDirMenu); DriveDirMenu = NULL;
         }
      }
      Printf_dbg("2");
   }
   Printf_dbg(" %08x\n", (uint32_t)CrtChips[NumCrtChips].ChipROM);
   return true;
}
 
void FreeCrtChips()
{ //free chips allocated in RAM2 and reset NumCrtChips
   for(uint16_t cnt=0; cnt < NumCrtChips; cnt++) 
      if((uint32_t)CrtChips[cnt].ChipROM >= 0x20200000) free(CrtChips[cnt].ChipROM);
   NumCrtChips = 0;
}
 
void RedirectEmptyDriveDirMenu()
{
   if((IO1[rWRegCurrMenuWAIT] == rmtSD || IO1[rWRegCurrMenuWAIT] == rmtUSBDrive) && DriveDirMenu == NULL)
   {  //return to Teensy Menu instead of re-loading dir to save time
      IO1[rWRegCurrMenuWAIT] = rmtTeensy;
      MenuChange();
   }
}

bool PathIsRoot()
{
   return (strlen(DriveDirPath) == 1 && DriveDirPath[0] == '/');
}

bool SetTypeFromCRT(StructMenuItem* MyMenuItem, uint8_t EXROM, uint8_t GAME)   
{
   SendMsgPrintfln("%d Chip(s) found/loaded", NumCrtChips); 
   MyMenuItem->Code_Image = CrtChips[0].ChipROM;
   
   //check configuration
      
   if(CrtChips[0].LoadAddress == 0x8000 && CrtChips[0].ROMSize == 0x2000) 
   //Usually GAME ==1 and EXROM==0
   //Centiped calls for GAME low but doesn't use 16k
   //Epyx Fastload sets EXROM & GAME high in crt
   {
      MyMenuItem->ItemType = rtBin8kLo;
      SendMsgPrintfln(" 8kLo config");
      return true;
   }      

   if(                                     CrtChips[0].ROMSize == 0x2000 && EXROM==1 && GAME==0)
   {
      MyMenuItem->ItemType = rtBin8kHi;
      SendMsgPrintfln(" 8kHi/Ultimax config");
      return true;
   }      

   if(CrtChips[0].LoadAddress == 0x8000                                  && EXROM==0 && GAME==0) //Zaxxon ROMSize is 0x1000, all others 0x4000
   {
      MyMenuItem->ItemType = rtBin16k;
      SendMsgPrintfln(" 16k config");
      return true;
   }      
   
   if(CrtChips[0].LoadAddress == 0x0000 && CrtChips[0].ROMSize == 0x2000 && EXROM==0 && GAME==0)
   {
      MyMenuItem->ItemType = rtBinC128;
      SendMsgPrintfln(" C128 config");
      return true;
   }      
   
   SendMsgPrintfln("HW config unknown");
   return false;
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

bool AssocHWID_IOH(uint16_t HWType)
{
   uint8_t Num = 0;
   
   while (Num < sizeof(HWID_IOH_Assoc)/sizeof(HWID_IOH_Assoc[0]))
   {
      if (HWType == HWID_IOH_Assoc[Num].HWID)
      {
         IO1[rwRegNextIOHndlr] = HWID_IOH_Assoc[Num].IOH;
         return true;
      }
      Num++;
   }
   return false;
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

