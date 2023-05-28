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
            DataPortWriteWaitLog(MenuSource[IO1[rwRegSelItem]].ItemType);  
            break;
         case rRegItemNameStart ... (rRegItemNameStart+MaxItemNameLength-1):
            Data = MenuSource[IO1[rwRegSelItem]].Name[Address-rRegItemNameStart];
            //Convert to PETscii, make this a table? Seems fast enough
            if (Data==95) Data=32; //underscore->space
            else if (Data>64) Data ^=32; 
            DataPortWriteWaitLog(Data);  
            break;
         case rRegStreamData:
            DataPortWriteWaitLog(MenuSource[IO1[rwRegSelItem]].Code_Image[StreamOffsetAddr]);
            //inc on read, check for end:
            if (++StreamOffsetAddr >= MenuSource[IO1[rwRegSelItem]].Size) IO1[rRegStrAvailable]=0; //signal end of transfer
            break;
         default: //used for all other IO1 reads
            DataPortWriteWaitLog(IO1[Address]); 
            break;
      }
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead(); 
      TraceLogAddValidData(Data);
      switch(Address)
      {
         case rwRegSelItem:
            IO1[rwRegSelItem]=Data;
            break;
         case rwRegNextIOHndlr:
            if (Data >= IOH_Num_Handlers) Data=0;
            IO1[rwRegNextIOHndlr]= Data;
            eepAddrToWrite = eepAdNextIOHndlr;
            eepDataToWrite = Data;
            IO1[rRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rWRegCurrMenuWAIT:
            IO1[rWRegCurrMenuWAIT]=Data;
            IO1[rRegStatus] = rsChangeMenu; //work this in the main code
            break;
         case rwRegPwrUpDefaults:
            IO1[rwRegPwrUpDefaults]= Data;
            eepAddrToWrite = eepAdPwrUpDefaults;
            eepDataToWrite = Data;
            IO1[rRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegTimezone:
            IO1[rwRegTimezone]= Data;
            eepAddrToWrite = eepAdTimezone;
            eepDataToWrite = Data;
            IO1[rRegStatus] = rsWriteEEPROM; //work this in the main code
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
                  IO1[rRegStatus] = rsIOHWinit; //Support IO handlers at reset
                  break;
               case rCtlStartSelItemWAIT:
                  IO1[rRegStatus] = rsStartItem; //work this in the main code
                  break;
               case rCtlGetTimeWAIT:
                  IO1[rRegStatus] = rsGetTime;   //work this in the main code
                  break;
               case rCtlRunningPRG:
                  IO1[rRegStatus] = rsIOHWinit; //Support IO handlers in PRG
                  break;
            }
            break;
      }
   } //write
}

