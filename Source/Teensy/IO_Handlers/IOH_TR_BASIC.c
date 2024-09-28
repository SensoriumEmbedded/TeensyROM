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

#define TgetQueueSize      256
#define TgetQueueUsed      ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+TgetQueueSize-RxQueueTail))

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
   TR_BASStreamDataReg   = 0xba,   // (Read Only) File transfer stream data
   TR_BASStrAvailableReg = 0xbc,   // (Read Only) Signals stream data available

   // Control Reg Commands:
   TR_BASCont_None       = 0x00,   // No Action to be taken
   TR_BASCont_SendFN     = 0x02,   // Prep to send Filename from BAS to TR
   TR_BASCont_LoadPrep   = 0x04,   // Prep to load file from TR
   
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

uint8_t ContRegAction_LoadPrep()
{ //load file into RAM, returns TR_BASStatRegVal                
   //check that file exists & load into RAM
   
   //if (strlen(FilePath) == 1 && FilePath[0] == '/') sprintf(FullFilePath, "%s%s", FilePath, MyMenuItem->Name);  // at root
   //else sprintf(FullFilePath, "%s/%s", FilePath, MyMenuItem->Name);
      
   Printf_dbg("Loading: %s\n", LSFileName);

   FS *sourceFS = &firstPartition;
   //if(LatestSIDLoaded[0] == rmtSD)
   //{
   //   sourceFS = &SD;
   //   if(!SD.begin(BUILTIN_SDCARD)) // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   //   {
   //      Printf_dbg("SD Init Fail\n");
   //   }
   //}

   File myFile = sourceFS->open((char*)LSFileName, FILE_READ);
   
   //if (sourceFS != &SD) FullFilePath[0] = 0; //terminate if not SD
   
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
   Printf_dbg("Size: 0x%08x bytes\n", XferSize);
   if(XferSize > (0xc000-0x0801)) //fit in BASIC area but don't overwrite custom commands
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
      myFile.close();
      return BAS_ERROR_FILE_DATA;   
   }
   
   StreamOffsetAddr = 0; //set to start of data
   TR_BASStrAvailableRegVal = 0xff;    // transfer available flag   
   Printf_dbg("Done\n");
   return TR_BASStat_Ready;   //TR RAM Load Sussceful, ready to x-fer to C64
}

//__________________________________________________________________________________

void InitHndlr_TR_BASIC()
{
   if (TgetQueue == NULL) TgetQueue = (uint8_t*)malloc(TgetQueueSize);
   if (LSFileName == NULL) LSFileName = (uint8_t*)malloc(MaxPathLength);
   
   RxQueueHead = RxQueueTail = 0; //as used in Swiftlink & ASID
 
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
                  break;
               case TR_BASCont_LoadPrep: //load file into RAM 
                  TR_BASContRegAction = Data;
                  TR_BASStatRegVal = TR_BASStat_Processing; //initialize status
                  break;
            }
            break;
            
         case TR_BASFileNameReg: //receive file name characters
            LSFileName[FNCount++] = Data;
            if (Data == 0)
            {
               Printf_dbg("Received FN: %s\n", LSFileName);
            }
            break;
      }
   } //write
}

void PollingHndlr_TR_BASIC()
{
   if (TR_BASContRegAction == TR_BASCont_LoadPrep)
   {
      TR_BASStatRegVal = ContRegAction_LoadPrep();
      
      TR_BASContRegAction = TR_BASCont_None;
   }
   
   if (Serial.available())
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
   

