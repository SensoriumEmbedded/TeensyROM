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
  "SwiftLink",     //Name of handler
  &InitHndlr_SwiftLink,   //Called once at handler startup
  &IO1Hndlr_SwiftLink,    //IO1 R/W handler
  NULL,    //IO2 R/W handler
  NULL,   //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_SwiftLink,                     //Polled in main routine
  NULL,  //called at the end of EVERY c64 cycle
};

extern bool EthernetInit();

volatile uint8_t SwiftTxBuf, SwiftRxBuf = 0;
volatile uint8_t SwiftRegStatus, SwiftRegCommand, SwiftRegControl;
volatile uint32_t SwiftLastRxMicros = 0;

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

void SwiftConnectToHost(uint8_t HostNum)
{
   struct stcHostInfo
   {
     char Name[25];
     uint16_t  Port;
   };
   
   stcHostInfo Hosts[] =
   {
      "13th.hoyvision.com", 6400,
      "oasisbbs.hopto.org", 6400,
   };
   
   if(HostNum >= sizeof(Hosts)/sizeof(Hosts[0]))
   {
      Serial.printf("Host %d out of bounds\n", HostNum);
      return;
   }
   
   Serial.printf("Connecting to #%02d- %s:%d ...", HostNum, Hosts[HostNum].Name, Hosts[HostNum].Port);
   if (client.connect(Hosts[HostNum].Name, Hosts[HostNum].Port)) Serial.println("Done");
   else Serial.println("Failed!");
}

//__________________________________________________________________________________________

void InitHndlr_SwiftLink()
{
   EthernetInit();
   //if(!EthernetInit())  //could let this go and check later?
   SwiftRegStatus = SwiftStatusTxEmpty; //default reset state
   SwiftRegCommand = 0;
   SwiftRegControl = 0;
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

#define MinMicrosBetweenRx 417 //26  //417

void PollingHndlr_SwiftLink()
{
   if (client.connected()) 
   {
      //if Tx data available to send to host, then read/send to host
      if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) //Tx byte ready
      {
         Printf_dbg("sending %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         client.print((char)SwiftTxBuf);
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
      }
      
      //if Rx data available to send to C64, IRQ enabled, and ready, then read/send to C64
      if (client.available()) 
      {
         if((SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0) //IRQ both enabled and not already set
         {
            uint32_t MicrosSinceLastRx = micros() - SwiftLastRxMicros;
            if (MicrosSinceLastRx > MinMicrosBetweenRx)
            {
               SwiftRxBuf = client.read();
               //Printf_dbg("Rx %02x: %c @ %luuS\n", SwiftRxBuf, SwiftRxBuf, MicrosSinceLastRx);
               SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
               SetNMIAssert;
               //SwiftLastRxMicros = micros();
            }
         }
      }
   }
   
   else  //off-line, at commands, etc..................................
   {
      if ((SwiftRegStatus & SwiftStatusTxEmpty) == 0) //Tx byte ready
      {
         Printf_dbg("echo %02x: %c\n", SwiftTxBuf, SwiftTxBuf);
         SwiftRegStatus |= SwiftStatusTxEmpty; //Ready for more
         
         if((SwiftRegCommand & SwiftCmndRxIRQEn) == 0)  //IRQ both enabled... 
         {
            if((SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0) //...and not already set
            {
               SwiftRxBuf = SwiftTxBuf;
               SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
               SetNMIAssert;
            }
            //else Printf_dbg("IRQ already set\n");
         }
         //else Printf_dbg("IRQ is off\n");
        
         
      }
      
   }
}

