
//Compile with 816MHz (overclock) option set

#include "ROM_Images.h"
#include "TeensyROM.h"

uint8_t IO2_RAM[256];
volatile uint8_t doReset = true;
volatile uint8_t extReset = false;
uint16_t StreamStartAddr = 0;
uint16_t StreamOffsetAddr = 0;
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
      Serial.println("Resetting C64"); 
      SetResetAssert; 
      delay(5); 
      SetResetDeassert;
      delay(3); //avoid self reset detection
      doReset=false;
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


