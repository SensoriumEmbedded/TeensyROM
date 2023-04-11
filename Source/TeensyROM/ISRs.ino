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


FASTRUN void isrButton()
{
   BtnPressed = true;
}


FASTRUN void isrPHI2() //Phi2 rising edge
{
   //if ((ARM_DWT_CYCCNT-StartCycCnt) > 1500)... //check for missed cycles?
   StartCycCnt = ARM_DWT_CYCCNT;     //RESET_CYCLECOUNT; kills any other delay() type opperations
   if (DisablePhi2ISR) return;
   SetDebugAssert;

   WaitUntil_nS(nS_RWnReady); 
   register uint8_t  Data;
   register uint32_t GPIO_6 = ReadGPIO6; //Address bus and (almost) R/*W are valid on Phi2 rising, Read now
   register uint16_t Address = GP6_Address(GPIO_6); //parse out address
   
 	WaitUntil_nS(nS_PLAprop); 
   register uint32_t GPIO_9 = ReadGPIO9; //Now read the derived signals 
   
   if (!GP9_ROML(GPIO_9)) //ROML: 8000-9FFF address space, read only
   {
      if (LOROM_Image!=NULL) DataPortWriteWait(LOROM_Image[Address & 0x1FFF]);  
   }  //ROML
   else if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
   {
      if (HIROM_Image!=NULL) DataPortWriteWait(HIROM_Image[(Address & 0x1FFF)]); 
   }  //ROMH
   else if (!GP9_IO1n(GPIO_9)) //IO1: DExx address space
   {
      Address &= 0xFF;
      if (GP6_R_Wn(GPIO_6)) //High (IO1 Read)
      {
         switch(Address)
         {
            case rRegItemType:
               DataPortWriteWait(MenuSource[IO1[rwRegSelItem]].ItemType);  
               break;
            case rRegItemNameStart ... (rRegItemNameStart+MaxItemNameLength-1):
               Data = MenuSource[IO1[rwRegSelItem]].Name[Address-rRegItemNameStart];
               DataPortWriteWait(Data>64 ? (Data^32) : Data);  //Convert to PETscii
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
                     DisablePhi2ISR = true;
                     doReset=true;
                     break;
                  case rCtlStartSelItemWAIT:
                     IO1[rRegStatus] = rsStartItem; //work this in the main code
                     break;
                  case rCtlGetTimeWAIT:
                     IO1[rRegStatus] = rsGetTime; //work this in the main code
                     break;
               }
               break;
         }
      } //write
   }  //IO1
 #ifdef DebugMessages
   //IO2: DFxx address space
   else if (!GP9_IO2n(GPIO_9)) Serial.printf("IO2 %s %d\n", GP6_R_Wn(GPIO_6) ? "Rd from" : "Wr to", Address);
 #endif

if (HIROM_Image!=NULL) // && SetExROMDeassert(ed)
{
   while(GP6_Phi2(ReadGPIO6)); //Re-align to phi2 falling   
   //phi2 has gone low..........................................................................
   
   StartCycCnt = ARM_DWT_CYCCNT;

   WaitUntil_nS(nS_VICStart);
   
   GPIO_6 = ReadGPIO6; //Address bus and R/*W 
   Address = GP6_Address(GPIO_6); //parse out address
   GPIO_9 = ReadGPIO9; //Now read the derived signals

   if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
   {
      DataPortWriteWait(HIROM_Image[(Address & 0x1FFF)]); //uses same hold time as normal cycle
   } 
}
   
   //leave time enough time to re-trigger on rising edge!
   SetDebugDeassert;    
}



