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
  "SwiftLink/Modem",        //Name of handler
  &InitHndlr_SwiftLink,     //Called once at handler startup
  &IO1Hndlr_SwiftLink,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_SwiftLink,  //Polled in main routine
  &CycleHndlr_SwiftLink,    //called at the end of EVERY c64 cycle
};

extern volatile uint32_t CycleCountdown;

uint8_t* RxQueue;  //circular queue to pipe data to the c64 
char* TxMsg;  //to hold messages (AT commands) when off line
uint16_t  RxQueueHead, RxQueueTail, TxMsgOffset;
bool ConnectedToHost = false;
uint32_t NMIassertMicros = 0;
volatile uint8_t SwiftTxBuf, SwiftRxBuf;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

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
#define SwiftCmndDefault   0xE0   // Default command reg state

#define RxQueueUsed ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+RxQueueSize-RxQueueTail))

bool EthernetInit()
{
   uint32_t beginWait = millis();

   Serial.print("\nEthernet init... ");
   if (Ethernet.begin(mac, 9000, 4000) == 0)  //reduce timeout from 60 to 9 sec, should be longer, or option to skip
   {
      Serial.printf("***Failed!*** took %d mS\n", (millis() - beginWait));
      // Check for Ethernet hardware present
      if (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("Ethernet HW was not found.");
      else if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");
      
      IO1[rRegLastSecBCD]  = 0;      
      IO1[rRegLastMinBCD]  = 0;      
      IO1[rRegLastHourBCD] = 0;      
      return false;
   }
   Serial.printf("passed. took %d mS\nIP: ", (millis() - beginWait));
   Serial.println(Ethernet.localIP());
   return true;
}
   
uint8_t PullFromRxQueue()
{
  uint8_t c = RxQueue[RxQueueTail++]; 
  if (RxQueueTail == RxQueueSize) RxQueueTail = 0;
  //Printf_dbg("Pull H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
  return c;
}

void CheckSendRx()
{
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

void FlushRxQueue()
{
   while (RxQueueUsed) CheckSendRx();  
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

//_____________________________________AT Commands_____________________________________________________

#define MaxATcmdLength   20

struct stcATCommand
{
  char Command[MaxATcmdLength];
  void (*Function)(char*); 
};

void AT_DT(char* CmdArg)
{  //ATDT<HostName>:<Port>   Connect telnet
   uint16_t  Port = 6400; //default if not defined
   char* Delim = strstr(CmdArg, ":");


   if (Delim != NULL) //port defined, read it
   {
      Delim[0]=0; //terminate host name
      Port = atoi(Delim+1);
      //if (Port==0) AddASCIIStrToRxQueue("invalid port #");
   }
   
   char Buf[100];
   sprintf(Buf, "trying %s\r\non port %d...\r\n", CmdArg, Port);
   AddASCIIStrToRxQueue(Buf);
   FlushRxQueue();
   //Printf_dbg("Host name: %s  Port: %d\n", CmdArg, Port);
   
   if (client.connect(CmdArg, Port)) AddASCIIStrToRxQueue("done");
   else AddASCIIStrToRxQueue("failed!");
}

void AT_C(char* CmdArg)
{  //ATC: Connect Ethernet
   AddASCIIStrToRxQueue("connecting to ethernet...");
   FlushRxQueue();
   
   if (EthernetInit()==true)
   {
      AddASCIIStrToRxQueue("done");
      
      uint32_t ip32 = Ethernet.localIP();
      uint8_t *ip = (uint8_t*)&ip32;
      char Buf[100];
      sprintf(Buf, "\r\nIP Address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      AddASCIIStrToRxQueue(Buf);
      sprintf(Buf, "\r\nMAC Address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      AddASCIIStrToRxQueue(Buf);
   }
   else
   {
      AddASCIIStrToRxQueue("failed!");
      if (Ethernet.hardwareStatus() == EthernetNoHardware) AddASCIIStrToRxQueue("\r\n hw was not found");
      else if (Ethernet.linkStatus() == LinkOFF) AddASCIIStrToRxQueue("\r\n cable is not connected");
   }
}

stcATCommand ATCommands[] =
{
   "dt"                    ,&AT_DT,
   "c"                    ,&AT_C,
};

void ProcessATCommand()
{
   char* CmdMsg = TxMsg; //local copy for manipulation
   uint16_t Num=0;
   
   while (CmdMsg[Num])
   {  //conv to lower case ASCII
      CmdMsg[Num] &= 127;
      if(CmdMsg[Num] >= 'A') CmdMsg[Num] ^= 32;
      Num++;
   }
   Printf_dbg("AT Msg recvd: %s\n", CmdMsg);
   
   if (strstr(CmdMsg, "at")!=CmdMsg)
   {
      AddASCIIStrToRxQueue("at not found");
      return;
   }
   CmdMsg+=2; //past the AT
   if(CmdMsg[0]==0) return;  //ping
   
   Num=0;
   while(Num < sizeof(ATCommands)/sizeof(ATCommands[0]))
   {
      if (strstr(CmdMsg, ATCommands[Num].Command) == CmdMsg)
      {
         CmdMsg+=strlen(ATCommands[Num].Command); //move past the Command
         ATCommands[Num].Function(CmdMsg);
         return;
      }
      Num++;
   }
   
   Printf_dbg("Unk Msg: %s CmdMsg: %s\n", TxMsg, CmdMsg);
   AddASCIIStrToRxQueue("unknown command: ");
   AddASCIIStrToRxQueue(TxMsg);
}




//_____________________________________Handlers_____________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = SwiftCmndDefault;
   SwiftRegControl = 0;
   CycleCountdown=0;
   
   RxQueueHead = RxQueueTail = TxMsgOffset =0;
   free(RxQueue);
   RxQueue = (uint8_t*)malloc(RxQueueSize);
   free(TxMsg);
   TxMsg = (char*)malloc(TxMsgSize);
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
            SwiftRegCommand = SwiftCmndDefault;
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
      AddASCIIStrToRxQueue("\r\n\r\n\r\n*** ");
      if (ConnectedToHost) AddASCIIStrToRxQueue("connected to host\r\n");
      else AddASCIIStrToRxQueue("not connected\r\n");
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
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
      }
      else  //off-line, at commands, etc..................................
      {
         //Printf_dbg("echo %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         AddCharToRxQueue(SwiftTxBuf); //echo it
         
         TxMsg[TxMsgOffset++] = SwiftTxBuf;
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
         if (TxMsg[TxMsgOffset-1] == 13 || TxMsgOffset == TxMsgSize) //return hit or max size
         {
            TxMsg[TxMsgOffset-1] = 0; //terminate it
            //Printf_dbg("TxMsg: %s\n", TxMsg);
            ProcessATCommand();
            AddASCIIStrToRxQueue("\r\nok\r\n");
            TxMsgOffset = 0;
         }
      }
   }

   CheckSendRx();
}

void CycleHndlr_SwiftLink()
{
   if (CycleCountdown) CycleCountdown--;
}


