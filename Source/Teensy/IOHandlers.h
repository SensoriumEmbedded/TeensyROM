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
MIDIDevice usbHostMIDI(myusbHost);
USBDrive myDrive(myusbHost);
USBFilesystem firstPartition(myusbHost);

EthernetUDP udp;
EthernetClient client;

#define usbDevMIDI usbMIDI
#define IOHNameLength 20  //limited by display location on C64

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

#include "IO_Handlers\IOH_None.c"
#include "IO_Handlers\IOH_MIDI.c"
#include "IO_Handlers\IOH_Debug.c"
#include "IO_Handlers\IOH_TeensyROM.c" 
#include "IO_Handlers\IOH_Swiftlink.c"
#include "IO_Handlers\IOH_EpyxFastLoad.c"

stcIOHandlers* IOHandler[] =  //Synch order/qty with enumIOHandlers (above)
{
   &IOHndlr_None,
   &IOHndlr_MIDI_Datel,      
   &IOHndlr_MIDI_Sequential, 
   &IOHndlr_MIDI_Passport,   
   &IOHndlr_MIDI_NamesoftIRQ,
   &IOHndlr_Debug,
   &IOHndlr_TeensyROM, 
   &IOHndlr_SwiftLink,
   &IOHndlr_EpyxFastLoad,
};
