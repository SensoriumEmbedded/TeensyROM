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
  // #define DbgMsgs_IO    //Serial out messages (Printf_dbg): CRT Chip info
  // #define Dbg_TestMin    //Test minimal build by loading a CRT on start
   #define FeatTCPListen //Enable TCP Listen capability for remote commands

//less used:  Few have been tested in Minimal build
  // #define DbgIOTraceLog //Logs Reads/Writes to/from IO1 to BigBuf. Like debug handler but can use for others
  // #define DbgCycAdjLog  //Logs ISR timing adjustments to BigBuf.
  // #define Dbg_SerTimChg //Allow commands over serial that tweak timing parameters.
  // #define Dbg_SerLog    //Allow commands over serial that display log info
  // #define Dbg_SerMem    //Allow commands over serial that display memory info
  // #define DbgSpecial    //Special case logging to BigBuf
  // #define DbgFab0_3plus     //Only for fab 0.3 or higher PCB! (uses different debug signal)

// Use debug signal line to sense RESET on C64. Use this if you want to trigger
// an external reset and TeensyROM will boot into the menu again. This requires
// a hardware modification. On 0.2.x PCBs the trace from dot_clk to U4 needs to
// be cut and a jumper wire from RESET to the right pin on U4. 0.3.x PCBs require
// more changes, because this line is configured as output.
  // #define DbgSignalSenseReset

#define MinimumBuild         //Must be defined for minimal build to identify in common files
#define Num8kSwapBuffers  16 //space for bank swapping upper blocks of large CRTs
                             //  Must be even number for 16k banks
                             //  Have seen SNKvsCAP (stronger) use 14 within a scene
                             //  Used by EZFlash and MagicDesk2 only
                             //     Leave space (with Eth Listen on) for other cart types up to 512k that don't use it 
                             //      (5_OceanType1 , 15_GameSystem3, 60_GMod2)

#ifdef FeatTCPListen
   #define EthernetDeduction   104  
   //Ethernet takes this from RAM1 and another ~96k from RAM2 (when initialized/enabled), plus uses more local variables (see below)
   // Total: ~200k of RAM needed to support Ethernet, 100k if disabled

   // Test case: USB monitor *not* connected (passes when connected) crashes on startup, before full image handoff
   //   RAM1: variables:344292, code:136168, padding:27672   free for local variables:16156 <--not enough, crash
   //   RAM1: variables:340196, code:136168, padding:27672   free for local variables:20252 <--Fails intermittently?
   //   RAM1: variables:336100, code:136424, padding:27416   free for local variables:24348 <--Reliable
   //    *Need >24000 RAM1 free for local

#else
   #define EthernetDeduction    0
#endif

#define MaxRAM_ImageSize  (392-8*Num8kSwapBuffers-EthernetDeduction)  // base minus space for 8k swap blocks and ethernet needs
   // need to leave ~15k of RAM1 "free for local variables" w/ Ethernet,  OK w/ <7k free w/o it

