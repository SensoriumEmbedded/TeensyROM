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


//IO Handler for TR_BASIC 

void IO1Hndlr_TR_BASIC(uint8_t Address, bool R_Wn);  
void PollingHndlr_TR_BASIC();                           
void InitHndlr_TR_BASIC();                           

stcIOHandlers IOHndlr_TR_BASIC =
{
  "TR_BASIC",              //Name of handler
  &InitHndlr_TR_BASIC,     //Called once at handler startup
  &IO1Hndlr_TR_BASIC,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_TR_BASIC,  //Polled in main routine
  NULL,                     //called at the end of EVERY c64 cycle
};

#define TgetQueueSize      4096
#define TgetQueueUsed      ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+TgetQueueSize-RxQueueTail))
#define MainMemLoc         0xc000  //see MainMemLoc in assy code, start of BASIC extension code
#define MainMemLocEnd      0xd000  //see MainMemLocEnd in compiled symbols ($caf5 as of 4/20/25, adding buffer)

uint8_t* TgetQueue = NULL;  //to hold incoming messages
uint8_t* LSFileName = NULL;
extern uint32_t RxQueueHead, RxQueueTail;
uint16_t FNCount;
uint8_t  TR_BASContRegAction, TR_BASStatRegVal, TR_BASStrAvailableRegVal;


enum TR_BASregsMatching  //synch with TRCustomBasicCommands\source\main.asm
{
   // registers:
   TR_BASDataReg         = 0xb2,   // (R/W) for TPUT/TGET data  
   TR_BASContReg         = 0xb4,   // (Write only) Control Reg 
   TR_BASStatReg         = 0xb6,   // (Read only) Status Reg 
   TR_BASFileNameReg     = 0xb8,   // (Write only) File name transfer
   TR_BASStreamDataReg   = 0xba,   // (R/W Only) File transfer stream data
   TR_BASStrAvailableReg = 0xbc,   // (Read Only) Signals stream data available

   // Control Reg Commands:
   TR_BASCont_None       = 0x00,   // No Action to be taken
   TR_BASCont_SendFN     = 0x02,   // Prep to send Filename from BAS to TR
   TR_BASCont_LoadPrep   = 0x04,   // Prep to load file from TR RAM
   TR_BASCont_SaveFinish = 0x06,   // Save file from TR RAM to SD/USB
   TR_BASCont_DirPrep    = 0x08,   // Load Dir into TR RAM
   TR_BASCont_DmaTest    = 0x0a,   // Assert DMA for 100mS
   
   // StatReg Values:
   TR_BASStat_Processing = 0x00,   // No update, still processing
   //Do not conflict with BASIC_Error_Codes (basic.ERROR_*)
   TR_BASStat_Ready      = 0x55,   // Ready to Transfer

}; //end enum synch

enum BASIC_Error_Codes
{
   BAS_ERROR_TOO_MANY_FILES         = 0x01,
   BAS_ERROR_FILE_OPEN              = 0x02,
   BAS_ERROR_FILE_NOT_OPEN          = 0x03,
   BAS_ERROR_FILE_NOT_FOUND         = 0x04,
   BAS_ERROR_DEVICE_NOT_PRESENT     = 0x05,
   BAS_ERROR_NOT_INPUT_FILE         = 0x06,
   BAS_ERROR_NOT_OUTPUT_FILE        = 0x07,
   BAS_ERROR_MISSING_FILENAME       = 0x08,
   BAS_ERROR_ILLEGAL_DEVICE_NUM     = 0x09,
   BAS_ERROR_NEXT_WITHOUT_FOR       = 0x0a,
   BAS_ERROR_SYNTAX                 = 0x0b,
   BAS_ERROR_RETURN_WITHOUT_GOSUB   = 0x0c,
   BAS_ERROR_OUT_OF_DATA            = 0x0d,
   BAS_ERROR_ILLEGAL_QUANTITY       = 0x0e,
   BAS_ERROR_OVERFLOW               = 0x0f,
   BAS_ERROR_OUT_OF_MEMORY          = 0x10,
   BAS_ERROR_UNDEFD_STATEMENT       = 0x11,
   BAS_ERROR_BAD_SUBSCRIPT          = 0x12,
   BAS_ERROR_REDIMD_ARRAY           = 0x13,
   BAS_ERROR_DIVISION_BY_ZERO       = 0x14,
   BAS_ERROR_ILLEGAL_DIRECT         = 0x15,
   BAS_ERROR_TYPE_MISMATCH          = 0x16,
   BAS_ERROR_STRING_TOO_LONG        = 0x17,
   BAS_ERROR_FILE_DATA              = 0x18,
   BAS_ERROR_FORMULA_TOO_COMPLEX    = 0x19,
   BAS_ERROR_CANT_CONTINUE          = 0x1a,
   BAS_ERROR_UNDEFD_FUNCTION        = 0x1b,
   BAS_ERROR_VERIFY                 = 0x1c,
   BAS_ERROR_LOAD                   = 0x1d,
}; //end BASIC_Error_Codes


