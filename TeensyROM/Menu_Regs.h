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


//  !!!!!!!!!!!!!!!!!!!!These need to match C64 Code: MainMenu_C000.asm !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define MaxItemNameLength 28

enum IO1_Registers  //offset from 0xDE00
{
   rRegStatus        =  0 , //Busy when doing SD/USB access.  note: loc 0(DE00) gets written to at reset
   rRegStrAddrLo     =  1 , //lo byte of start address of the prg file being transfered to mem
   rRegStrAddrHi     =  2 , //Hi byte of start address
   rRegStrAvailable  =  3 , //zero when inactive/complete 
   rRegStreamData    =  4 , //next byte of data to transfer, auto increments when read
   wRegControl       =  5 , //RegCtlCommands: execute specific functions
   rRegPresence1     =  6 , //for HW detect: 0x55
   rRegPresence2     =  7 , //for HW detect: 0xAA
   rRegLastHourBCD   =  8 , //Last TOD Hours read
   rRegLastMinBCD    =  9 , //Last TOD Minutes read
   rRegLastSecBCD    = 10 , //Last TOD Seconds read
   rWRegCurrMenuWAIT = 11 , //RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItem      = 12 , //select Menu Item for name, type, execution, etc
   rRegNumItems      = 13 , //num items in menu list
   rRegItemType      = 14 , //regItemTypes: type of item 
   rRegItemNameStart = 15 , //MaxItemNameLength bytes long (incl term)
   rRegItemNameTerm  = rRegItemNameStart + MaxItemNameLength,
   StartSIDRegs      = rRegItemNameTerm+1 , //start of SID Regs, matching SID Reg order ($D400)
   rRegSIDFreqLo1    = StartSIDRegs +  0, 
   rRegSIDFreqHi1    = StartSIDRegs +  1,
   rRegSIDDutyLo1    = StartSIDRegs +  2,
   rRegSIDDutyHi1    = StartSIDRegs +  3,
   rRegSIDVoicCont1  = StartSIDRegs +  4,
   rRegSIDAttDec1    = StartSIDRegs +  5,
   rRegSIDSusRel1    = StartSIDRegs +  6,
                                       
   rRegSIDFreqLo2    = StartSIDRegs +  7, 
   rRegSIDFreqHi2    = StartSIDRegs +  8,
   rRegSIDDutyLo2    = StartSIDRegs +  9,
   rRegSIDDutyHi2    = StartSIDRegs + 10,
   rRegSIDVoicCont2  = StartSIDRegs + 11,
   rRegSIDAttDec2    = StartSIDRegs + 12,
   rRegSIDSusRel2    = StartSIDRegs + 13,

   rRegSIDFreqLo3    = StartSIDRegs + 14, 
   rRegSIDFreqHi3    = StartSIDRegs + 15,
   rRegSIDDutyLo3    = StartSIDRegs + 16,
   rRegSIDDutyHi3    = StartSIDRegs + 17,
   rRegSIDVoicCont3  = StartSIDRegs + 18,
   rRegSIDAttDec3    = StartSIDRegs + 19,
   rRegSIDSusRel3    = StartSIDRegs + 20,

   rRegSIDFreqCutLo  = StartSIDRegs + 21,
   rRegSIDFreqCutHi  = StartSIDRegs + 22,
   rRegSIDFCtlReson  = StartSIDRegs + 23,
   rRegSIDVolFltSel  = StartSIDRegs + 24,
   EndSIDRegs        = StartSIDRegs + 25,
   
   rRegSIDStrStart   = StartSIDRegs + 26,
   //  9: 3 chars per voice (oct, note, shrp)
   //  1: Out of voices indicator
   //  3: spaces betw
   // 14 total w// term:  ON# ON# ON# X
   rRegSIDOutOfVoices= StartSIDRegs + 38,
   rRegSIDStringTerm = StartSIDRegs + 39,
   
};

enum RegStatusTypes
{
   rsReady      = 0x5a,
   rsChangeMenu = 0x9d,
   rsStartItem  = 0xb1,
   rsGetTime    = 0xe6,
   //rsError      = 0x24,
};

enum RegMenuTypes
{
   rmtSD        = 0,
   rmtTeensy    = 1,
   rmtUSBHost   = 2,
   rmtUSBDrive  = 3,
};

enum RegCtlCommands
{
   rCtlVanish           = 0,
   rCtlVanishReset      = 1,
   rCtlStartSelItemWAIT = 2,
   rCtlGetTimeWAIT      = 3,
};

enum regItemTypes
{
   rtNone = 0,
   rt16k  = 1,
   rt8kHi = 2,
   rt8kLo = 3,
   rtPrg  = 4,
   rtUnk  = 5,
   rtCrt  = 6,
   rtDir  = 7,
};

//   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End C64 matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

struct StructMenuItem
{
  unsigned char ItemType;
  char Name[MaxItemNameLength];
  const unsigned char *Code_Image;
  uint16_t Size;
};

