// MIT License
// 
// Copyright (c) 2024 Travis Smith
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


void HandleExecution()
{
   StructMenuItem MenuSelCpy = MenuSource[SelItemFullIdx]; //local copy selected menu item to modify
   IO1[rRegStrAvailable] = 0;    // default transfer start flag to stop in case of previous abort (such as text read abort)
   
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
   {  //find source based on current menu, perform device specific actions
      case rmtSD:
         sourceFS = &SD;
      case rmtUSBDrive:
         //USB or SD drive actions:
         if (MenuSelCpy.ItemType == rtFileHex)  //FW update from hex file
         {
            char FullFilePath[MaxNamePathLength];
            
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
         
         if (MenuSelCpy.ItemType == rtD64 ||
             MenuSelCpy.ItemType == rtD71 ||
             MenuSelCpy.ItemType == rtD81)
         {  //edit path as needed and load the new directory from SD/USB
            strcat(DriveDirPath, "/"); 
            strcat(DriveDirPath, MenuSelCpy.Name); //append selected d64 name as a dir
            LoadDxxDirectory(sourceFS, MenuSelCpy.ItemType); 
            strcat(DriveDirPath, "*"); //mark to indicate d64 file instead of "real" dir
            SetNumItems(NumDrvDirMenuItems);
            return;  //we're done here...
         }
         
         if(DriveDirPath[strlen(DriveDirPath)-1] == '*')
         { //load from D64/71/81 file
            if(!LoadDxxFile(&MenuSelCpy, sourceFS)) return;
         }
         else 
         {
            if(!LoadFile(sourceFS, DriveDirPath, &MenuSelCpy)) return;     
         }
         
         MenuSelCpy.Code_Image = RAM_Image;
         break;
         
      case rmtTeensy:  
         // local Teensy Flash menu actions
         // not many size checks as this is loading internally
         
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
               if (!ParseChipHeader(ptrChipOffset, "")) //sends error messages
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
         {  //non-CRT: copy the whole thing from flash into the RAM1 buffer
            memcpy(RAM_Image, MenuSelCpy.Code_Image, MenuSelCpy.Size);
            MenuSelCpy.Code_Image = RAM_Image;   
         }            
         SendMsgPrintfln("Copied to RAM"); 
         break;
         
   }
   
   //Have to do this differently for Flash vs ext media:
   // if (MenuSelCpy.ItemType == rtFileCrt) ParseCRTFile(&MenuSelCpy); //will update MenuSelCpy.ItemType & .Code_Image, if checks ok
 
   //will update MenuSelCpy.ItemType & .Code_Image, if checks ok:
   if (MenuSelCpy.ItemType == rtFileP00) ParseP00File(&MenuSelCpy); 
   
   //has to be distilled down to one of these by this point, only ones supported so far.
   //Emulate ROM or prep for tranfer
   uint8_t CartLoaded = false;

   switch(MenuSelCpy.ItemType)
   {
      case rtFileSID:
         XferImage = MenuSelCpy.Code_Image;
         XferSize = MenuSelCpy.Size;
         
         //save source/path/name for later use
         LatestSIDLoaded[0] = IO1[rWRegCurrMenuWAIT]; //set source
         if(LatestSIDLoaded[0] == rmtTeensy)
         { // built-in SID
            //figure out what menu dir we're in
            if (MenuSource == TeensyROMMenu) strcpy(LatestSIDLoaded + 1, "/"); //root
            else
            {
               //find sub-dir
               uint8_t DirNum = 0;
               while(MenuSource != (StructMenuItem*)TeensyROMMenu[DirNum].Code_Image)
               {
                  //MenuSelCpy.Code_Image;
                  if (++DirNum == sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]))
                  {
                     Printf_dbg("TR Dir not found\n"); //what now?
                     DirNum = 3;  //choose SID dir?
                     break;
                  }
               }
               strcpy(LatestSIDLoaded + 1, TeensyROMMenu[DirNum].Name);
            }
         }
         else
         { // from SD or USB
            strcpy(LatestSIDLoaded + 1, DriveDirPath);
         }
         strcpy(LatestSIDLoaded + strlen(LatestSIDLoaded + 1) + 2, MenuSelCpy.Name);
         Printf_dbg("Saved SID: %d %s / %s\n", LatestSIDLoaded[0], LatestSIDLoaded+1, LatestSIDLoaded+strlen(LatestSIDLoaded+1)+2);
                  
         ParseSIDHeader(MenuSelCpy.Name); //Parse SID File & set up to transfer to C64 RAM
         break;
      case rtFileKla:
         XferImage = MenuSelCpy.Code_Image;
         XferSize = MenuSelCpy.Size;
         //Parse Koala File:
         if(!ParseKLAHeader()) return;
         StreamOffsetAddr = 0; //set to start of data
         IO1[rRegStrAvailable] = 0xff;    // transfer start flag, set last    
         break;
      case rtFileArt:
         XferImage = MenuSelCpy.Code_Image;
         XferSize = MenuSelCpy.Size;
         //Parse Hi-Res Art File:
         if(!ParseARTHeader()) return;
         StreamOffsetAddr = 0; //set to start of data
         IO1[rRegStrAvailable] = 0xff;    // transfer start flag, set last    
         break;
      case rtFilePETSCII:
      case rtFileTxt:
         XferImage = MenuSelCpy.Code_Image;
         XferSize = MenuSelCpy.Size;
         //convert from ASCII to PETSCII if needed:
         if (MenuSelCpy.ItemType==rtFileTxt) for(uint32_t Cnt=0; Cnt<XferSize; Cnt++) XferImage[Cnt]=ToPETSCII(XferImage[Cnt]);
         StreamOffsetAddr = 0; //set to start of data
         IO1[rRegStrAvailable] = 0xff;    // transfer start flag, set last    
         break;
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
         StreamOffsetAddr = 0; //set to start of data
         IO1[rRegStrAvailable] = 0xff;
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
      IOHandlerSelectInit();
   }

}

