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


#include "TeensyROM.h"
#include "Menu_Regs.h"
#include "ROM_Images.h"
#include <SD.h>
#include <USBHost_t36.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>


uint8_t* IO1;  //io1 space/regs
uint8_t* RAM_Image = NULL; //For receiving files from USB Drive & SD
volatile uint8_t doReset = true;
volatile uint8_t BtnPressed = false; 
volatile uint8_t EmulateVicCycles = false;
volatile uint8_t Phi2ISRState = P2I_Normal;
volatile uint8_t IO1Handler = IO1H_None;
uint32_t* CycleTime;
uint16_t StreamOffsetAddr = 0;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;

StructMenuItem *MenuSource = ROMMenu; //init to internal memory

StructMenuItem SDMenu[MaxMenuItems];
char SDPath[250] = "/";

StructMenuItem USBDriveMenu[MaxMenuItems];
char USBDrivePath[250] = "/";

StructMenuItem USBHostMenu = {
  rtNone,  //unsigned char ItemType;
  "<Nothing Sent>",      //char Name[MaxItemNameLength];
  NULL,    //const unsigned char *Code_Image;
  0        //uint16_t Size;
};
uint8_t* HOST_Image = NULL; //For receiving files from USB Host
uint8_t NumUSBHostItems = 1;
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);
USBDrive myDrive(myusb);
USBFilesystem firstPartition(myusb);

unsigned int localPort = 8888;       // local port to listen for UDP packets
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const char timeServer[] = "us.pool.ntp.org"; // time.nist.gov     NTP server
EthernetUDP Udp;

extern "C" uint32_t set_arm_clock(uint32_t frequency);

void setup() 
{
   set_arm_clock( 816000000 );  //force slight overclocking
   
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));
   Serial.printf("CPU Freq: %lu Hz\n", F_CPU_ACTUAL);
   
   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
   DataBufDisable; //buffer disabled
   SetDataPortDirOut; //default to output (for C64 Read)
   SetDMADeassert;
   SetNMIDeassert;
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

   uint32_t MagNumRead;
   EEPROM.get(eepAdMagicNum, MagNumRead);
   if (MagNumRead != eepMagicNum)
   {
      Serial.println("EEPROM first use, setting defaults");
      EEPROM.put(eepAdMagicNum, (uint32_t)eepMagicNum);
      EEPROM.write(eepAdPwrUpDefaults, rpudMusicMask /* | rpudNetTimeMask */); //default music on, eth time synch off
      EEPROM.write(eepAdrwRegTimezone, -8 ); //default to pacific time
   }

   IO1 = (uint8_t*)calloc(IO1_Size, sizeof(uint8_t)); //allocate IO1 space and init to 0
   IO1[rRegStatus]        = rsReady;
   IO1[rWRegCurrMenuWAIT] = rmtTeensy;
   IO1[rRegNumItems]      = sizeof(ROMMenu)/sizeof(ROMMenu[0]);
   IO1[rRegPresence1]     = 0x55;   
   IO1[rRegPresence2]     = 0xAA;   
   for (uint16_t reg=rRegSIDStrStart; reg<rRegSIDStringTerm; reg++) IO1[reg]=' '; 
   IO1[rRegSIDStringTerm] = 0;   
   IO1[rwRegPwrUpDefaults]= EEPROM.read(eepAdPwrUpDefaults);
   IO1[rwRegTimezone]     = EEPROM.read(eepAdrwRegTimezone);  
   SetUpMainMenuROM();

   //CycleTime = (uint32_t*)malloc(NumTimeSamples*sizeof(uint32_t));
   //StreamOffsetAddr = 0;
   //Phi2ISRState = P2I_TimingCheck;
   //while (Phi2ISRState!=P2I_Normal);
   //for (uint8_t SampNum = 0; SampNum < NumTimeSamples; SampNum++) 
   //{
   //   Serial.print(SampNum);
   //   Serial.print(" : ");
   //   Serial.print(CycleTime[SampNum]);
   //   Serial.print(" : ");
   //   Serial.println(CycleTime[SampNum]*(1000000000UL>>16)/(F_CPU_ACTUAL>>16));
   //}
   //free(CycleTime);
   
   Serial.print("TeensyROM 0.2 is on-line\n");
   Serial.flush();
} 
     
void loop()
{
   if (BtnPressed)
   {
      Serial.print("Button detected\n"); 
      SetLEDOn;
      BtnPressed=false;
      SetUpMainMenuROM(); //back to main menu
      Phi2ISRState=P2I_Normal;
   }
   
   if (doReset)
   {
      SetResetAssert; 
      Serial.println("Resetting C64"); 
      Serial.flush();
      delay(50); 
      while(ReadButton==0); //avoid self reset detection
      doReset=false;
      BtnPressed = false;
      SetResetDeassert;
   }
  
   if (IO1[rRegStatus] != rsReady) 
   {  //ISR requested work
      if     (IO1[rRegStatus] == rsChangeMenu) MenuChange();
      else if(IO1[rRegStatus] == rsGetTime)    getNtpTime();
      else if(IO1[rRegStatus] == rsStartItem)  HandleExecution();
      else if(IO1[rRegStatus] == rsIO1HWinit)  IO1HWinit(IO1[rwRegNextIO1Hndlr]);
      
      IO1[rRegStatus] = rsReady;
   }
   
   if (Serial.available()) ServiceSerial();
   
   myusb.Task();
   if (MIDIRxBytesToSend == 0) midi1.read(); //read MIDI-in data in only if ready to send to C64 (buffer empty)
      
   if (MIDITxBytesReceived == 3)  //Transmit MIDI-out data if buffer full/ready from C64
   {
      if (MIDITxBuf[0]<0xf0) midi1.send(MIDITxBuf[0] & 0xf0, MIDITxBuf[1], MIDITxBuf[2], MIDITxBuf[0] & 0x0f);
      else midi1.send(MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2], 0);
      //Serial.printf("Mout: %02x %02x %02x\n", MIDITxBuf[0], MIDITxBuf[1], MIDITxBuf[2]);
      MIDITxBytesReceived = 0;
   }
}

void SetUpMainMenuROM()
{
   //emulate 16k cart ROM
   SetIRQDeassert;
   SetGameAssert;
   SetExROMAssert;
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = TeensyROMC64_bin+0x2000;
   EmulateVicCycles = false;
   IO1HWinit(IO1H_TeensyROM);   
   IO1[rwRegNextIO1Hndlr] = IO1H_MIDI;//IO1H_None; //default load next
   doReset = true;
}

