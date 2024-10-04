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

void HandleExecution()
{
   StructMenuItem MenuSelCpy = DriveDirMenu; //local copy selected menu item to modify
   
   if(!LoadFile(&MenuSelCpy, &SD)) return;     

   MenuSelCpy.Code_Image = RAM_Image;

   //has to be distilled down to one of these by this point, only ones supported so far.
   //Emulate ROM or prep for tranfer
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
         //NVIC_DISABLE_IRQ(IRQ_ENET); //disable ethernet interrupt when emulating VIC cycles
         //NVIC_DISABLE_IRQ(IRQ_PIT);
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
      //called after cart loaded

      BigBufCount = 0;
      
      if (RegNextIOHndlr>=IOH_Num_Handlers)
      {
         Serial.println("***IOHandler out of range");
         return;
      }
      
      Serial.printf("Loading IO handler: %s\n", IOHandler[RegNextIOHndlr]->Name);
      
      if (IOHandler[RegNextIOHndlr]->InitHndlr != NULL) IOHandler[RegNextIOHndlr]->InitHndlr();
      
      Serial.flush();
      CurrentIOHandler = RegNextIOHndlr;
      doReset=true;
   }

}

bool LoadFile(StructMenuItem* MyMenuItem, FS *sourceFS) 
{
   char FullFilePath[MaxNamePathLength];

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
   
   //chips in main buffer, then malloc in RAM2.
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
      if(NULL == (CrtChips[NumCrtChips].ChipROM = (uint8_t*)malloc(CrtChips[NumCrtChips].ROMSize)))
      {
         SendMsgPrintfln("Not enough room: %d", NumCrtChips); 
         return false;         
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

//Big endian byte to int conversions:
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
         RegNextIOHndlr = HWID_IOH_Assoc[Num].IOH;
         return true;
      }
      Num++;
   }
   return false;
}


void SendMsgPrintfln(const char *Fmt, ...)
{
   char SerialStringBuf[MaxPathLength]; 
   
   va_list ap;
   va_start(ap,Fmt);
   vsprintf(SerialStringBuf, Fmt, ap); 
   va_end(ap);
    
   Serial.printf("%s\n", SerialStringBuf);
   Serial.flush();
}

