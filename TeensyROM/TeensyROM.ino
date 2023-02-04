
//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"
#include "Menu_Regs.h"
#include "ROM_Images.h"
#include <SD.h>

uint8_t IO2_RAM[256];
volatile uint8_t doReset = true;
volatile uint8_t BtnPressed = false;
volatile uint8_t DisablePhi2ISR = false;
uint8_t RegStatus = rsReady;
uint8_t RegSelect = 0;
uint16_t StreamStartAddr = 0;
uint16_t StreamOffsetAddr = 0;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;

uint8_t CurrentMenu = rmtTeensy;
StructMenuItem *MenuSource = ROMMenu; //init to internal memory
uint8_t NumMenuItems = sizeof(ROMMenu)/sizeof(ROMMenu[0]);

StructMenuItem SDMenu[MaxMenuItems];
uint8_t NumSDItems = 0;
char SDPath[250] = "/";

StructMenuItem USBHostMenu;
uint8_t NumUSBHostItems = 0;

void setup() 
{
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));
   Serial.print(F_CPU_ACTUAL);
   Serial.println(" Hz");
   
   DataBufDisable; //buffer disabled
   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
   SetDataPortDirOut; //default to output (for C64 Read)
   SetDMADeassert;
   SetNMIDeassert;
   SetIRQDeassert;
   SetUpMainMenuROM();
   SetLEDOn;
   SetDebugDeassert;
   SetResetDeassert;
  
   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Reset_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Reset_Btn_In_PIN), isrButton, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   
   if (SD.begin(BUILTIN_SDCARD)) 
   {
     Serial.println("SD card initialized");
     LoadSDDirectory();  
   }
   else
   {
     Serial.println("SD card initialization failed!"); 
   }   
   
   Serial.print("TeensyROM 0.01 is on-line\n");
   //go directly to BASIC:
      //SetGameDeassert;
      //SetExROMDeassert;      
      //LOROM_Image = NULL;
      //HIROM_Image = NULL;  
      //DisablePhi2ISR = true;
      //SetLEDOff;

} 
     
void loop()
{
   if (RegStatus != rsReady) 
   {
      HandleExecution(); //rsStartItem is only non-ready setting, currently
      RegStatus = rsReady;
   }
   
   if (BtnPressed)
   {
      Serial.print("Button detected\n"); 
      SetLEDOn;
      BtnPressed=false;
      SetDebugDeassert;
      SetUpMainMenuROM(); //back to main menu
      DisablePhi2ISR=false;
      doReset=true;
   }
   
   if (doReset)
   {
      Serial.print("Resetting C64..."); 
      SetResetAssert; 
      delay(50); 
      while(ReadButton==0); //avoid self reset detection
      Serial.print("Done\n");
      doReset=false;
      BtnPressed = false;
      SetDebugDeassert;
      SetResetDeassert;
   }
  
   if (Serial.available()) ServiceSerial();
}

void SetUpMainMenuROM()
{
   SetGameDeassert;
   SetExROMAssert;
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = NULL;
}
