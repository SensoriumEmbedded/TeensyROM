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


USBHost myusbHost;
USBHub hub1(myusbHost);
USBHub hub2(myusbHost);
MIDIDevice_BigBuffer usbHostMIDI(myusbHost);
USBDrive myDrive(myusbHost);
USBFilesystem firstPartition(myusbHost);

USBHIDParser hid1(myusbHost);
USBHIDParser hid2(myusbHost);
USBHIDParser hid3(myusbHost);  //need all 3?

USBSerial USBHostSerial(myusbHost);   //update HostSerialType to match, defined in PN532_UHSU.h
//USBSerial           USBHostSerial(myusbHost);    // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer USBHostSerial(myusbHost, 1); // Handles anything up to 512 bytes
//    doesn't seem to change operation, but uses an extra 3,456 bytes of RAM1
//USBSerial_BigBuffer USBHostSerial(myusbHost);    // Handles up to 512 but by default only for those > 64 bytes.  
//    doesnt seem to work, at least not for CH340 comm

EthernetUDP udp;
EthernetClient client;

#define usbDevMIDI usbMIDI
#define IOHNameLength 20  //limited by display location on C64
#define nfcStateEnabled       0
#define nfcStateBitDisabled   1
#define nfcStateBitPaused     2

struct stcIOHandlers
{
  char Name[IOHNameLength];                        //Name of handler
  void (*InitHndlr)();                             //Called once at handler startup
  void (*IO1Hndlr)(uint8_t Address, bool R_Wn);    //IO1 R/W handler
  void (*IO2Hndlr)(uint8_t Address, bool R_Wn);    //IO2 R/W handler
  void (*ROMLHndlr)(uint32_t Address);             //ROML Read handler, in addition to any ROM data sent
  void (*ROMHHndlr)(uint32_t Address);             //ROMH Read handler, in addition to any ROM data sent
  void (*PollingHndlr)();                          //Polled in main routine
  void (*CycleHndlr)();                            //called at the end of EVERY c64 cycle
};

#include "IO_Handlers/IOH_MIDI.c"
#include "IO_Handlers/IOH_Debug.c"
#include "IO_Handlers/IOH_TeensyROM.c" 
#include "IO_Handlers/IOH_TR_BASIC.c" 
#include "IO_Handlers/IOH_Swiftlink.c"
#include "IO_Handlers/IOH_ASID.c"
#include "MinimalBoot/Common/IOH_None.c"
#include "MinimalBoot/Common/IOH_EpyxFastLoad.c"
#include "MinimalBoot/Common/IOH_MagicDesk.c"
#include "MinimalBoot/Common/IOH_Dinamic.c"
#include "MinimalBoot/Common/IOH_Ocean1.c"
#include "MinimalBoot/Common/IOH_FunPlay.c"
#include "MinimalBoot/Common/IOH_SuperGames.c"
#include "MinimalBoot/Common/IOH_C64GameSystem3.c"
#include "MinimalBoot/Common/IOH_EasyFlash.c"
#include "MinimalBoot/Common/IOH_ZaxxonSuper.c"
#include "IO_Handlers/IOH_GMod2.c"
#include "MinimalBoot/Common/IOH_MagicDesk2.c"


stcIOHandlers* IOHandler[] =  //Synch order/qty with enum enumIOHandlers
{
   &IOHndlr_None,               //IOH_None,
   &IOHndlr_SwiftLink,          //IOH_Swiftlink,
   &IOHndlr_MIDI_Datel,         //IOH_MIDI_Datel,      
   &IOHndlr_MIDI_Sequential,    //IOH_MIDI_Sequential, 
   &IOHndlr_MIDI_Passport,      //IOH_MIDI_Passport,   
   &IOHndlr_MIDI_NamesoftIRQ,   //IOH_MIDI_NamesoftIRQ,
   &IOHndlr_Debug,              //IOH_Debug, //last manually selectable, see LastSelectableIOH
                                
   &IOHndlr_TeensyROM,          //IOH_TeensyROM, 
   &IOHndlr_EpyxFastLoad,       //IOH_EpyxFastLoad,
   &IOHndlr_MagicDesk,          //IOH_MagicDesk,
   &IOHndlr_Dinamic,            //IOH_Dinamic,
   &IOHndlr_Ocean1,             //IOH_Ocean1,
   &IOHndlr_FunPlay,            //IOH_FunPlay,
   &IOHndlr_SuperGames,         //IOH_SuperGames,
   &IOHndlr_C64GameSystem3,     //IOH_C64GameSystem3,
   &IOHndlr_EasyFlash,          //IOH_EasyFlash,
   &IOHndlr_ZaxxonSuper,        //IOH_ZaxxonSuper,
   &IOHndlr_ASID,               //IOH_ASID,
   &IOHndlr_TR_BASIC,           //IOH_TR_BASIC,
   &IOHndlr_GMod2,              //IOH_GMod2,
   &IOHndlr_MagicDesk2,         //IOH_MagicDesk2,
};
