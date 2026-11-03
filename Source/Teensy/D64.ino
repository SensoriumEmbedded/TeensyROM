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

#include "DiskImage/MenuDisk.h"

enum enDxxFileNameBytes
{
   DxxFNB_NameLength = 16,
   DxxFNB_NameTerm =   16,
   DxxFNB_Track =      17,
   DxxFNB_Sector =     18,
   DxxFNB_DiskType =   19,
   DxxFNB_Bytes =      20,
};

enum enDxxFileTypes
{
   DxxFT_DEL = 0, 
   DxxFT_SEQ = 1, 
   DxxFT_PRG = 2, 
   DxxFT_USR = 3, 
   DxxFT_REL = 4, 
};

// An image file open for the shared disk image engine (DiskImage/MenuDisk.h)
struct DxxImage
{
   File *Image;
   d64_t Disk;
};

FLASHMEM static int DxxSectorRead(void *Context, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   struct DxxImage *Dxx = (struct DxxImage *)Context;
   return Dxx->Image->seek(d64_sector_offset(&Dxx->Disk, Track, Sector)) && Dxx->Image->read(Block, D64_SECTOR) == D64_SECTOR ? 0 : -1;
}

FLASHMEM static bool DxxOpen(struct DxxImage *Dxx, File *Image, uint8_t DiskType)
{
   enum MenuDiskKind Kind = DiskType == rtD71 ? MenuDiskD71 : DiskType == rtD81 ? MenuDiskD81 : MenuDiskD64;

   Dxx->Image = Image;
   return MenuDiskOpen(&Dxx->Disk, Kind, Image->size(), DxxSectorRead, Dxx);
}

FLASHMEM uint8_t ItemTypeFromDxxFileType(uint8_t FileType)
{
   uint8_t RetItemType = rtUnknown; //default to unknown
   
   switch(FileType & 0x0f) //check lower 4 bits
   {
      case DxxFT_PRG:
         RetItemType = rtFilePrg;
         break;
      case DxxFT_SEQ:
         RetItemType = rtFilePETSCII;
         break;
      //Display DEL, USR, and REL as text files?
   }
   return RetItemType;
}

