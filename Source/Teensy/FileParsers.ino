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


uint8_t NumCrtChips = 0;
StructCrtChip CrtChips[MAX_CRT_CHIPS];
char* StrSIDInfo;  // allocated to RAM2 via StrSIDInfoSize
char* LatestSIDLoaded; // allocated to RAM2 via MaxPathLength
char StrMachineInfo[16]; //~5 extra


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
   
bool ParseChipHeader(uint8_t* ChipHeader, const char *FullFilePath)   
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
         {  //we've used up all of ram1 & 2 now, too big for main image, have to boot minimal image    
            //SendMsgPrintfln("Too large for main image..."); 
            
            //check for minimal image present by seeing if there's an image (this one) at higher address
            // These two image checks provided by AndyA via the PJRC forum
            // https://forum.pjrc.com/index.php?threads/teensy-4-1-dual-boot-capability.74479/post-339451
            uint32_t imageStartAddress = FLASH_BASEADDRESS + UpperAddr; //point to main TR image
      
            // check For Valid Image: SPIFlashConfigMagicWord and VectorTableMagicWord
            if ((*((uint32_t*)imageStartAddress) != 0x42464346) || (*((uint32_t*)(imageStartAddress+0x1000)) != 0x432000D1))
            {
               SendMsgPrintfln("Dual boot image not found (NoMagNums)");
               return false;                        
            }
               
            // ivt starts 0x1000 after the start of flash. Address of start of code is 2nd vector in table.
            uint32_t firstInstructionPtr = imageStartAddress + 0x1000 + sizeof(uint32_t);
            uint32_t firstInstructionAddr = *(uint32_t*)firstInstructionPtr;
      
            // very basic sanity check, code should start after the ivt but not too far into the image.
            Printf_dbg("First instruction ptr: 0x%08X  addr: 0x%08X\n", firstInstructionPtr, firstInstructionAddr);
            if ( (firstInstructionAddr < (imageStartAddress+0x1000)) || (firstInstructionAddr > (imageStartAddress+0x3000)) ) 
            {
               // Address of first instruction isn't sensible for location in flash. Image was probably incorrectly built
               SendMsgPrintfln("Dual boot image not found (NoFirstInst)");
               return false;                        
            }
            
            if (FullFilePath[0] == 0)
            {
               SendMsgPrintfln("Must run this crt from SD"); 
               return false;                        
            }

            SendMsgPrintfln("Rebooting Teensy to Minimal"); 
            EEPwriteStr(eepAdCrtBootName, FullFilePath);
            EEPROM.write(eepAdMinBootInd, MinBootInd_ExecuteMin);
            REBOOT;
            
         }
         else
         {
            FreeDriveDirMenu(); //free/clear prev loaded directory to make space
         }
      }
      Printf_dbg("2");
   }
   Printf_dbg(" %08x\n", (uint32_t)CrtChips[NumCrtChips].ChipROM);
   return true;
}
 
FLASHMEM bool ParseARTHeader()
{
  // XferImage and XferSize are populated w/ koala file info
  
   if(XferImage[0] != 0 || XferImage[1] != 0x20) //allow only $2000
   {
      SendMsgPrintfln("Bad addr: $%02x%02x (exp $2000)", XferImage[1], XferImage[0]);
      return false;
   }

   if (XferSize != 9002 && XferSize != 9009) //exact expected image size
   {
      SendMsgPrintfln("Bad size: %lu bytes (exp 9002 or 9009)", XferSize);
      return false;
   }
   if (XferSize == 9002)
   {  //border/screen color unknown for this size
      XferImage[XferSize++] = 15; //PokeLtGrey
   }
   return true;
}

FLASHMEM bool ParseKLAHeader()
{
  // XferImage and XferSize are populated w/ koala file info
  
   if(XferImage[0] != 0 || (XferImage[1] & 0xbf) != 0x20) //allow only $2000 & $6000
   {
      SendMsgPrintfln("Bad addr: $%02x%02x (exp $2000 or $6000)", XferImage[1], XferImage[0]);
      return false;
   }

   XferImage[1] = 0x20;  //force to $2000

   if (XferSize != 10003) //exact expected image size
   {
      SendMsgPrintfln("Bad size: %lu bytes (exp 10003)", XferSize);
      return false;
   }

   return true;
}

FLASHMEM void SIDLoadError(const char* ErrMsg)
{
   strcat(StrSIDInfo, "Error: ");
   strcat(StrSIDInfo, ErrMsg); //add to displayed info
   SendU16(BadSIDToken);
   SendMsgPrintfln("Error:");
   SendMsgPrintfln(ErrMsg);
}