//__________________________________________________________________________________


FS *FSfromFileName(char** ptrptrLSFileName)
{  //returns file system type (default USB) and changes pointer to skip USB:/SD:

   FS *sourceFS = &firstPartition; //default to USB
   char *ptrLSFileName = *ptrptrLSFileName;
   char LSFileNameTmp[4];
   
   //copy first 4 chars as lower case:
   for(uint8_t num=0; num<4; num++) LSFileNameTmp[num]=tolower(ptrLSFileName[num]);
   
   if(memcmp(LSFileNameTmp, "sd:", 3) == 0)
   {
      ptrLSFileName += 3;
      sourceFS = &SD;
      Printf_dbg("SD:*\n");
      if(!SD.begin(BUILTIN_SDCARD)) // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
      {
         Printf_dbg("SD Init Fail\n");
         return NULL;   //return BAS_ERROR_DEVICE_NOT_PRESENT;
      }
   }
   else if(memcmp(LSFileNameTmp, "usb:", 4) == 0)
   {
      ptrLSFileName += 4;      
      //sourceFS = &firstPartition; //already default
      Printf_dbg("USB:*\n");
   }
   //else if(memcmp(ptrLSFileName, "TR:", 3) == 0)
   //{
   //   MenuSourceID = rmtTeensy;
   //   ptrLSFileName += 3;      
   //}
   else
   {
      Printf_dbg("SD:/USB: not found, default USB\n"); //default to USB if not specified
   }
   
   *ptrptrLSFileName = ptrLSFileName; //update the pointer
   return sourceFS;
}


void AddToRAM_Image(const char *ToAdd)
{  //and convert to petscii
   uint32_t count = 0;
   
   while(1)
   {
      RAM_Image[XferSize] = ToPETSCII(ToAdd[count]);
      if (ToAdd[count] == 0) return;
      XferSize++; count++;
   }
}


uint8_t ContRegAction_LoadPrep()
{ //load file into RAM, returns TR_BASStatRegVal                
   //check that file exists & load into RAM_Image
   
   char* ptrLSFileName = (char*)LSFileName; //local pointer
   FS *sourceFS = FSfromFileName(&ptrLSFileName);
   
   if(sourceFS == NULL) return BAS_ERROR_DEVICE_NOT_PRESENT;

   Printf_dbg("Load: %s\n", ptrLSFileName);
   File myFile = sourceFS->open(ptrLSFileName, FILE_READ);
      
   if (!myFile) 
   {
      Printf_dbg("File Not Found\n");
      return BAS_ERROR_FILE_NOT_FOUND;
   }
   
   if (myFile.isDirectory())
   {
      Printf_dbg("File is Dir\n"); 
      myFile.close();
      return BAS_ERROR_NOT_INPUT_FILE;   
   }
   
   XferSize = myFile.size();
   Printf_dbg("Size: %d bytes\n", XferSize);
   if(XferSize > 0xc000) //Max to fit below custom commands at $c000
   {
      Printf_dbg("File too large\n"); 
      myFile.close();
      return BAS_ERROR_OUT_OF_MEMORY;    
   }

   uint32_t count = 0;
   while (myFile.available() && count < XferSize) RAM_Image[count++]=myFile.read();

   myFile.close();
   if (count != XferSize)
   {
      Printf_dbg("Size Mismatch\n");
      return BAS_ERROR_FILE_DATA;   
   }
   
   uint16_t PStart = (RAM_Image[1]<<8) | RAM_Image[0];
   Printf_dbg("Addr: $%04x to $%04x\n", PStart, PStart+XferSize);
   if (PStart < MainMemLocEnd && PStart+XferSize >= MainMemLoc)
   {
      Printf_dbg("Conflict with ext code\n");
      return BAS_ERROR_OUT_OF_MEMORY;  //make this a custom message? 
   }      
   //if (RAM_Image[0] != 0x01 || RAM_Image[1] != 0x08)  //BAS_ERROR_TYPE_MISMATCH;   
   
   TR_BASStrAvailableRegVal = 0xff;    // transfer available flag   
   Printf_dbg("Done\n");
   return TR_BASStat_Ready;   //TR RAM Load Sussceful, ready to x-fer to C64
}


