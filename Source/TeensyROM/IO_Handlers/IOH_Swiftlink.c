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

stcIOHandlers IOHndlr_SwiftLink =
{
  "SwiftLink",              //Name of handler
  &InitHndlr_SwiftLink,     //Called once at handler startup
  &IO1Hndlr_SwiftLink,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_SwiftLink,  //Polled in main routine
  NULL,                     //called at the end of EVERY c64 cycle
};

extern bool EthernetInit();

uint8_t* RxQueue;  //circular queue to pipe data to the c64 
uint8_t* TxMsg;  //to hold messages (AT commands) when off line
uint16_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost = false;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
volatile uint32_t SwiftLastRxMicros = 0;

#define RxQueueSize        2048
#define TxMsgSize           128
#define MinMicrosBetweenRx  100 //stops NMI from re-asserting too quickly. CCGMS fails below 70uS (~14khz, 114 baud);   for 38400 baud, just need to be below 208: 1/38400*8=208.3

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

void AddCharToRxQueue(uint8_t c)
{
  if (RxQueueUsed >= RxQueueSize-1)
  {
     Serial.println("RxBuff full!");
     return;
  }
  RxQueue[RxQueueHead++] = c; 
  if (RxQueueHead == RxQueueSize) RxQueueHead = 0;
}

void AddStringToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   while(s[CharNum] != 0) AddCharToRxQueue((char)ToPETSCII(s[CharNum++]));
}

void AddNumToRxQueue(uint8_t Num)
{
   char buf[6];
   sprintf(buf, "%d", Num);
   AddStringToRxQueue(buf);
}

void ProcessATCommand(char* CmdMsg)
{
   if (CmdMsg[0] != 'A' || CmdMsg[1] != 'T')
   {
      AddStringToRxQueue("AT not found");
      return;
   }
   
   CmdMsg+=2; //past the AT
   
   if (CmdMsg[0] == 0) return;              //AT: ping
   
   if (CmdMsg[0] == 'C' && CmdMsg[1] == 0)  //ATC: Connect Ethernet
   {
         if(EthernetInit())
         {
            AddStringToRxQueue("Connected to Ethernet");
            //uint8_t *ip = (uint8_t*)&current_options.ip_address.s_addr;
            uint32_t ip32 = Ethernet.localIP();
            uint8_t *ip = (uint8_t*)&ip32;
            AddStringToRxQueue("\rIPAddress: ");
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
            AddStringToRxQueue("Ethernet connect failed!");
            if (Ethernet.hardwareStatus() == EthernetNoHardware) AddStringToRxQueue("\r HW was not found");
            else if (Ethernet.linkStatus() == LinkOFF) AddStringToRxQueue("\r Cable is not connected");
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
         Delim++;
         Port = 0;
         while (*Delim >='0' && *Delim <='9')
         {
            Port*=10;
            Port += *Delim-'0';
            Delim++;
         }
         //Port = strtol((Delim+1), (char **)NULL, 10); //takes ~32k of ram?
         //Port = atoi(Delim+1);  //takes ~32k of ram?
         //sscanf((), "%d", &Port); //takes ~60k of ram???
         if (Port==0) AddStringToRxQueue("Invalid Port #");
      }
   
      if (client.connect(CmdMsg, Port)) AddStringToRxQueue("Connected to Host");
      else AddStringToRxQueue("Host connect Failed");
      Printf_dbg("Host name: %s  Port: %d\n", CmdMsg, Port);
   
      return;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddStringToRxQueue("Unknown Command");
}

uint8_t PullFromRxQueue()
{
  uint8_t c = RxQueue[RxQueueTail++]; 
  if (RxQueueTail == RxQueueSize) RxQueueTail = 0;
  return c;
}


//__________________________________________________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = 0;
   SwiftRegControl = 0;
   
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
            SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
            SetNMIDeassert;
            SwiftLastRxMicros = micros(); //mark time of last receive
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
      AddStringToRxQueue("\r\r\r*** ");
      if (ConnectedToHost) AddStringToRxQueue("Connected to host\r");
      else AddStringToRxQueue("Not connected\r");
   }
   
   if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) //Tx data available from C64
   {
      if (client.connected()) //send Tx data to host
      {
         Printf_dbg("send %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
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
            ProcessATCommand((char*)TxMsg);
            AddStringToRxQueue("\rok\r");
            TxMsgOffset = 0;
         }
      }
      SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
   }
   
   //if client data available, add to Rx Queue
   //change to if?   qualify with connected????????????????????????
   while (client.available()) AddCharToRxQueue(client.read());

   //  if Rx data available to send to C64, IRQ enabled, and ready (not set), 
   //  and enough time has passed, then read/send to C64...
   if (RxQueueUsed > 0 && \
      (SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && \
      (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0 && \
      (micros() - SwiftLastRxMicros) > MinMicrosBetweenRx)
   {
      SwiftRxBuf = PullFromRxQueue();
      SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
      SetNMIAssert;
   }
      
}

