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

#define DefSIDSource        rmtTeensy  // Default should always be local (rmtTeensy)
#define DefSIDPath          "/SID Cover Tunes" 
#define DefSIDName          "Sleep Dirt            Frank Zappa" 

#define MaxRAM_ImageSize  (144)  // RAM1 space (in kB) used for CRT & Transfer buffer

//Build options: enable debug messaging at your own risk, can cause emulation interference/fails
// #define DbgMsgs_IO    //Serial out messages (Printf_dbg): Swift, MIDI (mostly out), CRT Chip info
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
// #define DbgSignalASIDIRQ  //state togles on each IRQ triggered to C64 (timed or untimed)
// #define DbgSignalIsrPHI2  //high at start of Phi2 ISR, low when exits

//enabling this on a fab 0.2x PBC could cause damage to your C64!
// fab 0.3 uses different debug signal and direct data buffer dir control
// #define DbgFab0_3plus     //Only for fab 0.3 or higher PCB! 

