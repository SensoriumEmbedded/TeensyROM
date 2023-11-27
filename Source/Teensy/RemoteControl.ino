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


//  TR: Set up wRegIRQ_ACK, rRegIRQ_CMD, & launch menu info (if needed)
//  TR: Assert IRQ, wait for ack1
// C64: IRQ handler catches, sets local reg (smcIRQFlagged) and sends ack1 to wRegIRQ_ACK
//  TR: sees ack1, deasserts IRQ, waits for ack2 (echo of command)
// C64: Main code sees local reg, reads IRQ command, and echoes it to ack (ack 2)
//  TR: sees ack2, success/return
// C64: Executes command

bool InterruptC64(RegIRQCommands IRQCommand)
{
   IO1[rRegIRQ_CMD] = IRQCommand;
   bool IRQSuccess = DoC64IRQ();
   IO1[rRegIRQ_CMD] = ricmdNone; //always set back to none/0 for default/protection 
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
   
   if (IO1[wRegIRQ_ACK] != IO1[rRegIRQ_CMD]) //mismatch!
   {
      Printf_dbg("Mismatch\n");
      return false; // echoed ack2 does not match command sent
   }
   
   return true;
}

bool RemotePauseSID()
{  //assumes TR is not "busy" (Handler active)
   return InterruptC64(ricmdSIDPause);
}

void RemoteLaunch(bool SD_nUSB, const char *FileNamePath)
{  //assumes file exists & TR is not "busy" (Handler active)
   
   RemoteLaunched = true;
   //Set selected drive
   IO1[rWRegCurrMenuWAIT] = SD_nUSB ? rmtSD : rmtUSBDrive;
   if (SD_nUSB) SD.begin(BUILTIN_SDCARD); // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   
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
   
   // Set up DriveDirMenu to point to file to load
   //    without doing LoadDirectory(&SD/&firstPartition);
   InitDriveDirMenu();
   SetDriveDirMenuNameType(0, ptrFilename);
   NumDrvDirMenuItems = 1;
   MenuSource = DriveDirMenu; 
   SetNumItems(1); //sets # of menu items
   IO1[rwRegCursorItemOnPg] = 0;
   SelItemFullIdx = 0;  //  "Select" item
   
   Printf_dbg("Remote Launch:\nP: %s\nF: %s\n", DriveDirPath, ptrFilename);
   
   //Get the attention of the C64 via IRQ or reset:
   if(CurrentIOHandler == IOH_TeensyROM)
   {
      Printf_dbg("Interrupt/launch"); 
      if(InterruptC64(ricmdLaunch)) return;
   }

   //force reset then launch
   Printf_dbg("Reset/launch"); 
   IO1[rRegIRQ_CMD] = ricmdLaunch;
   SetUpMainMenuROM(); 
   
}

