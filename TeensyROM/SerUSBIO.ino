
void ServiceSerial()
{
   uint8_t inByte = Serial.read();
   if (inByte == 0x64) //command from app
   {
      inByte = Serial.read();
      switch (inByte)
      {
         case 0x55:
            Serial.println("TeensyROM Ready!");
            break;
         case 0xAA:
            ReceiveFile();        
            break;
         case 0xEE:
            Serial.println("Reset cmd received");
            SetUpMainMenuROM();
            doReset = true;
            break;
         //case 0x67:
         //   SDFileDir = SD.open("/");
         //   if(SDFileDir) LoadSDDirectory(SDFileDir);  
         //   else Serial.println("SD Card not present/initialized");
         //   break;
         default:
            Serial.printf("Unk: %02x\n", inByte); 
            break;
      }
   }
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
   
   uint16_t bytenum = 0;
   while(bytenum < len)
   {
      if(!SerialAvailabeTimeout()) return;
      RAM_Image[bytenum] = Serial.read();
      CheckSum-=RAM_Image[bytenum++];

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
   if (strcmp((USBHostMenu.Name + strlen(USBHostMenu.Name) - 4), ".prg")==0)
   {
      USBHostMenu.ItemType = rtPrg;
      Serial.println(".PRG file detected");
      USBHostMenu.Code_Image=RAM_Image;
      NumUSBHostItems = 1;
      return;
   }
   
   if (strcmp((USBHostMenu.Name + strlen(USBHostMenu.Name) - 4), ".crt")!=0)
   {
      //NumUSBHostItems = 0, set at start
      Serial.println("File type unknown!");
      return;
   }
   
   Serial.println(".CRT file detected"); 
   ParseCRTFile(&USBHostMenu);   
 
}

void ParseCRTFile(StructMenuItem* MyMenuItem)   
{
   //https://vice-emu.sourceforge.io/vice_17.html#SEC369
   //http://ist.uwaterloo.ca/~schepers/formats/CRT.TXT
   
   if (memcmp(RAM_Image, "C64 CARTRIDGE", 13)!=0)
   {
      Serial.println("\"C64 CARTRIDGE\" not found");
      return;
   }
   
   uint32_t HeaderLen = toU32(RAM_Image+0x10);
   Serial.printf("Header len: %lu\n", HeaderLen);
   if (HeaderLen < 0x40) HeaderLen = 0x40;
   
   Serial.printf("HW Type: %d\n", toU16(RAM_Image+0x16));
   if (toU16(RAM_Image+0x16) !=0)
   {
      Serial.println("Only \"Normal\" carts *currently* supported");
      return;
   }
   
   uint8_t EXROM = RAM_Image[0x18];
   uint8_t GAME = RAM_Image[0x19];
   Serial.printf("EXROM: %d\n", EXROM);
   Serial.printf(" GAME: %d\n", GAME);
   
   Serial.printf("Name: %s\n", (RAM_Image+0x20));
   
   uint8_t *ChipImage = RAM_Image+HeaderLen;
   //On to CHIP packet(s)...
   if (memcmp(ChipImage, "CHIP", 4)!=0)
   {
      Serial.println("\"CHIP\" not found");
      return;
   }
  
   Serial.printf("Packet len: $%08x\n",  toU32(ChipImage+0x04));
   Serial.printf("Chip Type: %d\n",      toU16(ChipImage+0x08));
   Serial.printf(" Bank Num: %d\n",      toU16(ChipImage+0x0A));
   Serial.printf("Load Addr: $%04x\n",   toU16(ChipImage+0x0C));
   Serial.printf(" ROM Size: $%04x\n",   toU16(ChipImage+0x0E));
   
   //We have a good CRT image, new defaults:
   MyMenuItem->ItemType = rtUnk;
   NumUSBHostItems = 1;
   
   MyMenuItem->Code_Image=RAM_Image+HeaderLen+0x10;
   
   if(EXROM==0 && GAME==1 && toU16(ChipImage+0x0C) == 0x8000 && toU16(ChipImage+0x0E) == 0x2000)
   {
      MyMenuItem->ItemType = rt8kLo;
      Serial.println("\n 8kLo config");
      return;
   }      

   if(EXROM==1 && GAME==0 && toU16(ChipImage+0x0C) == 0xe000 && toU16(ChipImage+0x0E) == 0x2000)
   {
      MyMenuItem->ItemType = rt8kHi;
      Serial.println("\n 8kHi config");
      return;
   }      

   if(EXROM==0 && GAME==0 && toU16(ChipImage+0x0C) == 0x8000 && toU16(ChipImage+0x0E) == 0x4000)
   {
      MyMenuItem->ItemType = rt16k;
      Serial.println("\n 16k config");
      return;
   }      
   
   Serial.println("\nHW config unknown!");
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
   
   Serial.write(0x9B);  
   Serial.write(0x7F);  
   Serial.print("Timeout!\n");  
   return(false);
}

void LoadSDDirectory() 
{
   NumSDItems = 0;
   File dir = SD.open(SDPath);
   if(!dir) return;   
   
   if (!(strlen(SDPath) == 1 && SDPath[0] == '/'))
   {  //not at root, add up dir option
      NumSDItems = 1;
      strcpy(SDMenu[0].Name, UpDirString);
      SDMenu[0].ItemType = rtDir;
   }
   
   do
   {
      File entry = dir.openNextFile();
      if (! entry) return;
      if (entry.isDirectory())
      {
         SDMenu[NumSDItems].Name[0] = '/';
         memcpy(SDMenu[NumSDItems].Name+1, entry.name(), MaxItemNameLength-1);
      }
      else memcpy(SDMenu[NumSDItems].Name, entry.name(), MaxItemNameLength);
      
      SDMenu[NumSDItems].Name[MaxItemNameLength-1]=0; //terminate in case too long. 
      
      if (entry.isDirectory()) SDMenu[NumSDItems].ItemType = rtDir;
      else if (strcmp((SDMenu[NumSDItems].Name + strlen(SDMenu[NumSDItems].Name) - 4), ".prg")==0) SDMenu[NumSDItems].ItemType = rtPrg;
      else if (strcmp((SDMenu[NumSDItems].Name + strlen(SDMenu[NumSDItems].Name) - 4), ".crt")==0) SDMenu[NumSDItems].ItemType = rtCrt;
      else SDMenu[NumSDItems].ItemType = rtUnk;
      
      //Serial.printf("%d- %s\n", NumSDItems, SDMenu[NumSDItems].Name); 
      entry.close();
   } while(NumSDItems++ < MaxMenuItems);
   
   Serial.print("Too many files!");
}

bool LoadSDFile()
{
   char FullFilePath[300];

   if (strlen(SDPath) == 1 && SDPath[0] == '/') sprintf(FullFilePath, "%s%s", SDPath, SDMenu[RegSelect].Name);  // at root
   else sprintf(FullFilePath, "%s/%s", SDPath, SDMenu[RegSelect].Name);
      
   Serial.printf("Openning: %s\n", FullFilePath);
   
   File myFile = SD.open(FullFilePath, FILE_READ);
   if (!myFile) return false;

   uint16_t count=0;
   while (myFile.available()) RAM_Image[count++]=myFile.read();
   
   SDMenu[RegSelect].Code_Image = RAM_Image;
   SDMenu[RegSelect].Size = count;
   myFile.close();
   return true;
}

void HandleExecution()
{
   //DisablePhi2ISR = true;
   //interpret/load rtDir, rtCrt, and rtPrg from SD card
   if (CurrentMenu == rmtSD) switch(SDMenu[RegSelect].ItemType)
   {
      case rtDir: //load the new directory from SD to SDMenu/NumSDItems
         if(strcmp(SDMenu[RegSelect].Name, UpDirString)==0)
         {  //up dir
            char * LastSlash = strrchr(SDPath, '/'); //find last slash
            if (LastSlash != NULL) LastSlash[0] = 0;  //terminate it there 
         }
         else strcat(SDPath, SDMenu[RegSelect].Name); //append dir name
         LoadSDDirectory(); 
         NumMenuItems = NumSDItems;
         return;
         break;
      case rtPrg: //Load the prg file to RAM
         if(!LoadSDFile()) SDMenu[RegSelect].ItemType=rtUnk; //mark unknown if error
         break;
      case rtCrt: //load the Crt in to RAM and parse it for emulation
         if(!LoadSDFile()) SDMenu[RegSelect].ItemType=rtUnk; //mark unknown if error
         ParseCRTFile(&SDMenu[RegSelect]); //will convert type, if checks ok
         break;

   }      
   
   //has to be distilled down to one of these by this point, only ones supported so far.
   //Execute ROM or prep PRG tranfer
   switch(MenuSource[RegSelect].ItemType)
   {
      case rt16k:
         SetGameAssert;
         SetExROMAssert;
         LOROM_Image = MenuSource[RegSelect].Code_Image;
         HIROM_Image = MenuSource[RegSelect].Code_Image+0x2000;
         doReset=true;
         break;
      case rt8kHi:
         SetGameAssert;
         SetExROMDeassert;
         LOROM_Image = NULL;
         HIROM_Image = MenuSource[RegSelect].Code_Image;
         doReset=true;
         break;
      case rt8kLo:
         SetGameDeassert;
         SetExROMAssert;
         LOROM_Image = MenuSource[RegSelect].Code_Image;
         HIROM_Image = NULL;
         doReset=true;
         break;
      case rtPrg:
         //set up for transfer
         StreamStartAddr = (MenuSource[RegSelect].Code_Image[1]<<8) + MenuSource[RegSelect].Code_Image[0];
         StreamOffsetAddr = 2; //set to start of data
         break;
   }
    
}



        