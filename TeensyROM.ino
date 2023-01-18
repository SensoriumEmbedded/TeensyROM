
//Compile with 816MHz (overclock) option set

#include "ROM_Images.h"
#include "TeensyROM.h"

uint8_t IO2_RAM[256];
volatile uint8_t doReset = true;
volatile uint8_t extReset = false;
uint8_t RegSelect = 0;
static const unsigned char *HIROM_Image = NULL;
static const unsigned char *LOROM_Image = NULL;

void setup() 
{
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));
   Serial.print(F_CPU_ACTUAL);
   Serial.println(" Hz");
   
   SetResetDeassert;
   DataBufDisable; //buffer disabled
   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
   SetDataPortDirOut; //default to output (for C64 Read)
   //SetExROMDeassert;
   //SetGameDeassert; 

   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   attachInterrupt( digitalPinToInterrupt(Reset_In_PIN), isrReset, FALLING );
   
   SetDebugDeassert;
   SetDebug2Deassert;
   SetUpMainMenuROM();
  
   Serial.print("TeensyROM is on-line\n");

} 
     
void loop()
{
   if (extReset)
   {
      Serial.println("External Reset detected"); 
      SetUpMainMenuROM(); //back to main menu
      extReset=false;
   }
   
   if (doReset)
   {
      doReset=false;
      Serial.println("Resetting C64"); 
      SetResetAssert; 
      delay(3); 
      SetResetDeassert;
      delay(1); //avoid self reset detection
      extReset = false;
   }
}

void SetUpMainMenuROM()
{
   SetGameDeassert;
   SetExROMAssert;
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = NULL;
}

FASTRUN void isrReset()
{
   extReset = true;
}

FASTRUN void isrPHI2()
{
   RESET_CYCLECOUNT;
   
 	SetDebug2Assert;
   //SetDebugDeassert;
   
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
               if(Data==RCtlStartRom)
               {
                  switch(ROMMenu[RegSelect].HW_Config)
                  {
                     case rt16k:
                        SetGameAssert;
                        SetExROMAssert;
                        LOROM_Image = ROMMenu[RegSelect].ROM_Image;
                        HIROM_Image = ROMMenu[RegSelect].ROM_Image+0x2000;
                        break;
                     case rt8kHi:
                        SetGameAssert;
                        SetExROMDeassert;
                        LOROM_Image = NULL;
                        HIROM_Image = ROMMenu[RegSelect].ROM_Image;
                        break;
                     case rt8kLo:
                        SetGameDeassert;
                        SetExROMAssert;
                        LOROM_Image = ROMMenu[RegSelect].ROM_Image;
                        HIROM_Image = NULL;
                        break;
                     case rtPrg:
                        SetGameAssert;
                        SetExROMAssert;
                        LOROM_Image = NULL;
                        HIROM_Image = NULL;
                        break;
                  }
                  doReset=true;
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
   SetDebug2Deassert;
}



