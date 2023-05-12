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
            EEPROM.write(eepAdNextIO1Hndlr, IO1[rwRegNextIO1Hndlr]); 
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
            EEPROM.write(eepAdTimezone, Data); 
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
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      if      (Address == rIORegAddrMIDIStatus)
      {
         DataPortWriteWait(rIORegMIDIStatus);  
         TraceLogAddValidData(rIORegMIDIStatus);
      }
      else if (Address == rIORegAddrMIDIReceive) //MIDI-in from USB kbd (interrupt driven)
      {
         if(MIDIRxBytesToSend)
         {
            DataPortWriteWait(MIDIRxBuf[--MIDIRxBytesToSend]);  
            TraceLogAddValidData(MIDIRxBuf[MIDIRxBytesToSend]);
         }
         else
         {
            DataPortWriteWait(0); 
            TraceLogAddValidData(0);
            Printf_dbgMIDI("unreq\n"); //unrequested read from Rx reg.
         }
         if (MIDIRxBytesToSend == 0) //if we're done/empty, remove the interrupt
         {
            rIORegMIDIStatus &= ~(MIDIStatusRxFull | MIDIStatusIRQReq);
            SetIRQDeassert;
         }
      } else
      {
         DataPortWriteWait(0); //read 0s from all other regs in IO1
         TraceLogAddValidData(0);
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      Data = DataPortWaitRead(); 
      if (Address == wIORegAddrMIDIControl)
      {
         if (Data == 0x03) //Master Reset
         {
            rIORegMIDIStatus   = 0;
            MIDIRxBytesToSend  = 0;
            //MIDIRxIRQEnabled = false;
            SetIRQDeassert;
         }
         MIDIRxIRQEnabled = (Data & 0x80) == 0x80;
         if (MIDIRxIRQEnabled) //Receive Interrupt Enable set
         {
            rIORegMIDIStatus |= MIDIStatusDCD;
            MIDIRxBytesToSend = 0; 
            Printf_dbgMIDI("RIE:%02x\n", Data);
         }
         if ((Data & 0x1C) == 0x14) // xxx101xx Word Select == 8 Bits + No Parity + 1 Stop Bit
         {
            rIORegMIDIStatus |= MIDIStatusTxRdy;              
         }
      }
      else if (Address == wIORegAddrMIDITransmit) //Tx MIDI out to USB instrument
      {
         if (MIDITxBytesReceived < 3) //make sure there's not a full packet already in progress
         {
            if ((Data & 0x80) == 0x80) //header byte, start new packet
            {
               if(MIDITxBytesReceived) Printf_dbgMIDI("drop %d\n", MIDITxBytesReceived); //had another in progress
               MIDITxBytesReceived = 0;
               switch(Data)
               {
                  case 0x00 ... 0xef: 
                  case 0xf1: 
                  case 0xf2: 
                  case 0xf3: 
                     //2-3 byte messages
                     MIDITxBuf[MIDITxBytesReceived++] = Data;
                     break;
                  case 0xf6:
                  case 0xf8 ... 0xff:
                     //1 byte messages, send now
                     MIDITxBuf[0] = Data;
                     MIDITxBuf[1] = 0;
                     MIDITxBuf[2] = 0;
                     MIDITxBytesReceived = 3;
                     break;
                  default:
                     Printf_dbgMIDI("igh: %02x\n", Data);
                     break;
               }
            }
            else  //adding data to existing
            {
               if(MIDITxBytesReceived > 0) //make sure we accepted a valid header byte previously
               {
                  MIDITxBuf[MIDITxBytesReceived++] = Data;
                  if(MIDITxBytesReceived == 2 && ((MIDITxBuf[0] & 0xf0) == 0xc0 || (MIDITxBuf[0] & 0xf0) == 0xd0 || MIDITxBuf[0] == 0xf1 || MIDITxBuf[0] == 0xf3))
                  { //single extra byte commands, send now
                     MIDITxBuf[2] = 0;
                     MIDITxBytesReceived = 3;
                  }
               }
               else Printf_dbgMIDI("igd: %02x\n", Data);
            }
            rIORegMIDIStatus &= ~MIDIStatusIRQReq;
            if(MIDITxBytesReceived == 3) rIORegMIDIStatus &= ~MIDIStatusTxRdy; //not ready, waiting for USB transmit
         }
         else Printf_dbgMIDI("Miss!\n");
      }
      TraceLogAddValidData(Data);
   }
}

//IO1 Handler for Debug _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_Debug(uint8_t Address, bool R_Wn)
{
   #ifndef DbgIOTraceLog
      BigBuf[BigBufCount] = Address; //initialize w/ address 
   #endif
   if (R_Wn) //High (IO1 Read)
   {
      //DataPortWriteWait(0);
      //BigBuf[BigBufCount] |= (0<<8) | IOTLDataValid;
      //Serial.printf("Rd $de%02x\n", Address);
   }
   else  // IO1 write
   {
      BigBuf[BigBufCount] |= (DataPortWaitRead()<<8) | IOTLDataValid;
      //Serial.printf("wr $de%02x:$%02x\n", Address, Data);
   }
   #ifndef DbgIOTraceLog
      if (R_Wn) BigBuf[BigBufCount] |= IOTLRead;
      if (BigBufCount < BigBufSize) BigBufCount++;
   #endif
}


//IO1 Handler Init _________________________________________________________________________________________

