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


uint32_t D64Offset(uint8_t Track, uint8_t Sector)
{
   if (Track<18) return (        ((Track- 1)*21+Sector)*256);
   if (Track<25) return (0x16500+((Track-18)*19+Sector)*256);
   if (Track<31) return (0x1ea00+((Track-25)*18+Sector)*256);
                 return (0x25600+((Track-31)*17+Sector)*256);
}

void LoadD64Directory(FS *sourceFS) 
{
   //Interpreted from: https://ist.uwaterloo.ca/~schepers/formats/D64.TXT
   
   uint32_t beginWait = millis();
   InitDriveDirMenu();
   
   // add up dir option
   DriveDirMenu[0].ItemType = rtDirectory;
   AddDirEntry(UpDirString);
   
   File myFile = sourceFS->open(DriveDirPath, FILE_READ); //path includes d64 file
   if (!myFile) return;
   
   //check size:
     // Disk type                  File Size
     // ---------                  ------
     // Realm.d64                  171520
     // 35 track, no errors        174848
     // 35 track, 683 error bytes  175531
     // 40 track, no errors        196608
     // 40 track, 768 error bytes  197376
   uint32_t FileSize = myFile.size(); 
   Printf_dbg("Loading dir from %s (%lu bytes)\n", DriveDirPath, FileSize);
   if(FileSize < 171520 || FileSize > 197376) 
   {
      Printf_dbg("Unexp file size\n");
      myFile.close();
      return;
   }
   
   uint8_t Track = 18;
   uint8_t Sector = 1;
   uint8_t SecOffset = 0;
   uint8_t FileName[16];
   uint8_t NextTrack, NextSect, FileType, FileTrack, FileSect;
   
   while(Track != 0)
   {
      uint32_t CurTSOffset = D64Offset(Track, Sector);
      Printf_dbg("Track:%d  Sector:%d = D64Offset:$%06x\n", Track, Sector, CurTSOffset); 
      
      do
      {
         myFile.seek(CurTSOffset+SecOffset);

         NextTrack = myFile.read(); //0x00 Only first is valid, rest in sect should be 00
         NextSect  = myFile.read(); //0x01 Only first is valid, rest in sect should be 00
         FileType  = myFile.read(); //0x02
         FileTrack = myFile.read(); //0x03
         FileSect  = myFile.read(); //0x04
         myFile.read(FileName, 16); //0x05-0x14, padded with $a0 (space)

         if(SecOffset == 0)
         {
            Track = NextTrack; //will be 0 if reading last sector
            Sector = NextSect;
         }
         
         if (FileTrack)
         {  //valid dir entry
            DriveDirMenu[NumDrvDirMenuItems].Name = (char*)malloc(19); // 16 char max + term + ftrack + fsec
            
            for(uint8_t CharNum=0; CharNum<16; CharNum++)
            { //converting to ascii, then back to petscii for display later.  All other file names are ascii...
               if (FileName[CharNum] & 0x80) FileName[CharNum] &= 0x7f; //Cap petscii to ascii
               else if (FileName[CharNum] & 0x40) FileName[CharNum] |= 0x20;  //lower case petscii to ascii
            }
            
            memcpy(DriveDirMenu[NumDrvDirMenuItems].Name, FileName, 16); 
            DriveDirMenu[NumDrvDirMenuItems].Name[16] = 0; //terminate it
            DriveDirMenu[NumDrvDirMenuItems].Name[17] = FileTrack; //store start track in case we need to load later 
            DriveDirMenu[NumDrvDirMenuItems].Name[18] = FileSect;  //store start sector in case we need to load later

            if ((FileType & 0x0f) == 0x02) DriveDirMenu[NumDrvDirMenuItems].ItemType = rtFilePrg;
            else DriveDirMenu[NumDrvDirMenuItems].ItemType = rtUnknown;
            //Display DEL, SEQ, USR, and REL as text files?

            Printf_dbg("  SecOffset $%02x:  NTrack:%02d NSect:%02d Type:$%02x FTrack:%02d FSect:%02d Name:%s\n", 
               SecOffset, NextTrack, NextSect, FileType, FileTrack, FileSect, DriveDirMenu[NumDrvDirMenuItems].Name);             
            SecOffset +=0x20;  //uint8_t rolls over to 0x00 at end of 256 byte sector
            NumDrvDirMenuItems++;
         }
         else
         { // FileTrack == 0 is end of dir, Track should also be 0 at this point
            SecOffset = 0; 
         }    
         
      } while (SecOffset != 0);
   
   }
   Printf_dbg("Loaded %d items from d64 in %lumS\n", NumDrvDirMenuItems, (millis()-beginWait));
   myFile.close();

}

bool LoadD64File(StructMenuItem* MyMenuItem, FS *sourceFS) 
{
   DriveDirPath[strlen(DriveDirPath)-1] = 0; //remove the "*" indicator at end
   File myFile = sourceFS->open(DriveDirPath, FILE_READ); //path includes d64 file
   SendMsgPrintfln("Loading:\r\n%s\r\nFrom:\r\n%s", MyMenuItem->Name, DriveDirPath);
   strcat(DriveDirPath, "*"); //put the marker back
   
   if (!myFile) 
   {
      SendMsgPrintfln("File Not Found");
      return false;
   }
   
   uint8_t Track = MyMenuItem->Name[17];
   uint8_t Sector = MyMenuItem->Name[18];
   MyMenuItem->Size = 0;

   //  Load the whole thing into the RAM1 buffer
   while(Track != 0)
   {
      uint32_t CurTSOffset = D64Offset(Track, Sector);
      Printf_dbg("Track:%d  Sector:%d = D64Offset:$%06x\n", Track, Sector, CurTSOffset); 
      
      if (MyMenuItem->Size + 256 > RAM_ImageSize)
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
   return true;   
}