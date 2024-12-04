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

#include "ROMs/TeensyROMC64.h" //TeensyROM Menu cart, stored in RAM

char strVersionNumber[17] = "v0.6.3"; //*VERSION*

//Build options: enable debug messaging at your own risk, can cause emulation interference/fails
// #define DbgMsgs_IO     //Serial out messages (Printf_dbg): Swift, MIDI (mostly out), CRT Chip info
// #define DbgMsgs_M2S   //MIDI2SID MIDI handler messages
// #define Dbg_SerTimChg //Serial commands that tweak timing parameters.
// #define Dbg_SerSwift  //Serial commands that tweak SwiftLink parameters.
// #define Dbg_SerMem    //Serial commands that display memory info
// #define Dbg_SerASID   //Serial commands that test the ASID player + queue adjust info
 
//logging:
// #define Dbg_SerLog    //Serial commands that display log info
// #define DbgIOTraceLog //Logs Reads/Writes to/from IO1 to BigBuf. Like debug handler but can use for others
// #define DbgCycAdjLog  //Logs ISR timing adjustments to BigBuf.
// #define DbgSpecial    //Special case logging to BigBuf
 
//Debug HW signal usage. Recommend using only 1 at a time.
// #define DbgFab0_3plus     //Only for fab 0.3 or higher PCB! (uses different debug signal)
// #define DbgSignalASIDIRQ  //state togles on each IRQ triggered to C64 (timed or untimed)
// #define DbgSignalIsrPHI2  //high at start of Phi2 ISR, low when exits



#define nfcScanner     //nfc scanner libs/code included in build
#define nfcStateEnabled       0
#define nfcStateBitDisabled   1
#define nfcStateBitPaused     2

#define eepBMTitleSize       75  //max chars in bookmark title
#define eepBMURLSize        225  //Max Chars in bookmark URL path
#define eepNumBookmarks       9  //Num Bookmarks saved

#ifdef nfcScanner
   #define MaxRAM_ImageSize  (184-40)  // ~18k added by host serial & nfc libs, crossed a 32k code boundry (22k more padding)
   //"626k Free"
   uint8_t Lastuid[7];  // Buffer to store the last UID read
#else
   #define MaxRAM_ImageSize  184  //normal max 
   //"666k Free"
#endif

#define DefSIDSource        rmtTeensy  // Default should always be local (rmtTeensy)
#define DefSIDPath          "/SID Cover Tunes" 
#define DefSIDName          "Sleep Dirt            Frank Zappa" 


