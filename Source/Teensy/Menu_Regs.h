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

#define MaxItemDispLength  35
#define MaxItemsPerPage    19

enum IO1_Registers  //offset from 0xDE00
{
   //skipping 0: Used by many others and accessed on reset
   rwRegStatus         =  1 , // Indicates busy when waiting for FW to complete something
   rRegStrAvailable    =  2 , // Stream PRG: zero when inactive/complete 
   rRegStreamData      =  3 , // Stream PRG: next byte of data to transfer, auto increments when read
   wRegControl         =  4 , // RegCtlCommands: execute specific functions
   rRegPresence1       =  5 , // HW detect: 0x55
   rRegPresence2       =  6 , // HW detect: 0xAA
   rRegLastHourBCD     =  7 , // Last TOD: Hours read
   rRegLastMinBCD      =  8 , // Last TOD: Minutes read
   rRegLastSecBCD      =  9 , // Last TOD: Seconds read
   rWRegCurrMenuWAIT   = 10 , // enum RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItemOnPage  = 11 , // Item sel/info: (zero based) select Menu Item On Current Page for name, type, execution, etc
   rwRegCursorItemOnPg = 12 , // Item sel/info: (zero based) Highlighted/cursor Menu Item On Current Page
   rRegNumItemsOnPage  = 13 , // Item sel/info: num items on current menu page
   rwRegPageNumber     = 14 , // Item sel/info: (one based) current page number
   rRegNumPages        = 15 , // Item sel/info: total number of pages
   rRegItemTypePlusIOH = 16 , // Item sel/info: regItemTypes: type of item, bit 7 indicates there's an assigned IOHandler (from TR mem menu) 
   rwRegPwrUpDefaults  = 17 , // EEPROM stored: power up default reg, see RegPowerUpDefaultMasks
   rwRegTimezone       = 18 , // EEPROM stored: signed char for timezone: UTC +/-12 
   rwRegNextIOHndlr    = 19 , // EEPROM stored: Which IO handler will take over upone exit/execute/emulate
   rwRegSerialString   = 20 , // Write selected item (RegSerialStringSelect) to select/reset, then Serially read out until 0 read.
   wRegSearchLetterWAIT= 21 , // Put cursor on first menu item with letter written
   rRegSIDInitHi       = 22 , // SID Play Info: Init address Hi
   rRegSIDInitLo       = 23 , // SID Play Info: Init Address Lo
   rRegSIDPlayHi       = 24 , // SID Play Info: Play Address Hi
   rRegSIDPlayLo       = 25 , // SID Play Info: Play Address Lo
   rRegSIDDefSpeedHi   = 26 , // SID Play Info: CIA interrupt timer speed Hi
   rRegSIDDefSpeedLo   = 27 , // SID Play Info: CIA interrupt timer speed Lo
   wRegVid_TOD_Clks    = 28 , // C64/128 Video Standard and TOD clock frequencies
   wRegIRQ_ACK         = 29 , // IRQ Ack from C64 app
   rwRegIRQ_CMD        = 30 , // IRQ Command from TeensyROM
   rwRegCodeStartPage  = 31 , // TR Code Start page in C64 RAM
   rwRegCodeLastPage   = 32 , // TR Code last page used in C64 RAM

   // These are used for the MIDI2SID app, keep in synch or make separate handler
   StartSIDRegs        = 64 , // start of SID Regs, matching SID Reg order ($D400)
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

   IO1Size             = StartSIDRegs + 40, //last entry, sets size
};

enum RegIRQCommands       //rwRegIRQ_CMD, echoed to wRegIRQ_ACK
{
   ricmdNone           = 0, // no command, always 0 (init)
   ricmdAck1           = 1, // Ack1 response from C64 IRQ routine
   ricmdLaunch         = 2, // Launch app (set up before IRQ assert)
   ricmdSIDPause       = 3, // SID pause/play
};

