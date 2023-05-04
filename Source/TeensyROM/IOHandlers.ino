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


//IO1 Handler for TeensyROM _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_TeensyROM(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case rRegItemType:
            DataPortWriteWait(MenuSource[IO1[rwRegSelItem]].ItemType);  
            break;
         case rRegItemNameStart ... (rRegItemNameStart+MaxItemNameLength-1):
            Data = MenuSource[IO1[rwRegSelItem]].Name[Address-rRegItemNameStart];
            //Convert to PETscii, make this a table? Seems fast enough
            if (Data==95) Data=32; //underscore->space
            else if (Data>64) Data ^=32; 
            DataPortWriteWait(Data);  
            break;
         case rRegStreamData:
            DataPortWriteWait(MenuSource[IO1[rwRegSelItem]].Code_Image[StreamOffsetAddr]);
            //inc on read, check for end:
            if (++StreamOffsetAddr >= MenuSource[IO1[rwRegSelItem]].Size) IO1[rRegStrAvailable]=0; //signal end of transfer
            break;
         default: //used for all other IO1 reads
            DataPortWriteWait(IO1[Address]); 
            break;
      }
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead(); 
      switch(Address)
      {
         case rwRegSelItem:
            IO1[rwRegSelItem]=Data;
            break;
         case rwRegNextIO1Hndlr:
            if (Data < IO1H_Num_Handlers) IO1[rwRegNextIO1Hndlr]=Data;
            else IO1[rwRegNextIO1Hndlr]=0;
            break;
         case rWRegCurrMenuWAIT:
            IO1[rWRegCurrMenuWAIT]=Data;
            IO1[rRegStatus] = rsChangeMenu; //work this in the main code
            break;
         case rwRegPwrUpDefaults:
            IO1[rwRegPwrUpDefaults]= Data;
            EEPROM.write(eepAdPwrUpDefaults, Data); 
            break;
         case rwRegTimezone:
            IO1[rwRegTimezone]= Data;
            EEPROM.write(eepAdrwRegTimezone, Data); 
            break;
         case wRegControl:
            switch(Data)
            {
               case rCtlVanishROM:
                  SetGameDeassert;
                  SetExROMDeassert;      
                  LOROM_Image = NULL;
                  HIROM_Image = NULL;  
                  break;
               case rCtlBasicReset:  
                  //SetLEDOff;
                  doReset=true;
                  IO1[rRegStatus] = rsIO1HWinit; //Support IO handlers at reset
                  break;
               case rCtlStartSelItemWAIT:
                  IO1[rRegStatus] = rsStartItem; //work this in the main code
                  break;
               case rCtlGetTimeWAIT:
                  IO1[rRegStatus] = rsGetTime;   //work this in the main code
                  break;
               case rCtlRunningPRG:
                  IO1[rRegStatus] = rsIO1HWinit; //Support IO handlers in PRG
                  break;
            }
            break;
      }
   } //write
}

//IO1 Handler for MIDI Emulation _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_MIDI(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case rIORegAddrMIDIStatus:
            DataPortWriteWait(rIORegMIDIStatus);  
            break;
         case rIORegAddrMIDIReceive:
            if(MIDIRxBytesToSend)
            {
               DataPortWriteWait(MIDIRxBuf[--MIDIRxBytesToSend]);  
               if (MIDIRxBytesToSend == 0)
               {
                  rIORegMIDIStatus = 0;
                  SetIRQDeassert;
               }
            }
            break;
         //default:
            //break;
      }
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead(); 
      switch(Address)
      {
         case wIORegAddrMIDIControl:
            if (Data == 0x03) //Master Reset
            {
               rIORegMIDIStatus   = 0;
               MIDIRxBytesToSend  = 0;
               SetIRQDeassert;
            }
            else if (Data & 0x80) //Receive Interrupt Enable set
            {
               rIORegMIDIStatus |= 4; //Bit 2 (Data Carrier Detect)
            }               
            break;
         case wIORegAddrMIDITransmit:
            if (MIDITxBytesReceived != 0 || ((Data & 0x80) == 0x80 && (Data & 0xf0) != 0xf0)) //skip system & invalid headers
            {
               if (MIDITxBytesReceived < 3)
               {
                  MIDITxBuf[MIDITxBytesReceived++] = Data;
                  if(MIDITxBytesReceived == 2 && (MIDITxBuf[0] & 0xf0 == 0xc0 || MIDITxBuf[0] & 0xf0 == 0xd0))
                  {
                     MIDITxBuf[2] = 0;
                     MIDITxBytesReceived = 3;
                  }
               }
               //else Serial.print("Miss!");//replace with LED on
            }
            //Serial.printf("%d %02x\n", MIDITxBytesReceived, Data);
            
            break;
         //default:
            //break;
     }
   }
}

//IO1 Handler for Debug _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_Debug(uint8_t Address, bool R_Wn)
{
   if (R_Wn) //High (IO1 Read)
   {
      //Serial.printf("Rd $de%02x\n", Address);
   }
   else  // IO1 write
   {
      // Every write cycle is preceded by a read cycle, but the Serial.print on rd (here in the ISR) 
      // keeps it busy during the actual write.  Need to put this in a queue and print from outside or something.
      // So, for now, comment out the Serial Print in Rd above to see writes, or leave it uncommented to see reads
      uint8_t Data = DataPortWaitRead(); 
      Serial.printf("wr $de%02x:$%02x\n", Address, Data);
   }
}


//IO1 Handler Init _________________________________________________________________________________________

void IO1HWinit()
{
   IO1Handler = IO1[rwRegNextIO1Hndlr];
   
   rIORegMIDIStatus = 0;
   MIDIRxBytesToSend = 0;
   switch(IO1Handler)
   {
      case IO1H_TeensyROM:
         //MIDI handlers remain for MIDI2SID
         Serial.println("TeensyROM IO1 handler ready");
         break;
      case IO1H_MIDI:
         for (uint8_t ContNum=0; ContNum < NumMIDIControls;) MIDIControlVals[ContNum++]=63;
         midi1.setHandleNoteOff      (HWEOnNoteOff);
         midi1.setHandleNoteOn       (HWEOnNoteOn);
         midi1.setHandleControlChange(HWEOnControlChange);
         midi1.setHandlePitchChange  (HWEOnPitchChange);
         Serial.println("MIDI IO1 handler ready");
         break;
      case IO1H_Debug:
         midi1.setHandleNoteOff      (DbgOnNoteOff);
         midi1.setHandleNoteOn       (DbgOnNoteOn);
         midi1.setHandleControlChange(DbgOnControlChange);
         midi1.setHandlePitchChange  (DbgOnPitchChange);
         Serial.println("Debug IO1 handler ready");
         break;
   }

}