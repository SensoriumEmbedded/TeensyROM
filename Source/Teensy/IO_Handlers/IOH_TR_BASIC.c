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
extern uint32_t RxQueueHead, RxQueueTail;

enum TR_BASregsMatching  //synch with TRCustomBasicCommands\source\main.asm
{
   // registers:
   TR_BASDataReg         = 0xb4,   // (Write only)  
   TR_BASContReg         = 0xba,   // (Write only) Control Reg 

   // Control Reg Commands


}; //end enum synch

//__________________________________________________________________________________




//__________________________________________________________________________________

void InitHndlr_TR_BASIC()
{
   if (TgetQueue == NULL) TgetQueue = (uint8_t*)malloc(TgetQueueSize);
   
   RxQueueHead = RxQueueTail = 0; //as used in Swiftlink & ASID

   
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
      
      //   case rRegStreamData:
      //      DataPortWriteWait(XferImage[StreamOffsetAddr]);
      //      //inc on read, check for end:
      //      if (++StreamOffsetAddr >= XferSize) IO1[rRegStrAvailable]=0; //signal end of transfer
      //      break;
      //   default: //used for all other IO1 reads
      //      //DataPortWriteWaitLog(0); 
      //      break;
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
      }
   } //write
}

void PollingHndlr_TR_BASIC()
{
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
         Printf_dbg("H#%d= %d (%c)\n", RxQueueHead, Cin, Cin);
         TgetQueue[RxQueueHead++] = Cin;
         if (RxQueueHead == TgetQueueSize) RxQueueHead = 0;
      }
      
   }
}
   

