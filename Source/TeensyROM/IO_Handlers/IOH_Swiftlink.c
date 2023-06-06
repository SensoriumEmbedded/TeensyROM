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


//Network, 6551 ACIA interface emulation

void IO1Hndlr_SwiftLink(uint8_t Address, bool R_Wn);  
void PollingHndlr_SwiftLink();                           
void InitHndlr_SwiftLink();                           
void CycleHndlr_SwiftLink();                           

stcIOHandlers IOHndlr_SwiftLink =
{
  "SwiftLink",              //Name of handler
  &InitHndlr_SwiftLink,     //Called once at handler startup
  &IO1Hndlr_SwiftLink,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_SwiftLink,  //Polled in main routine
  &CycleHndlr_SwiftLink,    //called at the end of EVERY c64 cycle
};

extern bool EthernetInit();
extern uint32_t CycleCountdown;

uint8_t* RxQueue;  //circular queue to pipe data to the c64 
uint8_t* TxMsg;  //to hold messages (AT commands) when off line
uint16_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost = false;
uint32_t NMIassertMicros = 0;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;

#define TxMsgSize          128
#define RxQueueSize       8192 
#define C64CycBetweenRx   2300   //stops NMI from re-asserting too quickly. chars missed in large buffs when lower
#define NMITimeoutnS       300   //if Rx data not read within this time, deassert NMI anyway

// 6551 ACIA interface emulation
//register locations (IO1, DExx)
#define IORegSwiftData    0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus  0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand 0x02   // Swift Emulation Command Reg
#define IORegSwiftControl 0x03   // Swift Emulation Control Reg

//status reg flags
#define SwiftStatusIRQ     0x80   // high if ACIA caused interrupt;
#define SwiftStatusDSR     0x40   // reflects state of DSR line
#define SwiftStatusDCD     0x20   // reflects state of DCD line
#define SwiftStatusTxEmpty 0x10   // high if xmit-data register is empty
#define SwiftStatusRxFull  0x08   // high if receive-data register full
#define SwiftStatusErrOver 0x04   // high if overrun error
#define SwiftStatusErrFram 0x02   // high if framing error
#define SwiftStatusErrPar  0x01   // high if parity error

//command reg flags
#define SwiftCmndRxIRQEn   0x02   // low if Rx IRQ enabled

#define RxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+RxQueueSize-RxQueueTail))

uint8_t PullFromRxQueue()
{
  uint8_t c = RxQueue[RxQueueTail++]; 
  if (RxQueueTail == RxQueueSize) RxQueueTail = 0;
  //Printf_dbg("Pull H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
  return c;
}

void AddCharToRxQueue(uint8_t c)
{
  if (RxQueueUsed >= RxQueueSize-1)
  {
     Serial.println("RxBuff Overflow!");
     RxQueueHead = RxQueueTail = 0;
     //just in case...
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
     return;
  }
  RxQueue[RxQueueHead++] = c; 
  if (RxQueueHead == RxQueueSize) RxQueueHead = 0;
  //Printf_dbg("Push H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
}

void AddASCIIStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   //Printf_dbg("StrToRx(Len=%d): %s\n", strlen(s), s);
   while(s[CharNum] != 0)
   {
      AddCharToRxQueue(ToPETSCII(s[CharNum]));
      CharNum++; //putting this inside the above statment breaks it due to petscii macro multiple references
   }  
}

void AddNumToRxQueue(uint8_t Num)
{
   char buf[6];
   sprintf(buf, "%d", Num);
   AddASCIIStrToRxQueue(buf);
}

void ProcessATCommand(char* CmdMsg)
{
   if (CmdMsg[0] != 'A' || CmdMsg[1] != 'T')
   {
      AddASCIIStrToRxQueue("AT not found");
      return;
   }
   
   CmdMsg+=2; //past the AT
   
   if (CmdMsg[0] == 0) return;              //AT: ping
   
   if (CmdMsg[0] == 'C' && CmdMsg[1] == 0)  //ATC: Connect Ethernet
   {
         if(EthernetInit())
         {
            AddASCIIStrToRxQueue("Connected to Ethernet");
            //uint8_t *ip = (uint8_t*)&current_options.ip_address.s_addr;
            uint32_t ip32 = Ethernet.localIP();
            uint8_t *ip = (uint8_t*)&ip32;
            AddASCIIStrToRxQueue("\rIP Address: ");
            AddNumToRxQueue((uint8_t)*ip++);
            AddCharToRxQueue('.');
            AddNumToRxQueue((uint8_t)*ip++);
            AddCharToRxQueue('.');
            AddNumToRxQueue((uint8_t)*ip++);
            AddCharToRxQueue('.');
            AddNumToRxQueue((uint8_t)*ip);
            return;
         }
         else
         {
            AddASCIIStrToRxQueue("Ethernet connect failed!");
            if (Ethernet.hardwareStatus() == EthernetNoHardware) AddASCIIStrToRxQueue("\r HW was not found");
            else if (Ethernet.linkStatus() == LinkOFF) AddASCIIStrToRxQueue("\r Cable is not connected");
            return;
         }
   }
   
   if (CmdMsg[0] == 'D' && CmdMsg[1] == 'T') //ATDT<HostName>:<Port>   Connect telnet
   {   
      CmdMsg+=2; //past the DT
   
      uint16_t  Port = 6400; //default if not defined
      char* Delim = strstr(CmdMsg, ":");
      if (Delim != NULL) //port defined, read it
      {
         Delim[0]=0; //terminate host name
         Port = atoi(Delim+1);
         //if (Port==0) AddASCIIStrToRxQueue("Invalid Port #");
      }
      
      if (client.connect(CmdMsg, Port)) AddASCIIStrToRxQueue("Connected to Host");
      else AddASCIIStrToRxQueue("Host connect Failed");
      Printf_dbg("Host name: %s  Port: %d\n", CmdMsg, Port);
   
      return;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddASCIIStrToRxQueue("Unknown Command");
}


//__________________________________________________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = 0;
   SwiftRegControl = 0;
   CycleCountdown=0;
   
   RxQueueHead = RxQueueTail = TxMsgOffset =0;
   free(RxQueue);
   RxQueue = (uint8_t*)malloc(RxQueueSize);
   free(TxMsg);
   TxMsg = (uint8_t*)malloc(TxMsgSize);
}   