uint8_t ContRegAction_SaveFinish()
{  //file was transfered to RAM_Image[], size=StreamOffsetAddr  
   //save file from RAM, returns TR_BASStatRegVal                

   char* ptrLSFileName = (char*)LSFileName; //local pointer
   FS *sourceFS = FSfromFileName(&ptrLSFileName);
   
   if(sourceFS == NULL) return BAS_ERROR_DEVICE_NOT_PRESENT;

   Printf_dbg("Save: %s\nSize: %d bytes\n", ptrLSFileName, StreamOffsetAddr);
   sourceFS->remove(ptrLSFileName); //del prev version to overwrite!
   File myFile = sourceFS->open(ptrLSFileName, FILE_WRITE); //O_RDWR | O_CREAT <- doesn't reduce filesize if smaller 
      
   if (!myFile) 
   {
      Printf_dbg("Failed to open\n");
      return BAS_ERROR_FILE_NOT_OPEN;
   }

   uint32_t BytesWritten = myFile.write(RAM_Image, StreamOffsetAddr);
   myFile.close();
   
   if (BytesWritten != StreamOffsetAddr)
   {
      Printf_dbg("Not Fully Written\n");
      return BAS_ERROR_OUT_OF_DATA; 
   }

   return TR_BASStat_Ready;   //Save Sussceful
}


uint8_t ContRegAction_DirPrep()
{ //load dir into RAM, returns TR_BASStatRegVal                
   //check that dir exists & load into RAM_Image
   
   char* ptrLSFileName = (char*)LSFileName; //local pointer
   FS *sourceFS = FSfromFileName(&ptrLSFileName);
   if(sourceFS == NULL) return BAS_ERROR_DEVICE_NOT_PRESENT;

   if (ptrLSFileName[0] == 0) sprintf(ptrLSFileName, "/");  // default to root if zero len
   Printf_dbg("Dir: \"%s\"\n", ptrLSFileName);

   File dir = sourceFS->open(ptrLSFileName);
   
   if (!dir) 
   {
      Printf_dbg("Dir Not Found\n");
      return BAS_ERROR_FILE_NOT_FOUND;
   }
   
   const char *filename;
   XferSize = 0; //initialize RAM_Image size/count
   AddToRAM_Image("Contents of: \"");
   AddToRAM_Image(ptrLSFileName);
   AddToRAM_Image("\"\r");
   
   while (File entry = dir.openNextFile()) 
   {
      filename = entry.name();
      
      if (entry.isDirectory()) AddToRAM_Image(" /");
      else AddToRAM_Image("  ");
      
      AddToRAM_Image(filename);
      AddToRAM_Image("\r");
      
      Printf_dbg("%s\n", filename);
      
      entry.close();
      if (XferSize >= RAM_ImageSize-80)
      {
         AddToRAM_Image("*** Too many files!\r");         
         break;
      }
   }
   
   dir.close();
   TR_BASStrAvailableRegVal = 0xff;    // transfer available flag   Need this???????????
   Printf_dbg("Done\n");
   return TR_BASStat_Ready;   //Save Sussceful
}

uint8_t ContRegAction_DmaTest()
{  // Assert DMA for 100mS and release
   Printf_dbg("DMA Trig\n");
   delay(100);  //100
   DMA_State = DMA_S_StartDisable;
   return TR_BASStat_Ready;  
}

//__________________________________________________________________________________

void InitHndlr_TR_BASIC()
{
   if (TgetQueue == NULL) TgetQueue = (uint8_t*)malloc(TgetQueueSize);
   if (LSFileName == NULL) LSFileName = (uint8_t*)malloc(MaxPathLength);
   
   RxQueueHead = RxQueueTail = 0; //as used in Swiftlink & ASID
 
   TR_BASStatRegVal = TR_BASStat_Ready;
   TR_BASContRegAction = TR_BASCont_None; //default to no action
}   

