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

//  TeensyROM: A C64 ROM emulator and loader/interface cartidge based on the Teensy 4.1
//  Copyright (c) 2023 Travis Smith <travis@sensoriumembedded.com> 

//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"
#include "Menu_Regs.h"
#include "ROM_Images.h"
#include <SD.h>
#include <USBHost_t36.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>


uint8_t IO1[256];  //io1 space/regs
uint8_t RAM_Image[65536];  //For receiving files from USB/SD/etc, todo:do this dynamically...
volatile uint8_t doReset = true;
volatile uint8_t BtnPressed = false; 
volatile uint8_t DisablePhi2ISR = false;
uint16_t StreamOffsetAddr = 0;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;

StructMenuItem *MenuSource = ROMMenu; //init to internal memory

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
MIDIDevice midi1(myusb);
USBDrive myDrive(myusb);
USBFilesystem firstPartition(myusb);

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
   SetResetAssert; //assert reset until main loop()
  
   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Reset_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Reset_Btn_In_PIN), isrButton, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   NVIC_SET_PRIORITY(IRQ_GPIO6789,16); //set HW ints as high priority, otherwise ethernet int timer causes misses
   
   Serial.print("SD Card initialization... ");
   if (SD.begin(BUILTIN_SDCARD)) Serial.println("passed.");
   else Serial.println("***Failed!***");
  
   myusb.begin(); // Start USBHost_t36, HUB(s) and USB devices.
   midi1.setHandleNoteOff(OnNoteOff);
   midi1.setHandleNoteOn(OnNoteOn);
   midi1.setHandleControlChange(OnControlChange);
   midi1.setHandlePitchChange(myPitchChange);

   for (uint16_t reg=0; reg<sizeof(IO1); reg++) IO1[reg]=0; //initialize regs to 0
   IO1[rRegStatus]=rsReady;
   IO1[rWRegCurrMenuWAIT] = rmtTeensy;
   IO1[rRegNumItems] = sizeof(ROMMenu)/sizeof(ROMMenu[0]);
   IO1[rRegPresence1]  = 0x55;   
   IO1[rRegPresence2]  = 0xAA;   

   Serial.print("TeensyROM 0.01 is on-line\n");

} 
     
void loop()
{
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
  
   if (IO1[rRegStatus] != rsReady) 
   {
      if(IO1[rRegStatus] == rsChangeMenu) MenuChange();
      else if(IO1[rRegStatus] == rsGetTime) getNtpTime();
      else HandleExecution(); //rsStartItem is only other non-ready setting
      IO1[rRegStatus] = rsReady;
   }
   
   if (Serial.available()) ServiceSerial();
   
   myusb.Task();
   midi1.read();
}

void SetUpMainMenuROM()
{
   SetGameDeassert;
   SetExROMAssert;
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = NULL;
}

