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


//  !!!!!!!!!!!!!!!!!!!!These need to match C64 Code: MainMenu.asm !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#define MaxItemDispLength  32
#define MaxItemsPerPage    16

enum IO1_Registers  //offset from 0xDE00
{
   //skipping 0: Used by many others and accessed on reset
   rwRegStatus         =  1 , // Indicates busy when waiting for FW to complete something
   rRegStrAddrLo       =  2 , // Stream PRG: lo byte of start address of the prg file being transfered to mem
   rRegStrAddrHi       =  3 , // Stream PRG: Hi byte of start address
   rRegStrAvailable    =  4 , // Stream PRG: zero when inactive/complete 
   rRegStreamData      =  5 , // Stream PRG: next byte of data to transfer, auto increments when read
   wRegControl         =  6 , // RegCtlCommands: execute specific functions
   rRegPresence1       =  7 , // HW detect: 0x55
   rRegPresence2       =  8 , // HW detect: 0xAA
   rRegLastHourBCD     =  9 , // Last TOD: Hours read
   rRegLastMinBCD      = 10 , // Last TOD: Minutes read
   rRegLastSecBCD      = 11 , // Last TOD: Seconds read
   rWRegCurrMenuWAIT   = 12 , // enum RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItemOnPage  = 13 , // Item sel/info: (zero based) select Menu Item On Current Page for name, type, execution, etc
   rRegNumItemsOnPage  = 14 , // Item sel/info: num items on current menu page
   rwRegPageNumber     = 15 , // Item sel/info: (one based) current page number
   rRegNumPages        = 16 , // Item sel/info: total number of pages
   rRegItemTypePlusIOH = 17 , // Item sel/info: regItemTypes: type of item, bit 7 indicates there's an assigned IOHandler (from TR mem menu) 
   rwRegPwrUpDefaults  = 18 , // EEPROM stored: power up default reg, see RegPowerUpDefaultMasks
   rwRegTimezone       = 19 , // EEPROM stored: signed char for timezone: UTC +/-12 
   rwRegNextIOHndlr    = 20 , // EEPROM stored: Which IO handler will take over upone exit/execute/emulate
   rwRegSerialString   = 21 , // Write selected item (RegSerialStringSelect) to select/reset, then Serially read out until 0 read.

   StartSIDRegs        = 22, // start of SID Regs, matching SID Reg order ($D400)
   rRegSIDFreqLo1      = StartSIDRegs +  0, 
   rRegSIDFreqHi1      = StartSIDRegs +  1,
   rRegSIDDutyLo1      = StartSIDRegs +  2,
   rRegSIDDutyHi1      = StartSIDRegs +  3,
   rRegSIDVoicCont1    = StartSIDRegs +  4,
   rRegSIDAttDec1      = StartSIDRegs +  5,
   rRegSIDSusRel1      = StartSIDRegs +  6,
                                         
   rRegSIDFreqLo2      = StartSIDRegs +  7, 
   rRegSIDFreqHi2      = StartSIDRegs +  8,
   rRegSIDDutyLo2      = StartSIDRegs +  9,
   rRegSIDDutyHi2      = StartSIDRegs + 10,
   rRegSIDVoicCont2    = StartSIDRegs + 11,
   rRegSIDAttDec2      = StartSIDRegs + 12,
   rRegSIDSusRel2      = StartSIDRegs + 13,
                       
   rRegSIDFreqLo3      = StartSIDRegs + 14, 
   rRegSIDFreqHi3      = StartSIDRegs + 15,
   rRegSIDDutyLo3      = StartSIDRegs + 16,
   rRegSIDDutyHi3      = StartSIDRegs + 17,
   rRegSIDVoicCont3    = StartSIDRegs + 18,
   rRegSIDAttDec3      = StartSIDRegs + 19,
   rRegSIDSusRel3      = StartSIDRegs + 20,
                       
   rRegSIDFreqCutLo    = StartSIDRegs + 21,
   rRegSIDFreqCutHi    = StartSIDRegs + 22,
   rRegSIDFCtlReson    = StartSIDRegs + 23,
   rRegSIDVolFltSel    = StartSIDRegs + 24,
   EndSIDRegs          = StartSIDRegs + 25,
                       
