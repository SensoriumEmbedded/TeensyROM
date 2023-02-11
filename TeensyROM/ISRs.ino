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
   SetDebug2Assert;
   BtnPressed = true;
}


FASTRUN void isrPHI2()
{
   StartCycCnt = ARM_DWT_CYCCNT;
   if (DisablePhi2ISR) return;
   //RESET_CYCLECOUNT;
 	//SetDebug2Assert;
   
   WaitUntil_nS(nS_RWnReady); 
   register uint32_t GPIO_6 = ReadGPIO6; //Address bus and (almost) R/*W are valid on Phi2 rising, Read now
   register uint16_t Address = GP6_Address(GPIO_6); //parse out address
   register uint8_t  Data;
   register bool     IsRead = GP6_R_Wn(GPIO_6);
   
 	WaitUntil_nS(nS_PLAprop); 
   register uint32_t GPIO_9 = ReadGPIO9; //Now read the derived signals
   
   if (!GP9_IO1n(GPIO_9)) //IO1: DExx address space
   {
      Address &= 0xFF;
      if (IsRead) //High (IO1 Read)
      {
         switch(Address)
         {
            case rRegItemType:
               DataPortWriteWait(MenuSource[IO1[rwRegSelItem]].ItemType);  
               break;
            case rRegItemName ... (rRegItemName+MaxItemNameLength-1):
               Data = MenuSource[IO1[rwRegSelItem]].Name[Address-rRegItemName];
               DataPortWriteWait(Data>64 ? (Data^32) : Data);  //Convert to PETscii
               break;
            case rRegStreamData:
               DataPortWriteWait(MenuSource[IO1[rwRegSelItem]].Code_Image[StreamOffsetAddr]);
               //inc on read, check for end:
               if (++StreamOffsetAddr >= MenuSource[IO1[rwRegSelItem]].Size) IO1[rRegStrAddrHi]=0; //zero in Hi reg signals end of transfer
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
            case wRegControl:
               switch(Data)
               {
                  case rCtlVanish:
                     SetGameDeassert;
                     SetExROMDeassert;      
                     LOROM_Image = NULL;
                     HIROM_Image = NULL;  
                     //DisablePhi2ISR = true;
                     SetLEDOff;
                     break;
                  case rCtlVanishReset:  
                     SetGameDeassert;
                     SetExROMDeassert;      
                     LOROM_Image = NULL;
                     HIROM_Image = NULL;  
                     DisablePhi2ISR = true;
                     SetLEDOff;
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
   else if (!GP9_ROML(GPIO_9)) //ROML: 8000-9FFF address space, read only
   {
      if (LOROM_Image!=NULL) DataPortWriteWait(LOROM_Image[Address & 0x1FFF]);  
   }  //ROML
   else if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
   {
      if (HIROM_Image!=NULL) DataPortWriteWait(HIROM_Image[(Address & 0x1FFF)]); 
   }  //ROMH
 #ifdef DebugMessages
   //IO2: DFxx address space
   else if (!GP9_IO2n(GPIO_9)) Serial.printf("IO2 %s %d\n", IsRead ? "Rd from" : "Wr to", Address);
 #endif
     
   //SetDebugDeassert;
}