void IO1HWinit(uint8_t NewIO1Handler)
{
   MIDIRxIRQEnabled = false;
   MIDIRxBytesToSend = 0;
   rIORegMIDIStatus = 0;
   free(BigBuf);
   BigBuf = (uint32_t*)malloc(BigBufSize*sizeof(uint32_t));
   BigBufCount = 0;

   switch(NewIO1Handler)
   {
      case IO1H_TeensyROM:  //MIDI handlers for MIDI2SID
         midi1.setHandleNoteOff             (M2SOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (M2SOnNoteOn);              // 9x
         midi1.setHandleAfterTouchPoly      (NULL);                     // Ax
         midi1.setHandleControlChange       (M2SOnControlChange);       // Bx
         midi1.setHandleProgramChange       (NULL);                     // Cx
         midi1.setHandleAfterTouch          (NULL);                     // Dx
         midi1.setHandlePitchChange         (M2SOnPitchChange);         // Ex
         midi1.setHandleSystemExclusive     (M2SOnSystemExclusive);     // F0   
         midi1.setHandleTimeCodeQuarterFrame(NULL);                     // F1
         midi1.setHandleSongPosition        (NULL);                     // F2
         midi1.setHandleSongSelect          (NULL);                     // F3
         midi1.setHandleTuneRequest         (NULL);                     // F6
         midi1.setHandleRealTimeSystem      (NULL);                     // F8-FF (except FD)
         Serial.println("TeensyROM/MIDI2SID IO1 handler ready");
         break;
      case IO1H_MIDI_Datel:
      case IO1H_MIDI_Sequential:
      case IO1H_MIDI_Passport:
      case IO1H_MIDI_NamesoftIRQ:
         for (uint8_t ContNum=0; ContNum < NumMIDIControls;) MIDIControlVals[ContNum++]=63;
         midi1.setHandleNoteOff             (HWEOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (HWEOnNoteOn);              // 9x
         midi1.setHandleAfterTouchPoly      (HWEOnAfterTouchPoly);      // Ax
         midi1.setHandleControlChange       (HWEOnControlChange);       // Bx
         midi1.setHandleProgramChange       (HWEOnProgramChange);       // Cx
         midi1.setHandleAfterTouch          (HWEOnAfterTouch);          // Dx
         midi1.setHandlePitchChange         (HWEOnPitchChange);         // Ex
         midi1.setHandleSystemExclusive     (HWEOnSystemExclusive);     // F0   
         midi1.setHandleTimeCodeQuarterFrame(HWEOnTimeCodeQuarterFrame);// F1
         midi1.setHandleSongPosition        (HWEOnSongPosition);        // F2
         midi1.setHandleSongSelect          (HWEOnSongSelect);          // F3
         midi1.setHandleTuneRequest         (HWEOnTuneRequest);         // F6
         midi1.setHandleRealTimeSystem      (HWEOnRealTimeSystem);      // F8-FF (except FD)
         // not catching F4, F5, F7 (end of SysEx), and FD         
         SetMIDIRegs(NewIO1Handler - IO1H_MIDI_Datel);
         NewIO1Handler = IO1H_MIDI_Datel; //all 4 are the same after regs are set
         break;
      case IO1H_Debug:
         midi1.setHandleNoteOff             (DbgOnNoteOff);             // 8x
         midi1.setHandleNoteOn              (DbgOnNoteOn);              // 9x
         midi1.setHandleAfterTouchPoly      (DbgOnAfterTouchPoly);      // Ax
         midi1.setHandleControlChange       (DbgOnControlChange);       // Bx
         midi1.setHandleProgramChange       (DbgOnProgramChange);       // Cx
         midi1.setHandleAfterTouch          (DbgOnAfterTouch);          // Dx
         midi1.setHandlePitchChange         (DbgOnPitchChange);         // Ex
         midi1.setHandleSystemExclusive     (DbgOnSystemExclusive);     // F0   
         midi1.setHandleTimeCodeQuarterFrame(DbgOnTimeCodeQuarterFrame);// F1
         midi1.setHandleSongPosition        (DbgOnSongPosition);        // F2
         midi1.setHandleSongSelect          (DbgOnSongSelect);          // F3
         midi1.setHandleTuneRequest         (DbgOnTuneRequest);         // F6
         midi1.setHandleRealTimeSystem      (DbgOnRealTimeSystem);      // F8-FF (except FD)
         // not catching F4, F5, F7 (end of SysEx), and FD                  
         Serial.println("Debug IO1 handler ready");
         break;
   }
   
   IO1Handler = NewIO1Handler;
}

void SetMIDIRegs(uint8_t MIDI_ID)
{  
   //these must match enum IO1Handlers order/qty
   char sMIDIType[][15] = {
      "Datel/Siel", 
      "Sequential", 
      "Passport/Sent", 
      "Namesoft IRQ",
      };

   uint8_t MidiRegs[][4] = {
      //wControl, rStatus, wTransmit, rReceive $de00+
      4,6,5,7,  // Datel/Siel
      0,2,1,3,  // Sequential
      8,8,9,9,  // Passport/Sent
      0,2,1,3,  // Namesoft IRQ (same as seq, no NMI)
   };

   wIORegAddrMIDIControl  = MidiRegs[MIDI_ID][0];
   rIORegAddrMIDIStatus   = MidiRegs[MIDI_ID][1];
   wIORegAddrMIDITransmit = MidiRegs[MIDI_ID][2];
   rIORegAddrMIDIReceive  = MidiRegs[MIDI_ID][3];

   Serial.printf("%s MIDI IO1 handler ready\n", sMIDIType[MIDI_ID]);
}
