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

enum enDxxFileNameBytes
{
   DxxFNB_NameLength = 16,
   DxxFNB_NameTerm =   16,
   DxxFNB_Track =      17,
   DxxFNB_Sector =     18,
   DxxFNB_DiskType =   19,
   DxxFNB_Bytes =      20,
};

FLASHMEM uint32_t D64Offset(uint8_t Track, uint8_t Sector)
{
   if (Track<18) return (        ((Track- 1)*21+Sector)*256);
   if (Track<25) return (0x16500+((Track-18)*19+Sector)*256);
   if (Track<31) return (0x1ea00+((Track-25)*18+Sector)*256);
                 return (0x25600+((Track-31)*17+Sector)*256);
}

FLASHMEM uint32_t DxxOffset(uint8_t DiskType, uint8_t Track, uint8_t Sector)
{
   if (DiskType == rtD64) return D64Offset(Track, Sector);

   if (DiskType == rtD71)  
   {
      if (Track>35) return (0x2AB00 + D64Offset(Track-35, Sector)); //side 1
      else return D64Offset(Track, Sector);   //side 0
   }
   
   return (((Track- 1)*40+Sector)*256);  // Defaulte to (DiskType == rtD81) 
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
   if (!myFile) return;
   
   uint32_t FileSize = myFile.size(); 
   Printf_dbg("Loading dir from %s (%lu bytes) type: %d\n", DriveDirPath, FileSize, DiskType);
   if((DiskType == rtD64 && (FileSize < 171520 || FileSize > 197376)) ||  // Realm.d64 = 171520
      (DiskType == rtD71 && (FileSize < 349696 || FileSize > 351062)) ||
      (DiskType == rtD81 && (FileSize < 819200 || FileSize > 822400)) ) 
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
   
   while(Track != 0)
   {
      uint32_t CurTSOffset = DxxOffset(DiskType, Track, Sector);
      Printf_dbg("Track:%d  Sector:%d = DxxOffset:$%x\n", Track, Sector, CurTSOffset); 
      
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
         
         if (FileName[0]) //check for end of dir, no entry
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
            
            if ((FileType & 0x0f) == 0x02) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFilePrg;
            else DriveDirMenu[NumDrvDirMenuItems].ItemType = rtUnknown;
            //Display DEL, SEQ, USR, and REL as text files?

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
   
   uint8_t Track    = MyMenuItem->Name[DxxFNB_Track];
   uint8_t Sector   = MyMenuItem->Name[DxxFNB_Sector];
   uint8_t DiskType = MyMenuItem->Name[DxxFNB_DiskType];
   MyMenuItem->Size = 0;

   //  Load the whole thing into the RAM1 buffer
   while(Track != 0)
   {
      uint32_t CurTSOffset = DxxOffset(DiskType, Track, Sector);
      Printf_dbg("Track:%d  Sector:%d = D64Offset:$%06x\n", Track, Sector, CurTSOffset); 
      
      if (MyMenuItem->Size + 254 > RAM_ImageSize)
      {
         SendMsgPrintfln("File too large");
         myFile.close();
         return false;
      }
      myFile.seek(CurTSOffset);
      Track  = myFile.read();  //0x00
      Sector  = myFile.read(); //0x01
      myFile.read(RAM_Image+MyMenuItem->Size, 254); //read in the rest of the sector
      MyMenuItem->Size += 254;
   }
   
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