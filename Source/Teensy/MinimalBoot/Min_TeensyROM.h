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


//Build options: enable debug messaging at your own risk, can cause emulation interference/fails
// #define DbgMsgs_IO    //Serial out messages (Printf_dbg): Swift, MIDI (mostly out), CRT Chip info
// #define Dbg_TestMin    //Test minimal build by loading a CRT on start
// #define FeatTCPListen //Enable TCP Listen port for remote commands

//less used:  Few have been tested in Minimal build
// #define DbgIOTraceLog //Logs Reads/Writes to/from IO1 to BigBuf. Like debug handler but can use for others
// #define DbgCycAdjLog  //Logs ISR timing adjustments to BigBuf.
// #define Dbg_SerTimChg //Allow commands over serial that tweak timing parameters.
// #define Dbg_SerLog    //Allow commands over serial that display log info
// #define Dbg_SerMem    //Allow commands over serial that display memory info
// #define DbgSpecial    //Special case logging to BigBuf
// #define DbgFab0_3plus     //Only for fab 0.3 or higher PCB! (uses different debug signal)

#define MinimumBuild         //Must be defined for minimal build to identify in common files
#define Num8kSwapBuffers  12 //space for bank swapping upper blocks of large CRTs, must be even number for 16k banks

#ifdef FeatTCPListen
   #define EthernetDeduction   96  
   //Ethernet takes this from RAM1 and another ~96k from RAM2 (when initialized/enabled), plus uses more local variables (see below)
   // Total: ~200k of RAM needed to support Ethernet, 100k if disabled
#else
   #define EthernetDeduction    0
#endif

#define MaxRAM_ImageSize  (184+208-8*Num8kSwapBuffers-EthernetDeduction)  //184 is non-minimal image size;  minus space for 8k swap blocks
   // need to leave ~15k of RAM1 "free for local variables" w/ Ethernet,  OK w/ <7k free w/o it