void IO1Hndlr_TR_BASIC(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case TR_BASDataReg:
            if(TgetQueueUsed)
            {
               DataPortWriteWaitLog(TgetQueue[RxQueueTail++]);  
               if (RxQueueTail == TgetQueueSize) RxQueueTail = 0;
            }
            else  //no data to send, send 0
            {
               DataPortWriteWaitLog(0);
            }
            break;
         case TR_BASStatReg:
            DataPortWriteWaitLog(TR_BASStatRegVal);
            break;
         case TR_BASStreamDataReg:
            DataPortWriteWait(RAM_Image[StreamOffsetAddr]);
            //inc on read, check for end:
            if (++StreamOffsetAddr >= XferSize) TR_BASStrAvailableRegVal=0; //signal end of transfer
            break;
         case TR_BASStrAvailableReg:
            DataPortWriteWait(TR_BASStrAvailableRegVal);
         default: //used for all other IO1 reads
            //DataPortWriteWaitLog(0); 
            break;
      }
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead(); 
      TraceLogAddValidData(Data);
      switch(Address)
      {
         case TR_BASDataReg:
            Serial.write(Data); //a bit risky doing this here, but seems fast enough in testing
            break;
            
         case TR_BASContReg:
            //Control reg actions:
            switch(Data)
            {
               case TR_BASCont_SendFN: //file name being sent next
                  FNCount = 0;
                  StreamOffsetAddr = 0; //initialize for file load/save
                  break;
                  
               //these commandd require action outside of interrupt: 
               case TR_BASCont_DmaTest:    // Assert DMA for 100mS
                  DMA_State = DMA_S_StartActive; //start DMA on VIC Phase of this cycle

                  //DMA_State = DMA_S_Start_BA_Active; //Apply DMA When BA is low (bad line)

                  //SetDMAAssert; //start immediately (during Phi2 Low)
                  //DMA_State = DMA_S_ActiveReady;

                  // break keyword is not present, all the cases after the matching case are executed
               case TR_BASCont_LoadPrep:   //load file into RAM 
               case TR_BASCont_SaveFinish: //save file from RAM
               case TR_BASCont_DirPrep:    // Load Dir into RAM
                  TR_BASContRegAction = Data; //pass it to process outside of interrupt
                  TR_BASStatRegVal = TR_BASStat_Processing; //initialize status
                  break;
            }
            break;
            
         case TR_BASFileNameReg: //receive file name characters
            //// PETSCII To Lcase ASSCII:
            //Data &= 0x7f; //bit 7 is Cap in Graphics mode
            //if (Data & 0x40) Data |= 0x20;  //conv to lower case
            
            // PETSCII To ASSCII:
            if (Data & 0x80) Data &= 0x7f; //bit 7 is Cap in Graphics mode
            else if (Data & 0x40) Data |= 0x20;  //conv to lower case
         
            LSFileName[FNCount++] = Data;
            if (Data == 0)
            {
               Printf_dbg("Received FN: \"%s\"\n", LSFileName);
            }
            break;
         case TR_BASStreamDataReg: //receive save data
            RAM_Image[StreamOffsetAddr++] = Data;
            break;
      }
   } //write
}

void PollingHndlr_TR_BASIC()
{
   if (TR_BASContRegAction != TR_BASCont_None)
   {
      switch(TR_BASContRegAction)
      {
         case TR_BASCont_LoadPrep:
            TR_BASStatRegVal = ContRegAction_LoadPrep();
            break;
         case TR_BASCont_SaveFinish:
            TR_BASStatRegVal = ContRegAction_SaveFinish();
            break;
         case TR_BASCont_DirPrep:
            TR_BASStatRegVal = ContRegAction_DirPrep();
            break;
         case TR_BASCont_DmaTest:
            TR_BASStatRegVal = ContRegAction_DmaTest();
            break;         
         default:
            Printf_dbg("Unexpected TR_BASContRegAction: %d\n", TR_BASContRegAction);
      }
      TR_BASContRegAction = TR_BASCont_None;
   }
   
   while (Serial.available())
   {
      uint8_t Cin = Serial.read();
      if(TgetQueueUsed >= TgetQueueSize-1)
      {
         Printf_dbg("\n**Queue Full!\n");
         //loose the char and return
      }       
      else
      {
         //add to queue:
         Printf_dbg("#%d= %d (%c)\n", TgetQueueUsed, Cin, Cin);
         TgetQueue[RxQueueHead++] = Cin;
         if (RxQueueHead == TgetQueueSize) RxQueueHead = 0;
      }
      
   }
}
   

