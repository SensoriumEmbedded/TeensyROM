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
extern uint8_t *ptrRAM_ImageEnd;    // points to end of CRT-used RAM1 buffer

FLASHMEM void FreeRAMBanks(uint8_t *Bank[], uint8_t *NumBanks)
{
   // the handler may still be installed and the C64 running: zero the count first, so the
   //  IO register handlers return before touching the banks once the first one is freed
   uint8_t Count = *NumBanks;
   *NumBanks = 0;
   asm volatile("" ::: "memory");
   for(uint16_t cnt=0; cnt < Count; cnt++)
      if((uint32_t)Bank[cnt] >= 0x20200000) free(Bank[cnt]);
}

// Returns the number of banks allocated: a power of 2 up to MaxBanks, or 0 if fewer than MinBanks fit.
FLASHMEM uint8_t AllocRAMBanks(uint8_t *Bank[], uint8_t MaxBanks, uint8_t MinBanks)
{
   uint8_t NumBanks = 0;
   FreeDriveDirMenu();

   if (NumCrtChips == 0) ptrRAM_ImageEnd = RAM_Image; //If no CRT loaded, RAM1 is entirely free
   uint8_t *pRAM_Image = ptrRAM_ImageEnd;  //start at end of any CRT-used RAM1

   while (((uint32_t)pRAM_Image - (uint32_t)RAM_Image + RAM_Bank_Size <= RAM_ImageSize)
           && (NumBanks < MaxBanks))
   {
      Bank[NumBanks] = pRAM_Image;
      pRAM_Image += RAM_Bank_Size;
      NumBanks++;
   }

   while(NumBanks < MaxBanks && (NULL != (Bank[NumBanks] = (uint8_t*)malloc(RAM_Bank_Size))))
      NumBanks++;

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
   *ptrFileName = Path;
   RegMenuTypes MenuSourceID = RegMenuTypeFromFileName(ptrFileName);

   if (MenuSourceID == rmtSD) SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected
   if (MenuSourceID == rmtUSBDrive) USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up
   //rmtTeensy not allowed, no RAM image files in Teensy Mem

   return FSfromSourceID(MenuSourceID);
}

FLASHMEM static void ReadRAMBanksFile(File &LoadFile, uint8_t *Bank[], uint8_t NumBanks)
{
   if (LoadFile.size() > (uint32_t)NumBanks * RAM_Bank_Size)
      Serial.printf("Too Large! Loading first %luK\n", (uint32_t)NumBanks * RAM_Bank_Size / 1024);

   uint32_t CharNum = 0;
   for (uint8_t BankNum = 0; BankNum < NumBanks && LoadFile.available(); BankNum++)
   {
      int BytesRead = LoadFile.read(Bank[BankNum], RAM_Bank_Size);
      if (BytesRead <= 0) break;
      CharNum += BytesRead;
   }
   Serial.printf("Read %lu Bytes\n", CharNum);
   LoadFile.close();
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
   ReadRAMBanksFile(LoadFile, Bank, NumBanks);
}

FLASHMEM static bool WriteRAMBanksFile(FS *sourceFS, const char *FileName, uint8_t *Bank[], uint8_t NumBanks)
{
   Serial.printf("Saving: %s\n", FileName);
   sourceFS->remove(FileName);   // FILE_WRITE appends to an existing file
   File SaveFile = sourceFS->open(FileName, FILE_WRITE);
   if (!SaveFile)
   {
      Serial.println("Unable to open!");
      return false;
   }

   for (uint8_t BankNum = 0; BankNum < NumBanks; BankNum++)
   {
      if (SaveFile.write(Bank[BankNum], RAM_Bank_Size) != RAM_Bank_Size)
      {
         Serial.printf("Write failed after %dKBytes!\n", BankNum * RAM_Bank_Size / 1024);
         SaveFile.close();
         return false;
      }
   }
   Serial.printf("Saved %dKBytes\n", NumBanks * RAM_Bank_Size / 1024);
   SaveFile.close();
   return true;
}

