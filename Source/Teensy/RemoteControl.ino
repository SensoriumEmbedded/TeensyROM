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

//Functions to control C64/TR via USB connection


//  TR: Set up wRegIRQ_ACK, rwRegIRQ_CMD, & launch menu info (if needed)
//  TR: Assert IRQ, wait for ack1
// C64: IRQ handler catches, sets local reg (smcIRQFlagged) and sends ack1 to wRegIRQ_ACK
//  TR: sees ack1, deasserts IRQ, waits for ack2 (echo of command)
// C64: Main code sees local reg, reads IRQ command, and echoes it to ack (ack 2)
//  TR: sees ack2, success/return
// C64: Executes command

bool InterruptC64(RegIRQCommands IRQCommand)
{
   IO1[rwRegIRQ_CMD] = IRQCommand;
   bool IRQSuccess = DoC64IRQ();
   IO1[rwRegIRQ_CMD] = ricmdNone; //always set back to none/0 for default/protection 
   return IRQSuccess;   
}

bool DoC64IRQ()
{
   uint32_t beginWait = millis();
   IO1[wRegIRQ_ACK] = ricmdNone;
   SetIRQAssert;
   
   //wait for C64 app to respond from IRQ 
   while (IO1[wRegIRQ_ACK] != ricmdAck1) 
   {
      if (millis()-beginWait > 50) 
      {
         SetIRQDeassert;
         Printf_dbg("Ack1 Timeout\n");
         return false; // Timeout, Ack 1 (from IRQ)
      }
   }
   SetIRQDeassert;
   Printf_dbg("Ack 1 took %lumS\n", (millis()-beginWait));
   
   //now wait for it to respond from main loop
   while (IO1[wRegIRQ_ACK] == ricmdAck1) 
   {
      if (millis()-beginWait > 200) 
      {
         Printf_dbg("Ack2 Timeout\n");
         return false; // Timeout, Ack 2 (from main code)
      }
   }
   Printf_dbg("Ack 1+2 took %lumS\n", (millis()-beginWait));
   
   if (IO1[wRegIRQ_ACK] != IO1[rwRegIRQ_CMD]) //mismatch!
   {
      Printf_dbg("Mismatch\n");
      return false; // echoed ack2 does not match command sent
   }
   
   return true;
}

FLASHMEM bool RemotePauseSID()
{  //assumes TR is not "busy" (Handler active)
   return InterruptC64(ricmdSIDPause);
}

// Command: 
// Set one of the colors in the TR UI 
//    Color will be stored in EEPROM and IO space, TR menu will need to be reset to take visual effect
//
// Workflow:
// Receive <-- SetColorToken Token 0x6422
// Receive <-- Color reference to set (Range 0 to NumColorRefs-1) See enum ColorRefOffsets
// Receive <-- Color to set (Range 0 to 15)  See enum PokeColors
// Send --> AckToken 0x64CC or FailToken 0x9B7F
FLASHMEM bool SetColorRef()
{

   if (!SerialAvailabeTimeout()) return false;
   uint8_t ColRefNum = CmdChannel->read();

   if (!SerialAvailabeTimeout()) return false;
   uint8_t ColorNum = CmdChannel->read();
   
   if (ColRefNum >= NumColorRefs) return false;
   if (ColorNum  > 15) return false;
   
   IO1[rwRegColorRefStart+ColRefNum]=ColorNum;
   EEPROM.write(eepAdColorRefStart+ColRefNum , ColorNum);
   
   return true;
}

// Command: 
// Set sub-song number of currently loaded SID
//
// Workflow:
// Receive <-- SetSIDSongToken Token 0x6488 
// Receive <-- Song number to set (1 byte, zero based, song 1 is 0) 
// Send --> AckToken 0x64CC or FailToken 0x9B7F
FLASHMEM bool SetSIDSong()
{  //assumes TR is not "busy" (Handler active)
   if(!SerialAvailabeTimeout()) return false;
   uint8_t NewSongNumZ = CmdChannel->read();
   
   if(NewSongNumZ > IO1[rRegSIDNumSongsZ]) return false;
   IO1[rwRegSIDSongNumZ] = NewSongNumZ;
   return InterruptC64(ricmdSIDInit);
}


// Command: 
// Set SID playback speed of currently loaded SID
//
// Workflow:
// Receive <-- SIDSpeedLinToken  0x6499 -or- SIDSpeedLogToken  0x649A
// Receive <-- playback rate (16 bit signed int as 2 bytes: hi, lo)
//                Linear Range is -68(*256) to <128(*256), argument represents speed change percent from nominal
//                Logrithmic Range is -127(*256) to 99(*256) argument to percentage shown in "SID playback speed-log.txt"
// Send --> AckToken 0x64CC or FailToken 0x9B7F
//
// Example 1: 0x64, 0x99, 0xf0, 0x40 = Set to -15.75 via linear equation
// Example 2: 0x64, 0x9a, 0x20, 0x40 = set to +32.25 via logarithmic equation
FLASHMEM bool RemoteSetSIDSpeed(bool LogConv)
{  //assumes TR is not "busy" (Handler active)

   uint32_t PlaybackSpeedIn;
   if (!GetUInt(&PlaybackSpeedIn, 2)) return false;

   if (!SetSIDSpeed(LogConv, (int16_t)PlaybackSpeedIn)) return false;
     
   return InterruptC64(ricmdSetSIDSpeed);
}