FLASHMEM void ParseSIDHeader(const char *filename)
{
   // XferImage and XferSize are populated w/ SID file info
   // Need to parse dataOffset (StreamOffsetAddr), loadAddress, 
   //    initAddress(rRegSIDInitLo/Hi) and playAddress (rRegSIDPlayLo/Hi)
   // Kick off x-fer (rRegStrAvailable) if successful
      
   //https://gist.github.com/cbmeeks/2b107f0a8d36fc461ebb056e94b2f4d6
   //https://www.lemon64.com/forum/viewtopic.php?t=71980&start=30
   //https://hvsc.c64.org/

   //** Construct first few lines of StrSIDInfo
   char RetSpc[] = "\r "; //return char + space
   strcpy(StrSIDInfo, RetSpc); //clear/init SID info

   strncat(StrSIDInfo, filename, 38); //filename, cut to 38 chars max to not scroll
   strcat(StrSIDInfo, RetSpc); 
   strcat(StrSIDInfo, RetSpc); //blank line to separate filename from header info
   
   strcat(StrSIDInfo, "Name: "); 
   strncat(StrSIDInfo, (char*)XferImage+0x16, 0x20); //Name (32 chars max)
   strcat(StrSIDInfo, RetSpc); 
   
   strcat(StrSIDInfo, "Auth: "); 
   strncat(StrSIDInfo, (char*)XferImage+0x36, 0x20);  //Author (32 chars max)
   strcat(StrSIDInfo, RetSpc); 
   
   strcat(StrSIDInfo, " Rel: "); 
   strncat(StrSIDInfo, (char*)XferImage+0x56, 0x20);  //Released (32 chars max)
   strcat(StrSIDInfo, RetSpc); 
   
   //** File basic syntax checks:
   if (memcmp(XferImage, "PSID", 4) != 0) 
   {
      if (memcmp(XferImage, "RSID", 4) != 0) 
      {
         SIDLoadError("PSID/RSID not found");
         return;
      }
   }
   
   uint16_t sidVersion = toU16(XferImage+0x04);
   if ( sidVersion<2 || sidVersion>4) 
   {
      SIDLoadError("Unexpected Version");
      return;
   }

   StreamOffsetAddr = toU16(XferImage+0x06); //dataOffset
   if (StreamOffsetAddr!= 0x7c) 
   {
      SIDLoadError("Unexpected Data Offset");
      return;
   }
   
   if (toU16(XferImage+0x08) != 0)
   {
      Printf_dbg("\nNon-standard load addr");     
      //make standard by adding the addr in front of the data:
      StreamOffsetAddr -=2;
      XferImage[StreamOffsetAddr] = XferImage[0x09];
      XferImage[StreamOffsetAddr+1] = XferImage[0x08];
   }
   
   //** Extract needed information:
   uint16_t LoadAddress = (XferImage[StreamOffsetAddr + 1] << 8) 
      | XferImage[StreamOffsetAddr]; //little endian, opposite of toU16
      
   uint16_t PlayAddress = toU16(XferImage+0x0C);
   uint16_t InitAddress = toU16(XferImage+0x0A);
   SendMsgPrintfln("SID Loc %04x:%04x, Play=%04x", LoadAddress, LoadAddress+XferSize, PlayAddress);

   if (InitAddress == 0)
   { //"init address is 0, means the address is equal to the effective load address"
     // (Doesn't work)
      //XferImage[0x0A] = XferImage[StreamOffsetAddr+1]; //rRegSIDInitHi
      //XferImage[0x0B] = XferImage[StreamOffsetAddr];   //rRegSIDInitLo
      //InitAddress = toU16(XferImage+0x0A);
      //Printf_dbg("\n0000 init updated", IO1[rRegSIDInitHi], IO1[rRegSIDInitLo]);
      SIDLoadError("Init addr is 0");
      return;
   }
   
   Printf_dbg("\nSongs: %d", toU16(XferImage+0x0E));
   Printf_dbg("\nStart Song: %d", toU16(XferImage+0x10));
   Printf_dbg("\nInit: %04x", InitAddress);
   Printf_dbg("\nPlay: %04x", PlayAddress);
   Printf_dbg("\nTR Code: %02x00:%02xff", IO1[rwRegCodeStartPage], IO1[rwRegCodeLastPage]);

   //** check for conflict with IO1 space:   
   if (LoadAddress < 0xdf00 && LoadAddress+XferSize >= 0xde00)
   {
      SIDLoadError("IO1 mem conflict");
      return;
   }

   //** check for RAM conflict with TR code:   
   if (LoadAddress < (IO1[rwRegCodeLastPage]+1)*256 && LoadAddress+XferSize >= IO1[rwRegCodeStartPage]*256)
   {
      SIDLoadError("Mem conflict w/ TR app");
      return;
   }

   //speed: for each song (bit): 0 specifies vertical blank interrupt (50Hz PAL, 60Hz NTSC)
   //                            1 specifies CIA 1 timer interrupt (default 60Hz)
   Printf_dbg("\nSpeed reg: %08x", toU32(XferImage+0x12));

   //flags:  Bits 2-3 specify the video standard (clock):
   const char *VStandard[] =
   {
      "Unknown",  //    00 = Unknown, use PAL
      "PAL",      //    01 = PAL,
      "NTSC",     //    10 = NTSC,
      "Either",   //    11 = PAL and NTSC, use NTSC
   };
   
   const uint8_t CIATimer[4][2] =
   {   //rRegSIDDefSpeedLo/Hi = SONGSPEED/1022730 seconds for NTSC, higher=slower playback (timer)
       //verified with o-scope on IRQ line using a Kawari machine 12/24/23
      0x4C, 0xC7,   // PAL  SID on  PAL machine 50.13Hz IRQ rate
      0x4F, 0xB2,   // PAL  SID on NTSC machine 50.13Hz IRQ rate
      0x40, 0x58,   // NTSC SID on  PAL machine 59.81Hz IRQ rate
      0x42, 0xC6,   // NTSC SID on NTSC machine 59.81Hz IRQ rate
   };
   
   //set playback speed based on SID and Machine type
   uint16_t SidFlags = toU16(XferImage+0x76); //WORD flags
   Printf_dbg("\nSidFlags: %04x", SidFlags);
   SidFlags = (SidFlags >> 2) & 3;  //now just PAL/NTSC
   SendMsgPrintfln("SID Clock: %s", VStandard[SidFlags]);
   
   char TechBuf[40];
   strcat(StrSIDInfo, "Tech: "); //1+6
   sprintf(TechBuf, "%04x:%04x i=%04x p=%04x %s", LoadAddress, LoadAddress+(uint16_t)XferSize, InitAddress, PlayAddress, VStandard[SidFlags]);
   strcat(StrSIDInfo, TechBuf); //24 + 7 max ("Unknown")
  

   //bit 0: 1=NTSC, 0=PAL;    bit 1: 1=60Hz, 0=50Hz
   char MainsFreq[2] = {(IO1[wRegVid_TOD_Clks] & 2)==2 ? '6' : '5' , 0};
   Printf_dbg("\nMachine Clocks: %s Vid, %s0Hz TOD", 
      VStandard[(IO1[wRegVid_TOD_Clks] & 1)+1], MainsFreq);
      
   //** Finish StrSIDInfo
   //"NTSC vid, 6"
   strcpy(StrMachineInfo, VStandard[(IO1[wRegVid_TOD_Clks] & 1)+1]); 
   strcat(StrMachineInfo, " Vid, "); 
   strcat(StrMachineInfo, MainsFreq); 

   SidFlags = (IO1[wRegVid_TOD_Clks] & 1) | (SidFlags & 2); //now selects from CIATimer
   Printf_dbg("\nCIA Timer: %02x%02x", CIATimer[SidFlags][0], CIATimer[SidFlags][1]);

   Printf_dbg("\nrelocStartPage: %02x", XferImage[0x78]);
   Printf_dbg("\nrelocPages: %02x", XferImage[0x79]);

   IO1[rRegSIDDefSpeedHi] = CIATimer[SidFlags][0];
   IO1[rRegSIDDefSpeedLo] = CIATimer[SidFlags][1];  
   IO1[rRegSIDInitHi] = XferImage[0x0A];
   IO1[rRegSIDInitLo] = XferImage[0x0B];
   IO1[rRegSIDPlayHi] = XferImage[0x0C]; //Play address of 0 handled in assy code
   IO1[rRegSIDPlayLo] = XferImage[0x0D];
   IO1[rRegSIDNumSongsZ] = abs(toU16(XferImage+0x0E)-1); //Make Zero based, range is 1-256
   IO1[rwRegSIDSongNumZ] = abs(toU16(XferImage+0x10)-1); //Make Zero based, range is 1-256
   
   SendU16(GoodSIDToken);
   IO1[rRegStrAvailable] = 0xff; //transfer start flag, set last. Indicates error if not set
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
   Printf_dbg("%s<--", SerialStringBuf);
   Serial.flush();
   IO1[rwRegStatus] = rsC64Message; //tell C64 there's a message
   uint32_t beginWait = millis();
   //wait up to 3 sec for C64 to read message:
   while (millis()-beginWait<3000) if(IO1[rwRegStatus] == rsContinue) return;
   Serial.printf("\nTimeout!\n");
}
