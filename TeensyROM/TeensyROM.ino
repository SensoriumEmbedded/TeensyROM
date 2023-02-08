
//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"
#include "Menu_Regs.h"
#include "ROM_Images.h"
#include <SD.h>
#include <USBHost_t36.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

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

StructMenuItem USBDriveMenu[MaxMenuItems];
uint8_t NumUSBDriveItems = 0;
char USBDrivePath[250] = "/";

StructMenuItem USBHostMenu;
uint8_t NumUSBHostItems = 0;
USBHost myusb;
USBHub hub1(myusb);
USBDrive myDrive(myusb);
USBFilesystem firstPartition(myusb);

uint8_t LastSecBCD  =0;
uint8_t LastMinBCD  =0;
uint8_t LastHourBCD =0;
unsigned int localPort = 8888;       // local port to listen for UDP packets
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const char timeServer[] = "us.pool.ntp.org"; // time.nist.gov     NTP server
int timeZone = -8;  // -8==Pacific Standard Time, -7==Pacific Daylight Time (USA)
EthernetUDP Udp;

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
   
   Serial.print("SD Card initialization... ");
   if (SD.begin(BUILTIN_SDCARD)) Serial.println("passed.");
   else Serial.println("***Failed!***");
  
   Serial.print("USB Drive initialization... ");
   myusb.begin(); // Start USBHost_t36, HUB(s) and USB devices.
   // future USBFilesystem will begin automatically, begin(USBDrive) is a temporary feature
   if (firstPartition.begin(&myDrive)) Serial.println("passed.");
   else Serial.println("***Failed!***");

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
      if(RegStatus == rsChangeMenu) MenuChange();
      else if(RegStatus == rsGetTime) getNtpTime();
      else HandleExecution(); //rsStartItem is only other non-ready setting
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