// Command: 
// Set individual SID voice muting
//
// Workflow:
// Receive <-- SIDVoiceMuteToken   0x6433
// Receive <-- voice mute info (1 byte)
//                bit 0=  Voice 1  on=0, mute=1
//                bit 1=  Voice 2  on=0, mute=1
//                bit 2=  Voice 3  on=0, mute=1
//             bits 7:3= Zero
// Send --> AckToken 0x64CC or FailToken 0x9B7F
FLASHMEM bool RemoteSetSIDVoiceMute()
{  //assumes TR is not "busy" (Handler active)

   uint32_t VoiceMuteInfo;
   if (!GetUInt(&VoiceMuteInfo, 1)) return false;

   if (VoiceMuteInfo > 0x07) return false;
   
   IO1[rwRegScratch]= VoiceMuteInfo; //use scratch reg to transfer mute info
   return InterruptC64(ricmdSIDVoiceMute);
}


void RemoteLaunch(RegMenuTypes MenuSourceID, const char *FileNamePath, bool DoCartDirect)
{  //assumes file exists & TR is not "busy" (Handler active)
   
   //Printf_dbg("Launching: SrcID%d DoCRT%d \"%s\"\n", MenuSourceID, DoCartDirect, FileNamePath);
   RemoteLaunched = true;
   //Set selected drive
   IO1[rWRegCurrMenuWAIT] = MenuSourceID;

   if (MenuSourceID == rmtSD) SDFullInit(); // SD.begin(BUILTIN_SDCARD); with retry if presence detected

   if (MenuSourceID == rmtUSBDrive) USBFileSystemWait(); //wait up to 1.5 sec in case USB drive just changed or powered up
   
   //set path & filename
   strcpy(DriveDirPath, FileNamePath);
   char* ptrFilename = strrchr(DriveDirPath, '/'); //pointer file name, find last slash
   if (ptrFilename == NULL) 
   {  //no path:
      strcpy(DriveDirPath, "/");
      ptrFilename = (char*)FileNamePath; 
   }
   else
   {  //separate path/filename
      *ptrFilename = 0; //terminate DriveDirPath
      ptrFilename++; //inc to point to filename
   }
   
   //free mem for DriveDirMenu in case current (non-tr) handler is using it all
   FreeCrtChips();
   FreeSwiftlinkBuffs();
   InitDriveDirMenu();

   if (MenuSourceID == rmtTeensy)
   {
      //point MenuSource to directory
      int16_t MenuNum = -1;
      StructMenuItem* DefTRMenu = TeensyROMMenu;  //default to root menu
      uint16_t NumMenuItems = sizeof(TeensyROMMenu)/sizeof(StructMenuItem);
      
      if(strcmp(DriveDirPath, "/") !=0 )
      {//find dir menu
         MenuNum = FindTRMenuItem(DefTRMenu, NumMenuItems, DriveDirPath);
         if(MenuNum<0)
         {
            Printf_dbg("No TR Dir \"%s\"\n", DriveDirPath);
            //Somehow notify user?  
            return;
         }
         DefTRMenu = (StructMenuItem*)TeensyROMMenu[MenuNum].Code_Image;
         NumMenuItems = TeensyROMMenu[MenuNum].Size/sizeof(StructMenuItem);
      }
      Printf_dbg("TR Dir Num = %d\n", MenuNum);
      
      //find file name
      MenuNum = FindTRMenuItem(DefTRMenu, NumMenuItems, ptrFilename);
      if(MenuNum<0)
      {
         Printf_dbg("No TR File \"%s\"\n", ptrFilename);
         //Somehow notify user?  
         return;
      }   
      Printf_dbg("TR File Num = %d\n", MenuNum);

      //point to item # matching filename
      MenuSource = DefTRMenu;
      SetNumItems(NumMenuItems);
      IO1[rwRegCursorItemOnPg] = MenuNum;
      SelItemFullIdx = MenuNum;  //  "Select" item
   }
   else
   {
      // Set up DriveDirMenu to point to file to load
      //    without doing LoadDirectory(&SD/&firstPartition);
      Printf_dbg("Dir Setup\n");
      SetDriveDirMenuNameType(0, ptrFilename);  //not worried about out of memory here (first/only item)
      NumDrvDirMenuItems = 1;
      MenuSource = DriveDirMenu; 
      SetNumItems(1); //sets # of menu items
      IO1[rwRegCursorItemOnPg] = 0;
      SelItemFullIdx = 0;  //  "Select" item
   }
   
   //Printf_dbg("Remote Launch:\nP: %s\nF: %s\n", DriveDirPath, ptrFilename);
   Serial.printf("Remote Launch:\nP: %s\nF: %s\n", DriveDirPath, ptrFilename);

   if (DoCartDirect)
   {  //If a CRT, start and reset directly:
      switch(MenuSource[SelItemFullIdx].ItemType)
      {
         case rtFileCrt:
         case rtBin16k:
         case rtBin8kHi:
         case rtBin8kLo:
         case rtBinC128:
            Printf_dbg("Forced CRT launch\n"); 
            SendC64Msgs = false;
            HandleExecution();
            SendC64Msgs = true;
            if(doReset) return; //success, proceed to reset in main loop
            
            Printf_dbg("Unsuccesful\n"); 
            //Try again via menu so that fail message is displayed on C64...
            break;
      }
   }

   //Get the attention of the C64 via IRQ or reset:
   if(CurrentIOHandler == IOH_TeensyROM)
   {
      Printf_dbg("Interrupt/launch\n"); 
      if(InterruptC64(ricmdLaunch)) return;
   }

   //force reset then launch
   Printf_dbg("Reset/launch\n"); 
   IO1[rwRegIRQ_CMD] = ricmdLaunch;
   SetUpMainMenuROM(); //includes DoReset flag set
   
}