enum RegSerialStringSelect // rwRegSerialString
{
   rsstItemName        = 0,  // Name of selected item
   rsstNextIOHndlrName = 1,  // IOHandler Name selected in rwRegNextIOHndlr
   rsstSerialStringBuf = 2,  // build SerialStringBuf prior to selecting
   rsstVersionNum      = 3,  // version string for main banner 
   rsstShortDirPath    = 4,  // printable current path
   rsstSIDInfo         = 5,  // Info on last SID loaded
   rsstMachineInfo     = 6,  // Info on current machine vid/TOD clk (set when SID loaded)
};

enum RegPowerUpDefaultMasks
{  //eepAdPwrUpDefaults, rwRegPwrUpDefaults
   rpudSIDPauseMask  = 0x01, // rwRegPwrUpDefaults bit 0, 1=SID music paused
   rpudNetTimeMask   = 0x02, // rwRegPwrUpDefaults bit 1, 1=synch net time
   rpudNFCEnabled    = 0x04, // rwRegPwrUpDefaults bit 2, 1=NFC Enabled
   rpudRWReadyDly    = 0x08, // rwRegPwrUpDefaults bit 3, 1=RW Ready Detection delayed
   rpudJoySpeedMask  = 0xf0, // rwRegPwrUpDefaults bits 4-7=Joystick2 speed setting
};

enum RegStatusTypes  //rwRegStatus, match StatusFunction order
{
   rsChangeMenu         = 0x00,
   rsStartItem          = 0x01,
   rsGetTime            = 0x02,
   rsIOHWinit           = 0x03, //C64 code is executing transfered PRG, change IO1 handler
   rsWriteEEPROM        = 0x04,
   rsMakeBuildCPUInfoStr= 0x05,
   rsUpDirectory        = 0x06,
   rsSearchForLetter    = 0x07,
   rsLoadSIDforXfer     = 0x08,
   rsNextPicture        = 0x09,
   rsLastPicture        = 0x0a,
   
   rsNumStatusTypes     = 0x0b,

   rsReady              = 0x5a, //FW->64 (Rd) update finished (done, abort, or otherwise)
   rsC64Message         = 0xa5, //FW->64 (Rd) message for the C64, set to continue when finished
   rsContinue           = 0xc3, //64->FW (Wr) Tells the FW to continue with update

};

enum RegMenuTypes //must match TblMsgMenuName order/qty
{
   rmtSD        = 0,
   rmtTeensy    = 1,
   rmtUSBDrive  = 2,
};

enum RegCtlCommands
{
   rCtlVanishROM          =  0,
   rCtlBasicReset         =  1,
   rCtlStartSelItemWAIT   =  2,
   rCtlGetTimeWAIT        =  3,
   rCtlRunningPRG         =  4, // final signal before running prg, allows IO1 handler change
   rCtlMakeInfoStrWAIT    =  5, // MakeBuildCPUInfoStr
   rCtlUpDirectoryWAIT    =  6,
   rCtlLoadSIDWAIT        =  7, //load .sid file to RAM buffer and prep for x-fer
   rCtlNextPicture        =  8, 
   rCtlLastPicture        =  9, 
   rCtlRebootTeensyROM    = 10, 
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
   rtFileSID   = 11,
   rtFileKla   = 12,
   rtFileArt   = 13,

   //127 max, bit 7 used to indicate assigned IOH to TR
};

//   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  End C64 matching  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


struct StructMenuItem
{
  unsigned char ItemType;       //1  regItemTypes 
  uint8_t IOHndlrAssoc;         //1  enumIOHandlers (Teensy Mem Menu only)
  char *Name;                   //4
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
   IOH_C64GameSystem3,
   IOH_EasyFlash,
   IOH_ZaxxonSuper,
   
   
   IOH_Num_Handlers       //always last
};

#define LastSelectableIOH  IOH_Debug     //127 max
