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


// Swiftlink Rx Queue Functions


uint8_t PullFromRxQueue()
{  //assumes queue data is available before calling
   uint8_t c = RxQueue[RxQueueTail/RxQueueBlockSize][RxQueueTail%RxQueueBlockSize];
   RxQueueTail++;  
   if (RxQueueTail == RxQueueSize) RxQueueTail = 0;
   //Printf_dbg("Pull H=%d T=%d Char=%c\n", RxQueueHead, RxQueueTail, c);
   return c;
}

bool ReadyToSendRx()
{
   //  if IRQ enabled, 
   //  and IRQ not set, 
   //  and enough time has passed
   //  then C64 is ready to receive...
   return ((SwiftRegCommand & SwiftCmndRxIRQEn) == 0 && \
      (SwiftRegStatus & (SwiftStatusRxFull | SwiftStatusIRQ)) == 0 && \
      CycleCountdown == 0);
}

bool CheckRxNMITimeout()
{
   //Check for Rx NMI timeout: Doesn't happen unless a lot of serial printing enabled (ie DbgMsgs_IO) causing missed reg reads
   if ((SwiftRegStatus & SwiftStatusIRQ)  && (micros() - NMIassertMicros > NMITimeoutuS))
   {
     Serial.println("Rx NMI Timeout!");
     SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     SetNMIDeassert;
     return false;
   }
   return true;
}

void SendRxByte(uint8_t ToSend) 
{
   //assumes ReadyToSendRx() is true before calling
   
   SwiftRxBuf = ToSend;
   SwiftRegStatus |= SwiftStatusRxFull | SwiftStatusIRQ;
   SetNMIAssert;
   NMIassertMicros = micros();
}

void CheckSendRxQueue()
{  
   //  if queued Rx data available to send to C64, and C64 is ready, then read/send 1 character to C64...
   if (RxQueueUsed > 0 && ReadyToSendRx())
   {
      uint8_t ToSend = PullFromRxQueue();
      //Printf_dbg("RxBuf=%02x: %c\n", ToSend, ToSend); //not recommended
      
      if (BrowserMode)
      {  //browser data is stored in ASCII to preserve tag info, convert rest to PETSCII before sending
         if(ToSend == '<') 
         {
            ParseHTMLTag();
            ToSend = 0;
         }
         if(ToSend == '&') 
         {
            ParseEntityReference();
            ToSend = 0;
         }
         else 
         {
            if(ToSend == 13 || ToSend == 10) ToSend = 0; //ignore return char & New Line
            else ToSend = ToPETSCII(ToSend);
         }
         
         //if zero, skip sending to save a full c64 char cycle
         //ok during BrowserMode, but skipping 0s in regular mode kills downloads (no zeros sent)
         if(!ToSend) return;
         PageCharsReceived++; //normal char, not return or special PETSCII
      } //BrowserMode
      
      SendRxByte(ToSend);
   }
   
   CheckRxNMITimeout();
}

void FlushRxQueue()
{
   while (RxQueueUsed) CheckSendRxQueue();  
}

void AddRawCharToRxQueue(uint8_t c)
{
  if (RxQueueUsed >= RxQueueSize-1)
  {
     Printf_dbg("RxOvf! ");
     //RxQueueHead = RxQueueTail = 0;
     ////just in case...
     //SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
     //SetNMIDeassert;
     return;
  }
  
  RxQueue[RxQueueHead/RxQueueBlockSize][RxQueueHead%RxQueueBlockSize] = c;
  RxQueueHead++; 
  if (RxQueueHead == RxQueueSize) RxQueueHead = 0;
}

void AddRawStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   
   while(s[CharNum] != 0) AddRawCharToRxQueue(s[CharNum++]);
}

void AddToPETSCIIStrToRxQueue(const char* s)
{
   uint8_t CharNum = 0;
   
   //Printf_dbg("AStrToRx(Len=%d): %s\n", strlen(s), s);
   while(s[CharNum] != 0) AddRawCharToRxQueue(ToPETSCII(s[CharNum++]));
}

void AddToPETSCIIStrToRxQueueLN(const char* s)
{
   AddToPETSCIIStrToRxQueue(s);
   AddToPETSCIIStrToRxQueue("\r");
}

FLASHMEM void AddIPaddrToRxQueueLN(IPAddress ip)
{
   char Buf[50];
   sprintf(Buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
   AddToPETSCIIStrToRxQueueLN(Buf);
}

FLASHMEM void AddMACToRxQueueLN(uint8_t* mac)
{
   char Buf[50];
   sprintf(Buf, " MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   AddToPETSCIIStrToRxQueueLN(Buf);
}

FLASHMEM void AddInvalidFormatToRxQueueLN()
{
   AddToPETSCIIStrToRxQueueLN("Invalid Format");
}

FLASHMEM void AddUpdatedToRxQueueLN()
{
   AddToPETSCIIStrToRxQueueLN("Updated");
}

FLASHMEM void AddDHCPEnDisToRxQueueLN()
{
   AddToPETSCIIStrToRxQueue(" DHCP: ");
   if (EEPROM.read(eepAdDHCPEnabled)) AddToPETSCIIStrToRxQueueLN("Enabled");
   else AddToPETSCIIStrToRxQueueLN("Disabled");
}
  
FLASHMEM void AddDHCPTimeoutToRxQueueLN()
{
   uint16_t invalU16;
   char buf[50];
   EEPROM.get(eepAdDHCPTimeout, invalU16);
   sprintf(buf, " DHCP Timeout: %dmS", invalU16);
   AddToPETSCIIStrToRxQueueLN(buf);
}
  
FLASHMEM void AddDHCPRespTOToRxQueueLN()
{
   uint16_t invalU16;
   char buf[50];
   EEPROM.get(eepAdDHCPRespTO, invalU16);
   sprintf(buf, " DHCP Response Timeout: %dmS", invalU16);
   AddToPETSCIIStrToRxQueueLN(buf);
} 

FLASHMEM void Add_BR_ToRxQueue()
{
   AddRawStrToRxQueue("<br>");
}


//From Dns.cpp of ethernet lib, didn't want the baggage
//  Arduino DNS client for WizNet5100-based Ethernet shield
//  (c) Copyright 2009-2010 MCQN Ltd.
//  Released under Apache License, version 2.0
bool inet_aton(const char* address, IPAddress& result)
{
	uint16_t acc = 0; // Accumulator
	uint8_t dots = 0;

	while (*address) 
   {
		char c = *address++;
		if (c >= '0' && c <= '9') 
      {
			acc = acc * 10 + (c - '0');
			if (acc > 255) return false; // Value out of [0..255] range
		} 
      else if (c == '.') 
      {
			if (dots == 3) return false; // Too mmany dots (must be 3)
			result[dots++] = acc;
			acc = 0;
		} 
      else return false; // Invalid char
	}

	if (dots != 3) return false; // Too few dots (must be 3)
	result[3] = acc;
	return true;
}


FLASHMEM void StrToIPToEE(char* Arg, uint8_t EEPaddress)
{  // Arg is an IP address string, decode it and write it to EEPROM at EEPaddress
   IPAddress ip;   
   
   AddToPETSCIIStrToRxQueueLN(" IP Addr");
   if (!inet_aton(Arg, ip)) 
   {
      AddInvalidFormatToRxQueueLN();
      return;
   }
   
   EEPROM.put(EEPaddress, (uint32_t)ip);
   AddUpdatedToRxQueueLN();
   AddToPETSCIIStrToRxQueue("to ");
   AddIPaddrToRxQueueLN(ip);
}