FLASHMEM void LoadDxxDirectory(FS *sourceFS, uint8_t DiskType) 
{
   //Interpreted from: https://ist.uwaterloo.ca/~schepers/formats/D64.TXT  D71.TXT  D81.TXT  DISK.TXT
   
   uint32_t beginWait = millis();
   InitDriveDirMenu();
   
   // add up dir option, will be the only entry if error occurs
   DriveDirMenu[0].ItemType = rtDirectory;
   AddDirEntry(UpDirString);
   
   File myFile = sourceFS->open(DriveDirPath, FILE_READ); //path includes d64/71/81 file
   struct DxxImage Dxx;
   if (!myFile) return;
   
   Printf_dbg("Loading dir from %s (%lu bytes) type: %d\n", DriveDirPath, (uint32_t)myFile.size(), DiskType);
   if (!DxxOpen(&Dxx, &myFile, DiskType))
   {
      //Create dummy file/name to display err message
      DriveDirMenu[1].ItemType = rtUnknown;
      AddDirEntry("*Error: Wrong file size*");
      myFile.close();
      return;
   }
   
   uint8_t Track = 18;
   uint8_t Sector = 1;
   uint8_t SecOffset = 0;
   uint8_t FileName[16];
   uint8_t NextTrack, NextSect, FileType, FileTrack, FileSect;
   
   if(DiskType == rtD81)
   {
      Track = 40;
      Sector = 3;
   }
   
   //An image's directory chain can link back to itself; a real one is at most 37 sectors (D81)
   const uint8_t MaxDxxDirSectors = 64;
   uint8_t DirSectorsRead = 0;
   while(Track != 0 && DirSectorsRead++ < MaxDxxDirSectors && NumDrvDirMenuItems < MaxMenuItems)
   {
      long CurTSOffset = d64_sector_offset(&Dxx.Disk, Track, Sector);
      Printf_dbg("Track:%d  Sector:%d = DxxOffset:$%lx\n", Track, Sector, CurTSOffset);
      if (CurTSOffset < 0) break; //a link off the image

      do
      {
         myFile.seek(CurTSOffset+SecOffset);

         NextTrack = myFile.read(); //0x00 Only first is valid, rest in sect should be 00
         NextSect  = myFile.read(); //0x01 Only first is valid, rest in sect should be 00
         FileType  = myFile.read(); //0x02
         FileTrack = myFile.read(); //0x03
         FileSect  = myFile.read(); //0x04
         myFile.read(FileName, DxxFNB_NameLength); //0x05-0x14, padded with $a0 (space)

         Printf_dbg("  SecOffset $%02x:  NTrack:%02d NSect:%02d Type:$%02x FTrack:%02d FSect:%02d\n", 
            SecOffset, NextTrack, NextSect, FileType, FileTrack, FileSect);             

         if(SecOffset == 0)
         {
            Track = NextTrack; //will be 0 if reading last sector
            Sector = NextSect;
         }
         
         if (FileName[0] && NumDrvDirMenuItems >= MaxMenuItems) SecOffset = 0; //array full: stop
         else if (FileName[0]) //check for end of dir, no entry
         {  //valid dir entry
            DriveDirMenu[NumDrvDirMenuItems].Name = (char*)malloc(DxxFNB_Bytes); // 16 char max + term + ftrack + fsec + DiskType

            for(uint8_t CharNum=0; CharNum<DxxFNB_NameLength; CharNum++)
            { //converting to ascii, then back to petscii for display later.  All other file names are ascii...
               if (FileName[CharNum] & 0x80) FileName[CharNum] &= 0x7f; //Cap petscii to ascii
               else if (FileName[CharNum] & 0x40) FileName[CharNum] |= 0x20;  //lower case petscii to ascii
            }
            
            memcpy(DriveDirMenu[NumDrvDirMenuItems].Name, FileName, DxxFNB_NameLength); 
            DriveDirMenu[NumDrvDirMenuItems].Name[DxxFNB_NameTerm] = 0; //terminate it
            DriveDirMenu[NumDrvDirMenuItems].Name[DxxFNB_Track]    = FileTrack; //store start track in case we need to load later 
            DriveDirMenu[NumDrvDirMenuItems].Name[DxxFNB_Sector]   = FileSect;  //store start sector
            DriveDirMenu[NumDrvDirMenuItems].Name[DxxFNB_DiskType] = DiskType;  //store disk type
            DriveDirMenu[NumDrvDirMenuItems].ItemType = ItemTypeFromDxxFileType(FileType);
            
            Printf_dbg("   Name:%s\n", DriveDirMenu[NumDrvDirMenuItems].Name);             
            SecOffset +=0x20;  //uint8_t rolls over to 0x00 at end of 256 byte sector
            NumDrvDirMenuItems++;
         }
         else
         { // end of dir, Track should also be 0 at this point
            SecOffset = 0; 
         }    
         
      } while (SecOffset != 0);
   
   }
   Printf_dbg("Loaded %d items in %lumS\n", NumDrvDirMenuItems, (millis()-beginWait));
   myFile.close();

}

FLASHMEM bool LoadDxxFile(StructMenuItem* MyMenuItem, FS *sourceFS) 
{
   DriveDirPath[strlen(DriveDirPath)-1] = 0; //remove the "*" indicator at end
   File myFile = sourceFS->open(DriveDirPath, FILE_READ); //path includes d64 file
   SendMsgPrintfln("Loading:\r\n%s\r\nFrom:\r\n%s", MyMenuItem->Name, DriveDirPath);
   strcat(DriveDirPath, "*"); //put the marker back
   
   if (!myFile) 
   {
      SendMsgPrintfln("Dxx File Not Found");
      return false;
   }
   
   struct DxxImage Dxx;
   int32_t Length = DxxOpen(&Dxx, &myFile, MyMenuItem->Name[DxxFNB_DiskType])
                  ? MenuDiskReadFile(&Dxx.Disk, MyMenuItem->Name[DxxFNB_Track], MyMenuItem->Name[DxxFNB_Sector], RAM_Image, RAM_ImageSize)
                  : MENUDISK_BROKEN;

   if (Length < 0)
   {
      if (Length == MENUDISK_TOO_BIG) SendMsgPrintfln(PSTR("File too large"));
      else SendMsgPrintfln(PSTR("File could not be read"));
      myFile.close();
      return false;
   }
   MyMenuItem->Size = Length;

   SendMsgPrintfln("Done");
   myFile.close();
   Printf_dbg("Size:$%04x\n", MyMenuItem->Size); 
   
   //file writeback for debug purposes
   //Serial.printf("Writing file to SD/test.prg\n");
   //File writebackFile = SD.open("/test.prg", FILE_WRITE);
   //writebackFile.write(RAM_Image, MyMenuItem->Size);
   //writebackFile.close();
   
   return true;   
}