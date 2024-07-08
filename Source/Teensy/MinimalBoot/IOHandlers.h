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

//from IOH_TeensyROM.c :
uint8_t RegNextIOHndlr;
volatile uint8_t doReset = false;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;


//#define usbDevMIDI usbMIDI
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

//#include "IO_Handlers/IOH_MIDI.c"
//#include "IO_Handlers/IOH_Debug.c"
//#include "IO_Handlers/IOH_TeensyROM.c" 
//#include "IO_Handlers/IOH_Swiftlink.c"

// Shared files, same at main build:
#include "Common/IOH_None.c"
#include "Common/IOH_EpyxFastLoad.c"
#include "Common/IOH_MagicDesk.c"
#include "Common/IOH_Dinamic.c"
#include "Common/IOH_Ocean1.c"
#include "Common/IOH_FunPlay.c"
#include "Common/IOH_SuperGames.c"
#include "Common/IOH_C64GameSystem3.c"
#include "Common/IOH_EasyFlash.c"
#include "Common/IOH_ZaxxonSuper.c"

stcIOHandlers* IOHandler[] =  //Synch order/qty with enum enumIOHandlers
{
   &IOHndlr_None,               //IOH_None,
   //&IOHndlr_SwiftLink,          //IOH_Swiftlink,
   //&IOHndlr_MIDI_Datel,         //IOH_MIDI_Datel,      
   //&IOHndlr_MIDI_Sequential,    //IOH_MIDI_Sequential, 
   //&IOHndlr_MIDI_Passport,      //IOH_MIDI_Passport,   
   //&IOHndlr_MIDI_NamesoftIRQ,   //IOH_MIDI_NamesoftIRQ,
   //&IOHndlr_Debug,              //IOH_Debug, //last manually selectable, see LastSelectableIOH
                                
   //&IOHndlr_TeensyROM,          //IOH_TeensyROM, 
   &IOHndlr_EpyxFastLoad,       //IOH_EpyxFastLoad,
   &IOHndlr_MagicDesk,          //IOH_MagicDesk,
   &IOHndlr_Dinamic,            //IOH_Dinamic,
   &IOHndlr_Ocean1,             //IOH_Ocean1,
   &IOHndlr_FunPlay,            //IOH_FunPlay,
   &IOHndlr_SuperGames,         //IOH_SuperGames,
   &IOHndlr_C64GameSystem3,     //IOH_C64GameSystem3,
   &IOHndlr_EasyFlash,          //IOH_EasyFlash,
   &IOHndlr_ZaxxonSuper,        //IOH_ZaxxonSuper,
   
};
