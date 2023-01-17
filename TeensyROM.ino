
//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"
#include "ROMs\586220ast_Diagnostics.h"
#include "ROMs\Jupiter_Lander.h"
#include "ROMs\781220_Dead_Test.h"
#include "ROMs\Donkey_Kong.BIN.h"

uint8_t IO1_RAM[256];
volatile uint8_t doReset = true;
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
   SetExROMDeassert;
   SetGameDeassert; 

   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   
   attachInterrupt( digitalPinToInterrupt( PHI2_PIN ), isrPHI2, RISING );
   
   SetDebugDeassert;
   SetDebug2Deassert;
   //SetExROMAssert;
   //SetGameAssert;
  
   Serial.print("TeensyROM is on-line\n");

} 
  
void loop()
{
   if (doReset)
   {
      doReset=false;
      Serial.println("Resetting C64"); 
      SetResetAssert; 
      delay(5); 
      SetResetDeassert;
   }
}

FASTRUN void isrPHI2()
{
   RESET_CYCLECOUNT;
   
 	SetDebug2Assert;
   //SetDebugDeassert;
   WaitUntil_nS(nS_RWnReady); 

   register uint32_t GPIO_6 = ReadGPIO6; //Address bus and (almost) R/*W are valid on Phi2 rising, Read now
    
   register uint16_t Address = GP6_Address(GPIO_6); //parse out address
   register bool IsRead = GP6_R_Wn(GPIO_6);
   
 	WaitUntil_nS(nS_PLAprop); 
   
   register uint32_t GPIO_9 = ReadGPIO9; //Now read the derived signals
   if (!GP9_IO1n(GPIO_9)) //IO1: DExx address space
   {
      if (IsRead) //High (Read)
      {
         DataPortWriteWait(IO1_RAM[Address & 0xFF]);  
         //Serial.printf("Rd %d from %d\n", IO1_RAM[Address & 0xFF], Address);
      }
      else  //write
      {
         IO1_RAM[Address & 0xFF] = DataPortWaitRead(); 
         Serial.printf("Wr %d to %04x\n", IO1_RAM[Address & 0xFF], Address);
         if (Address == 57000)   //DEA8
         {
            switch (IO1_RAM[Address & 0xFF]) 
            {
            case 1:
               //reset C64
               doReset=true;
               break;
            case 2:
               Serial.println("Starting ExROM/ROML cartridge");
               SetGameDeassert;
               SetExROMAssert;
               LOROM_Image = a586220ast_Diagnostics_BIN;
               HIROM_Image = NULL;
               doReset=true;
               //works with H/L/3FFF on both
               //only requires L/1FFF
               break;
            case 3:
               Serial.println("Starting GAME/ROMH cartridge");
               SetExROMDeassert;
               SetGameAssert;
               LOROM_Image = NULL;
               HIROM_Image = a781220_Dead_Test_BIN;
               //ROM_Image = Jupiter_Lander_BIN;
               doReset=true;
               //requires ROMH and 1FFF as only response
               break;
            case 4:
               Serial.println("Starting GAME/ROMH/ExROM/ROML (16k) cartridge");
               SetExROMAssert;
               SetGameAssert;
               LOROM_Image = Donkey_Kong_BIN;
               HIROM_Image = Donkey_Kong_BIN + 0x2000;
               doReset=true;
               //works with H/L/3FFF on both
               //only requires H(1FFF | 2000)/L(1FFF)
               break;
            }

         }
      }
   }  //IO1
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



