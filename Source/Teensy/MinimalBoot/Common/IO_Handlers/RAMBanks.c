// MIT License
//
// Copyright (c) 2026 Travis Smith
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


// RAM expansion memory (REU, GeoRAM): 8k banks taken from the free end of RAM1's RAM_Image,
//  then from RAM2, preloaded from and saved to an image file.

#define RAM_Bank_Size   0x2000

extern void EEPreadStr(uint16_t addr, char* buf);
extern RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName);
extern bool SDFullInit();
extern bool USBFileSystemWait();
extern FS *FSfromSourceID(RegMenuTypes SourceID);
extern void FreeDriveDirMenu();
extern uint8_t RAM_Image[];
extern uint8_t NumCrtChips;
extern uint8_t *ptrRAM_ImageEnd;    // points to end of CRT-used RAM1 buffer

FLASHMEM void FreeRAMBanks(uint8_t *Bank[], uint8_t *NumBanks)
{ //free banks allocated in RAM2
   for(uint16_t cnt=0; cnt < *NumBanks; cnt++)
      if((uint32_t)Bank[cnt] >= 0x20200000) free(Bank[cnt]);
   *NumBanks = 0;
}

// Returns the number of banks allocated: a power of 2 up to MaxBanks, or 0 if fewer than MinBanks fit.
FLASHMEM uint8_t AllocRAMBanks(uint8_t *Bank[], uint8_t MaxBanks, uint8_t MinBanks)
{
   uint8_t NumBanks = 0;
   FreeDriveDirMenu(); //free/clear prev loaded directory to make space. Doing it regardless to preserve continuity

   if (NumCrtChips == 0) ptrRAM_ImageEnd = RAM_Image; //If no CRT loaded, RAM1 is entirely free
   uint8_t *pRAM_Image = ptrRAM_ImageEnd;  //start at end of any CRT-used RAM1

   //First use RAM_Image from RAM1:
   while (((uint32_t)pRAM_Image - (uint32_t)RAM_Image + RAM_Bank_Size <= RAM_ImageSize)
           && (NumBanks < MaxBanks))
   {
      Bank[NumBanks] = pRAM_Image;
      pRAM_Image += RAM_Bank_Size;
      NumBanks++;
   }

   //allocate the rest from RAM2
   while(NumBanks < MaxBanks && (NULL != (Bank[NumBanks] = (uint8_t*)malloc(RAM_Bank_Size))))
      NumBanks++;

   //resize to a power of 2 if needed and free unused banks
   if (NumBanks & (NumBanks-1))  //not a power of 2
   {
      // largest power less than or equal to NumBanks
      uint8_t NewNumBanks =  1 << (7 - (__builtin_clz(NumBanks) - 24));
      Serial.printf("Resizing from %d banks to %d banks\n", NumBanks, NewNumBanks);

      for (uint8_t unused_bank = NewNumBanks; unused_bank < NumBanks; unused_bank++)
         if((uint32_t)Bank[unused_bank] >= 0x20200000)
            free(Bank[unused_bank]);
      NumBanks = NewNumBanks;
   }

   if (NumBanks < MinBanks)
   {
      Serial.printf("Unable to allocate minimum size of %dK\n", MinBanks * RAM_Bank_Size / 1024);
      FreeRAMBanks(Bank, &NumBanks);
   }
   return NumBanks;
}

// Path is source-prefixed ("SD:/reu.reu")
FLASHMEM static FS *RAMBanksFS(char *Path, char **ptrFileName)
{
   *ptrFileName = Path; //pointer to move past SD/USB/TR:
   RegMenuTypes MenuSourceID = RegMenuTypeFromFileName(ptrFileName);

   if (MenuSourceID == rmtSD) SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected
   if (MenuSourceID == rmtUSBDrive) USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up
   //rmtTeensy not allowed, no RAM image files in Teensy Mem

   return FSfromSourceID(MenuSourceID);
}

FLASHMEM void LoadRAMBanks(uint8_t *Bank[], uint8_t NumBanks, const char *SourcePath)
{
   char Filename[MaxPathLength];
   strlcpy(Filename, SourcePath, MaxPathLength);
   Serial.printf("Loading: %s\n", Filename);
   char *ptrFileName;
   FS *sourceFS = RAMBanksFS(Filename, &ptrFileName);
   File LoadFile = sourceFS->open(ptrFileName, FILE_READ);
   if (!LoadFile)
   {
      Serial.println("Not found!");
      return;
   }

   if (LoadFile.size() > (uint32_t)NumBanks * RAM_Bank_Size)
      Serial.printf("Too Large! Loading first %luK\n", (uint32_t)NumBanks * RAM_Bank_Size / 1024);

   uint32_t CharNum = 0;
   for (uint8_t BankNum = 0; BankNum < NumBanks && LoadFile.available(); BankNum++)
      CharNum += LoadFile.read(Bank[BankNum], RAM_Bank_Size);
   Serial.printf("Read %lu Bytes\n", CharNum);
   LoadFile.close();
}

// Saves to the first free numbered name: reu.reu becomes reu000.reu, reu000.reu becomes reu001.reu, etc
FLASHMEM void SaveRAMBanks(uint8_t *Bank[], uint8_t NumBanks, const char *SourcePath)
{
   if (NumBanks == 0) return;   // nothing to save: probably due to failure to allocate

   char Filename[MaxPathLength];
   strlcpy(Filename, SourcePath, MaxPathLength);
   char *ptrFileName;
   FS *sourceFS = RAMBanksFS(Filename, &ptrFileName);

   char Extension[20];
   char *pDot = strrchr(ptrFileName, '.'); //find last dot
   if (pDot == NULL) pDot = ptrFileName + strlen(ptrFileName); //if no extension, set to end of filename
   strlcpy(Extension, pDot, sizeof(Extension)); //copy extension
   *pDot = 0; //terminate at dot/extension

   bool IsNum = true; //are last 3digits of filename all numbers?
   if (strlen(ptrFileName)>=3)
   {
      for(char *charloc=pDot-3; charloc<pDot; charloc++)
         if (*charloc<'0' || *charloc>'9') IsNum = false;
   }
   else IsNum = false;

   uint16_t NewNum = 0;
   if (IsNum)
   {  //find current number and then terminate it
      NewNum = atoi(pDot-3);
      *(pDot-3) = 0; //terminate number
   }

   //now see if the file exists, or itterate until it doesn't
   char NewFilename[MaxPathLength];

   do sprintf(NewFilename, "%s%03d%s", ptrFileName, NewNum++, Extension);
   while (sourceFS->exists(NewFilename));

   Serial.printf("Saving: %s\n", NewFilename);
   File SaveFile = sourceFS->open(NewFilename, FILE_WRITE);
   if (!SaveFile)
   {
      Serial.println("Unable to open!");
      return;
   }

   for (uint8_t BankNum = 0; BankNum < NumBanks; BankNum++)
      SaveFile.write(Bank[BankNum], RAM_Bank_Size);
   Serial.printf("Saved %dKBytes\n", NumBanks * RAM_Bank_Size / 1024);
   SaveFile.close();

   //Flash LED to ack save
   uint32_t LastmS = millis();
   bool LEDState = true;

   for (uint32_t FlashCount=0; FlashCount<20; FlashCount++)
   {
      while(millis()-LastmS < 75);
      LastmS = millis();
      if ((LEDState = !LEDState)) SetLEDOn;
      else SetLEDOff;
   }
}
