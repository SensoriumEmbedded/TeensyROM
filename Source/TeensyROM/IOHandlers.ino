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
               case rCtlVanish:
                  SetGameDeassert;
                  SetExROMDeassert;      
                  LOROM_Image = NULL;
                  HIROM_Image = NULL;  
                  //SetLEDOff;
                  break;
               case rCtlVanishReset:  
                  SetGameDeassert;
                  SetExROMDeassert;      
                  LOROM_Image = NULL;
                  HIROM_Image = NULL;  
                  SetLEDOff;
                  Phi2ISRState = P2I_Off;
                  doReset=true;
                  break;
               case rCtlStartSelItemWAIT:
                  IO1[rRegStatus] = rsStartItem; //work this in the main code
                  break;
               case rCtlGetTimeWAIT:
                  IO1[rRegStatus] = rsGetTime;   //work this in the main code
                  break;
               case rCtlRunningPRG:
                  IO1[rRegStatus] = rsIO1HWinit; //work this in the main code
                  break;
            }
            break;
      }
   } //write
}

__attribute__(( always_inline )) inline void IO1Hndlr_MIDI(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case rIORegAddrMIDIStatus:
            DataPortWriteWait(rIORegMIDIStatus);  
            //Serial.printf("St %02x", rIORegMIDIStatus);
            break;
         case rIORegAddrMIDIReceive:
            if(MIDIRxBytesToSend)
            {
               DataPortWriteWait(rIORegMIDIReceiveBuf[MIDIRxBytesToSend-1]);  
               if (--MIDIRxBytesToSend == 0)
               {
                  rIORegMIDIStatus   = 0;
                  SetIRQDeassert;
               }
            }
            //Serial.printf("Rx %d %02x", MIDIRxBytesToSend, rIORegMIDIReceiveBuf[MIDIRxBytesToSend]);
            break;
         default:
            //Serial.print("Unk");
            break;
      }
      //Serial.print(" r");
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead(); 
      switch(Address)
      {
         case wIORegAddrMIDIControl:
            if(Data == MIDIContReset)
            {
               rIORegMIDIStatus   = 0;
               MIDIRxBytesToSend  = 0;
               SetIRQDeassert;
            }
            //Serial.print("Cnt");
            break;
         case wIORegAddrMIDITransmit:
            //wIORegMIDITransmit = Data;
            //Serial.print("Tx");
            break;
         default:
            //Serial.print("Unk");
            break;
     }
      //Serial.printf(" w %02x", Data);
   }
   
   //Serial.printf(" de%02x\n", Address);
}


void IO1HWinit()
{
   IO1Handler = IO1[rwRegNextIO1Hndlr];
   
   if (IO1Handler==IO1H_MIDI)
   {
      Serial.println("MIDI IO1 handler ready");
      for (uint8_t ContNum=0; ContNum < NumMIDIControls;) MIDIControlVals[ContNum++]=63;
      rIORegMIDIStatus = 0;
      MIDIRxBytesToSend = 0;
      midi1.setHandleNoteOff(HWEOnNoteOff);
      midi1.setHandleNoteOn(HWEOnNoteOn);
      midi1.setHandleControlChange(HWEOnControlChange);
      midi1.setHandlePitchChange(HWEOnPitchChange);
   }



}