void MenuChange()
{
   strcpy(DriveDirPath, "/");
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
   }
   IO1[rwRegCursorItemOnPg] = 0;
}

bool LoadFile(FS *sourceFS, const char* FilePath, StructMenuItem* MyMenuItem) 
{
   char FullFilePath[MaxNamePathLength];

   //PathIsRoot() uses DriveDirPath directly
   if (strlen(FilePath) == 1 && FilePath[0] == '/') sprintf(FullFilePath, "%s%s", FilePath, MyMenuItem->Name);  // at root
   else sprintf(FullFilePath, "%s/%s", FilePath, MyMenuItem->Name);
      
   SendMsgPrintfln("Loading:\r\n%s", FullFilePath);

   File myFile = sourceFS->open(FullFilePath, FILE_READ);
   
   if (sourceFS != &SD) FullFilePath[0] = 0; //terminate if not SD
   
   if (!myFile) 
   {
      SendMsgPrintfln("File Not Found");
      return false;
   }
   
   MyMenuItem->Size = myFile.size();
   uint32_t count=0;
   
   if (MyMenuItem->ItemType == rtFileCrt)
   {  //load the CRT

      // No longer enforcing this, swap out if too large:
      //if(MyMenuItem->Size > MaxCRTKB*1024)
      //{
      //   SendMsgPrintfln("Not enough room"); 
      //   SendMsgPrintfln("  Size: %luk, Max CRT: %luk", MyMenuItem->Size/1024, MaxCRTKB); 
      //   myFile.close();
      //   return false;         
      //}

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
         if (!ParseChipHeader(lclBuf, FullFilePath)) //sends error messages
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
         SendMsgPrintfln("  Size: %luk, Max: %luk", MyMenuItem->Size/1024, RAM_ImageSize/1024); 
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

void InitDriveDirMenu() 
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
}

void SetDriveDirMenuNameType(uint16_t ItemNum, const char *filename)
{
   //malloc, copy file name and get item type from extension
   DriveDirMenu[ItemNum].Name = (char*)malloc(strlen(filename)+1);
   strcpy(DriveDirMenu[ItemNum].Name, filename);
   
   DriveDirMenu[ItemNum].ItemType = 
      Assoc_Ext_ItemType(DriveDirMenu[ItemNum].Name);
}

void LoadDirectory(FS *sourceFS) 
{
   InitDriveDirMenu();
   
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
      else //it's a file. copy name and get item type from extension
      {
         SetDriveDirMenuNameType(NumDrvDirMenuItems, filename);
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

void FreeDriveDirMenu()
{
   //free/clear prev loaded directory
   if(DriveDirMenu != NULL)
   {
      Printf_dbg("Dir info removed\n"); 
      for(uint16_t Num=0; Num < NumDrvDirMenuItems; Num++) free(DriveDirMenu[Num].Name);
      free(DriveDirMenu); DriveDirMenu = NULL;
   }
}

void FreeCrtChips()
{ //free chips allocated in RAM2 and reset NumCrtChips
   for(uint16_t cnt=0; cnt < NumCrtChips; cnt++) 
      if((uint32_t)CrtChips[cnt].ChipROM >= 0x20200000) free(CrtChips[cnt].ChipROM);
   NumCrtChips = 0;
}

uint8_t Assoc_Ext_ItemType(char * FileName)
{ //returns ItemType from enum regItemTypes

   uint32_t Length = strlen(FileName);
   
   if (Length < 4) return rtUnknown;
   
   char* Extension = strrchr( FileName, '.'); //find last dot
   // Must have dot, allow 2-4 char extension only, incl dot
   if (Extension == NULL || Extension > FileName + Length - 2 || Extension < FileName + Length -4) return rtUnknown; 
      
   Extension++; //skip dot
   //convert to lower case:
   for(char* cnt=Extension; cnt<=FileName + Length; cnt++) if(*cnt>='A' && *cnt<='Z') *cnt+=32;
   
   uint8_t Num = 0;
   
   while (Num < sizeof(Ext_ItemType_Assoc)/sizeof(Ext_ItemType_Assoc[0]))
   {
      if (strcmp(Extension, Ext_ItemType_Assoc[Num].Extension)==0) return Ext_ItemType_Assoc[Num].ItemType;
      Num++;
   }
   return rtUnknown;
}