   rRegSIDStrStart     = StartSIDRegs + 26,
   //  9: 3 chars per voice (oct, note, shrp)
   //  1: Out of voices indicator
   //  3: spaces betw
   // 14 total w// term:  ON# ON# ON# X
   rRegSIDOutOfVoices  = StartSIDRegs + 38,
   rRegSIDStringTerm   = StartSIDRegs + 39,

};

enum RegSerialStringSelect // rwRegSerialString
{
   rsstItemName        = 0,  // Name of selected item
   rsstNextIOHndlrName = 1,  // IOHandler Name selected in rwRegNextIOHndlr
   rsstSerialStringBuf = 2,  // build SerialStringBuf prior to selecting
   rsstVersionNum      = 3,     
};

enum RegPowerUpDefaultMasks
{
   rpudMusicMask     = 0x01, // rwRegPwrUpDefaults bit 0=music on
   rpudNetTimeMask   = 0x02, // rwRegPwrUpDefaults bit 1=synch net time
};

enum RegStatusTypes  //rwRegStatus, match StatusFunction order
{
   rsChangeMenu         = 0x00,
   rsStartItem          = 0x01,
   rsGetTime            = 0x02,
   rsIOHWinit           = 0x03, //C64 code is executing transfered PRG, change IO1 handler
   rsWriteEEPROM        = 0x04,
   rsMakeBuildCPUInfoStr= 0x05,
   
   rsNumStatusTypes     = 0x06,

   rsReady              = 0x5a, //FW->64 (Rd) update finished (done, abort, or otherwise)
   rsC64Message         = 0xa5, //FW->64 (Rd) message for the C64, set to continue when finished
   rsContinue           = 0xc3, //64->FW (Wr) Tells the FW to continue with update

};

enum RegMenuTypes //must match TblMsgMenuName order/qty
{
   rmtSD        = 0,
   rmtTeensy    = 1,
   rmtUSBHost   = 2,
   rmtUSBDrive  = 3,
};

enum RegCtlCommands
{
   rCtlVanishROM          = 0,
   rCtlBasicReset         = 1,
   rCtlStartSelItemWAIT   = 2,
   rCtlGetTimeWAIT        = 3,
   rCtlRunningPRG         = 4, // final signal before running prg, allows IO1 handler change
   rCtlMakeInfoStrWAIT    = 5, // MakeBuildCPUInfoStr
};

enum regItemTypes //synch with TblItemType
{
   rtNone      = 0,
   rtUnknown   = 1,
   rtBin16k    = 2,
   rtBin8kHi   = 3,
   rtBin8kLo   = 4,
   rtBinC128   = 5,
   rtDirectory = 6,
   //file extension matching:
   rtFilePrg   = 7,
   rtFileCrt   = 8,
   rtFileHex   = 9,
   rtFileP00   = 10,
   
   //127 max, bit 7 used to indicate assigned IOH to TR
};

//   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End C64 matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#define    MaxItemNameLength  76

struct StructMenuItem
{
  unsigned char ItemType;       //1  regItemTypes 
  uint8_t IOHndlrAssoc;         //1  enumIOHandlers
  char Name[MaxItemNameLength]; //76
  uint8_t *Code_Image;          //1
  uint32_t Size;                //4
};

enum enumIOHandlers //Synch order/qty with IOHandler[] (IOHandlers.h)
{
   IOH_None,
   IOH_Swiftlink,
   IOH_MIDI_Datel,      
   IOH_MIDI_Sequential, 
   IOH_MIDI_Passport,   
   IOH_MIDI_NamesoftIRQ,
   IOH_Debug, //last manually selectable, see LastSelectableIOH
   
   IOH_TeensyROM, 
   IOH_EpyxFastLoad,
   IOH_MagicDesk,
   IOH_Dinamic,
   IOH_Ocean1,
   IOH_FunPlay,
   IOH_SuperGames,
   
   IOH_Num_Handlers       //always last
};

#define LastSelectableIOH  IOH_Debug
