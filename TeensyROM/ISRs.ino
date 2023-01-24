
FASTRUN void isrResetBtn()
{
   SetDebug2Assert;
   ResetBtnPressed = true;
}

FASTRUN void isrPHI2()
{
   if (DisablePhi2ISR) return;
   RESET_CYCLECOUNT;
 	//SetDebug2Assert;
   
   static uint16_t StreamStartAddr = 0;
   static uint16_t StreamOffsetAddr = 0;
   static uint8_t RegSelect = 0;
   
   WaitUntil_nS(nS_RWnReady); 
   register uint32_t GPIO_6 = ReadGPIO6; //Address bus and (almost) R/*W are valid on Phi2 rising, Read now
   register uint16_t Address = GP6_Address(GPIO_6); //parse out address
   register uint8_t  Data;
   register bool     IsRead = GP6_R_Wn(GPIO_6);
   
 	WaitUntil_nS(nS_PLAprop); 
   register uint32_t GPIO_9 = ReadGPIO9; //Now read the derived signals
   
   if (!GP9_IO1n(GPIO_9)) //IO1: DExx address space
   {
      if (IsRead) //High (Read)
      {
         switch(Address & 0xFF)
         {
            case rwRegSelect:
               DataPortWriteWait(RegSelect);  
               break;
            case rRegNumROMs:
               DataPortWriteWait(sizeof(ROMMenu)/sizeof(ROMMenu[0]));  
               break;
            case rRegROMType:
               DataPortWriteWait(ROMMenu[RegSelect].HW_Config);  
               break;
            case rRegROMName ... (rRegROMName+MAX_ROMNAME_CHARS-1):
               Data = ROMMenu[RegSelect].Name[(Address & 0xFF)-rRegROMName];
               DataPortWriteWait(Data>64 ? (Data^32) : Data);  //Convert to PETscii
               break;
            case rRegStrData:
               DataPortWriteWait(ROMMenu[RegSelect].ROM_Image[StreamOffsetAddr]);
               if (++StreamOffsetAddr >= ROMMenu[RegSelect].Size) //inc on read, check for end
               {  //transfer finished
                  StreamStartAddr=0; //signals end of transfer
                  //StreamOffsetAddr=0;
               }
               break;
            case rRegStrAddrLo:
               DataPortWriteWait(StreamStartAddr & 0xFF);
               break;
            case rRegStrAddrHi:
               DataPortWriteWait(StreamStartAddr>>8);
               break;
            case rRegPresence1:
               DataPortWriteWait(0x55);
               break;
            case rRegPresence2:
               DataPortWriteWait(0xAA);
               break;
         }
         //Serial.printf("Rd %d from %d\n", IO1_RAM[Address & 0xFF], Address);
      }
      else  //write
      {
         Data = DataPortWaitRead(); 
         switch(Address & 0xFF)
         {
            case rwRegSelect:
               RegSelect=Data;
               break;
            case wRegControl:
               switch(Data)
               {
                  case RCtlVanish: //will go out to lunch if called from ext ROM
                     SetGameDeassert;
                     SetExROMDeassert;      
                     LOROM_Image = NULL;
                     HIROM_Image = NULL;  
                     DisablePhi2ISR = true;
                     SetLEDOff;
                     break;
                  case RCtlVanishReset:  //called from ROM, but it's resetting anyway
                     SetGameDeassert;
                     SetExROMDeassert;      
                     LOROM_Image = NULL;
                     HIROM_Image = NULL;  
                     DisablePhi2ISR = true;
                     SetLEDOff;
                     doReset=true;
                     break;
                  case RCtlStartRom:
                     switch(ROMMenu[RegSelect].HW_Config)
                     {
                        case rt16k:
                           SetGameAssert;
                           SetExROMAssert;
                           LOROM_Image = ROMMenu[RegSelect].ROM_Image;
                           HIROM_Image = ROMMenu[RegSelect].ROM_Image+0x2000;
                           doReset=true;
                           break;
                        case rt8kHi:
                           SetGameAssert;
                           SetExROMDeassert;
                           LOROM_Image = NULL;
                           HIROM_Image = ROMMenu[RegSelect].ROM_Image;
                           doReset=true;
                           break;
                        case rt8kLo:
                           SetGameDeassert;
                           SetExROMAssert;
                           LOROM_Image = ROMMenu[RegSelect].ROM_Image;
                           HIROM_Image = NULL;
                           doReset=true;
                           break;
                        case rtPrg:
                           //set up for transfer
                           StreamStartAddr = (ROMMenu[RegSelect].ROM_Image[1]<<8) + ROMMenu[RegSelect].ROM_Image[0];
                           StreamOffsetAddr = 2; //set to start of data
                           break;
                     }
                     break;
               }
               break;
         }
      } //write
   }  //IO1
   else if (!GP9_IO2n(GPIO_9)) //IO2: DFxx address space, virtual RAM
   {
      if (IsRead) //High (Read)
      {
         DataPortWriteWait(IO2_RAM[Address & 0xFF]);  
         //Serial.printf("Rd %d from %d\n", IO2_RAM[Address & 0xFF], Address);
      }
      else  //write
      {
         IO2_RAM[Address & 0xFF] = DataPortWaitRead(); 
         Serial.printf("IO2 Wr %d to 0x%04x\n", IO2_RAM[Address & 0xFF], Address);
      }
   }  //IO2
   else if (!GP9_ROML(GPIO_9)) //ROML: 8000-9FFF address space, read only
   {
      if (LOROM_Image!=NULL) DataPortWriteWait(LOROM_Image[Address & 0x1FFF]);  
   }  //ROML
   else if (!GP9_ROMH(GPIO_9)) //ROMH: A000-BFFF or E000-FFFF address space, read only
   {
      if (HIROM_Image!=NULL) DataPortWriteWait(HIROM_Image[(Address & 0x1FFF)]); 
   }  //ROMH
   
 
   //now the time-sensitive work is done, have a few hundred nS until the next interrupt...
   //SetDebug2Deassert;
}