void IO1Hndlr_SwiftLink(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      switch(Address)
      {
         case IORegSwiftData:   
            DataPortWriteWaitLog(SwiftRxBuf);
            CycleCountdown = C64CycBetweenRx;
            SetNMIDeassert;
            SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
            break;
         case IORegSwiftStatus:  
            DataPortWriteWaitLog(SwiftRegStatus);
            break;
         case IORegSwiftCommand:  
            DataPortWriteWaitLog(SwiftRegCommand);
            break;
         case IORegSwiftControl:
            DataPortWriteWaitLog(SwiftRegControl);
            break;
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      Data = DataPortWaitRead();
      switch(Address)
      {
         case IORegSwiftData:  
            //add to input buffer
            SwiftTxBuf=Data;
            SwiftRegStatus &= ~SwiftStatusTxEmpty; //Flag full until Tx processed
            break;
         case IORegSwiftStatus:  
            //Write to status reg is a programmed reset
            //not doing anything for now
            break;
         case IORegSwiftControl:
            SwiftRegControl = Data;
            //Could confirm setting 8N1 & acceptable baud?
            break;
         case IORegSwiftCommand:  
            SwiftRegCommand = Data;
            //check for Tx/Rx IRQs enabled?
            //handshake line updates?
            break;
      }
      TraceLogAddValidData(Data);
   }
}

void PollingHndlr_SwiftLink()
{
   if (ConnectedToHost != client.connected())
   {
      ConnectedToHost = client.connected();
      AddASCIIStrToRxQueue("\r\r\r*** ");
      if (ConnectedToHost) AddASCIIStrToRxQueue("Connected to host\r");
      else AddASCIIStrToRxQueue("Not connected\r");
   }
   
   //if client data available, add to Rx Queue
   #ifdef DbgMsgs_IO
      if(client.available())
      {
         uint16_t Cnt = 0;
         //Serial.printf("RxIn %d+", RxQueueUsed);
         while (client.available())
         {
            uint8_t c=client.read();
            AddCharToRxQueue(c);
            Cnt++;
         }
         //Serial.printf("%d=%d\n", Cnt, RxQueueUsed);
         if (RxQueueUsed>3000) Serial.printf("RxQueue added: %d  total: %d\n", Cnt, RxQueueUsed);
      }
   #else
      while (client.available()) AddCharToRxQueue(client.read());
   #endif
   
   if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) //Tx data available from C64
   {
      if (client.connected()) //send Tx data to host
      {
         //Printf_dbg("send %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         client.print((char)SwiftTxBuf);
      }
      else  //off-line, at commands, etc..................................
      {
         //Printf_dbg("echo %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         AddCharToRxQueue(SwiftTxBuf); //echo it
         
         TxMsg[TxMsgOffset++] = SwiftTxBuf;
         if (SwiftTxBuf == 13 || TxMsgOffset == TxMsgSize) //return hit or max size
         {
            TxMsg[TxMsgOffset-1] = 0; //terminate it
            //Printf_dbg("TxMsg: %s\n", TxMsg);
            ProcessATCommand((char*)TxMsg);
            AddASCIIStrToRxQueue("\rok\r");
            TxMsgOffset = 0;
         }
      }
      SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
   }
   
   //  if Rx data available to send to C64, IRQ enabled, and ready (not set), 
   //  and enough time has passed, then read/send to C64...
   if (RxQueueUsed > 0 && \
      (SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && \
      (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0 && \
      CycleCountdown == 0)
   {
      SwiftRxBuf = PullFromRxQueue();
      //Printf_dbg("RxBuf=%02x: %c\n", SwiftRxBuf, SwiftRxBuf);
      SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
      SetNMIAssert;
      NMIassertMicros = micros();
   }
      
   //Rx NMI timeout: Isn't needed unless a lot of printing enabled (ie DbgMsgs_IO) causing missed reg reads
   if ((SwiftRegStatus & SwiftStatusIRQ)  && (micros() - NMIassertMicros > NMITimeoutnS))
   {
     Serial.println("Rx NMI Timeout!");
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
   }
   

}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}