FLASHMEM static void FlashSavedLED()
{
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

// Saves to the first free numbered name: reu.reu becomes reu000.reu, reu000.reu becomes reu001.reu, etc
FLASHMEM void SaveRAMBanks(uint8_t *Bank[], uint8_t NumBanks, const char *SourcePath)
{
   if (NumBanks == 0) return;   // nothing to save: probably due to failure to allocate

   char Filename[MaxPathLength];
   strlcpy(Filename, SourcePath, MaxPathLength);
   char *ptrFileName;
   FS *sourceFS = RAMBanksFS(Filename, &ptrFileName);

   char Extension[20];
   char *pDot = strrchr(ptrFileName, '.');
   if (pDot == NULL) pDot = ptrFileName + strlen(ptrFileName); //if no extension, set to end of filename
   strlcpy(Extension, pDot, sizeof(Extension));
   *pDot = 0;

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

   char NewFilename[MaxPathLength];

   do sprintf(NewFilename, "%s%03d%s", ptrFileName, NewNum++, Extension);
   while (sourceFS->exists(NewFilename));

   if (WriteRAMBanksFile(sourceFS, NewFilename, Bank, NumBanks)) FlashSavedLED();
}

FLASHMEM static void WithExtension(char *Dest, const char *FileName, const char *Extension)
{
   strlcpy(Dest, FileName, MaxPathLength);
   char *pDot = strrchr(Dest, '.');
   if (pDot != NULL) *pDot = 0;
   strlcat(Dest, Extension, MaxPathLength);
}

static const char TempExtension[] = ".tmp", BackupExtension[] = ".bak";

// Overwrites the file, keeping the one it replaces as .bak: georam.ram becomes georam.bak
FLASHMEM void SaveRAMBanksReplacing(uint8_t *Bank[], uint8_t NumBanks, const char *SourcePath)
{
   if (NumBanks == 0) return;   // nothing to save: probably due to failure to allocate

   char Filename[MaxPathLength];
   strlcpy(Filename, SourcePath, MaxPathLength);
   char *ptrFileName;
   FS *sourceFS = RAMBanksFS(Filename, &ptrFileName);

   char TempFilename[MaxPathLength], BackupFilename[MaxPathLength];
   WithExtension(TempFilename, ptrFileName, TempExtension);
   WithExtension(BackupFilename, ptrFileName, BackupExtension);

   if (!WriteRAMBanksFile(sourceFS, TempFilename, Bank, NumBanks)) return;
   bool HadSave = sourceFS->exists(ptrFileName);
   if (HadSave)
   {  // rename refuses an existing target
      sourceFS->remove(BackupFilename);
      sourceFS->rename(ptrFileName, BackupFilename);
   }
   if (!sourceFS->rename(TempFilename, ptrFileName))
   {
      Serial.printf("Unable to rename %s to %s!\n", TempFilename, ptrFileName);
      if (HadSave) sourceFS->rename(BackupFilename, ptrFileName);
      return;
   }
   FlashSavedLED();
}

// Loads a file saved by SaveRAMBanksReplacing. Power lost between its two renames leaves no file,
//  only a complete .tmp (the newer save) and the .bak; a .tmp of any other size is a torn write.
FLASHMEM void LoadRAMBanksReplaced(uint8_t *Bank[], uint8_t NumBanks, const char *SourcePath)
{
   char Filename[MaxPathLength];
   strlcpy(Filename, SourcePath, MaxPathLength);
   Serial.printf("Loading: %s\n", Filename);
   char *ptrFileName;
   FS *sourceFS = RAMBanksFS(Filename, &ptrFileName);
   File LoadFile = sourceFS->open(ptrFileName, FILE_READ);
   if (!LoadFile)
   {
      char Fallback[MaxPathLength];
      WithExtension(Fallback, ptrFileName, TempExtension);
      LoadFile = sourceFS->open(Fallback, FILE_READ);
      if (LoadFile && LoadFile.size() != (uint32_t)NumBanks * RAM_Bank_Size)
      {
         Serial.printf("Skipping %s: %lu Bytes\n", Fallback, (uint32_t)LoadFile.size());
         LoadFile.close();
      }
      if (!LoadFile)
      {
         WithExtension(Fallback, ptrFileName, BackupExtension);
         LoadFile = sourceFS->open(Fallback, FILE_READ);
      }
      if (!LoadFile)
      {
         Serial.println("Not found!");
         return;
      }
      Serial.printf("Not found, loading: %s\n", Fallback);
   }
   ReadRAMBanksFile(LoadFile, Bank, NumBanks);
}
