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

//Known/public Cartridge HW types:

#define Cart_Generic                  0  //Supported
#define Cart_ActionReplay             1
#define Cart_KCSPowerCartridge        2
#define Cart_FinalCartridgeIII        3
#define Cart_SimonsBASIC              4
#define Cart_Oceantype1               5  //Supported
#define Cart_ExpertCartridge          6
#define Cart_FunPlayPowerPlay         7  //Supported
#define Cart_SuperGames               8  //Supported
#define Cart_AtomicPower              9
#define Cart_EpyxFastload            10  //Supported
#define Cart_WestermannLearning      11
#define Cart_RexUtility              12
#define Cart_FinalCartridgeI         13
#define Cart_MagicFormel             14
#define Cart_C64GameSystem3          15  //Supported
#define Cart_WarpSpeed               16
#define Cart_Dinamic                 17  //Supported
#define Cart_ZaxxonSuper             18  //Supported
#define Cart_MagicDesk               19  //Supported
#define Cart_SuperSnapshotV5         20
#define Cart_Comal80                 21
#define Cart_StructuredBASIC         22
#define Cart_Ross                    23
#define Cart_DelaEP64                24
#define Cart_DelaEP7x8               25
#define Cart_DelaEP256               26
#define Cart_RexEP256                27
#define Cart_MikroAssembler          28
#define Cart_FinalCartridgePlus      29
#define Cart_ActionReplay4           30
#define Cart_Stardos                 31
#define Cart_EasyFlash               32  //Supported (file size limit and no EAPI support)
#define Cart_EasyFlashXbank          33
#define Cart_Capture                 34
#define Cart_ActionReplay3           35
#define Cart_RetroReplay             36
#define Cart_MMC64                   37
#define Cart_MMCReplay               38
#define Cart_IDE64                   39
#define Cart_SuperSnapshotV4         40
#define Cart_IEEE488                 41
#define Cart_GameKiller              42
#define Cart_Prophet64               43
#define Cart_EXOS                    44
#define Cart_FreezeFrame             45
#define Cart_FreezeMachine           46
#define Cart_Snapshot64              47
#define Cart_SuperExplodeV50         48
#define Cart_MagicVoice              49
#define Cart_ActionReplay2           50
#define Cart_MACH5                   51
#define Cart_DiashowMaker            52
#define Cart_Pagefox                 53
#define Cart_Kingsoft                54
#define Cart_Silverrock128K          55
#define Cart_Formel64                56
#define Cart_RGCD                    57
#define Cart_RRNetMK3                58
#define Cart_EasyCalc                59
#define Cart_GMod2                   60
#define Cart_MAXBasic                61
#define Cart_GMod3                   62
#define Cart_ZIPPCODE48              63
#define Cart_BlackboxV8              64
#define Cart_BlackboxV3              65
#define Cart_BlackboxV4              66
#define Cart_REXRAMFloppy            67
#define Cart_BISPlus                 68
#define Cart_SDBOX                   69
#define Cart_MultiMAX                70
#define Cart_BlackboxV9              71
#define Cart_LtKernalHostAdaptor     72
#define Cart_RAMLink                 73
#define Cart_HERO                    74
#define Cart_IEEEFlash64             75
#define Cart_TurtleGraphicsII        76
#define Cart_FreezeFrameMK2          77
#define Cart_Partner64               78

// IO handlers only
#define Cart_DigiMax               -100 
#define Cart_DQBB                  -101 
#define Cart_GeoRAM                -102 
#define Cart_ISEPIC                -103 
#define Cart_RAMcart               -104 
#define Cart_REU                   -105 
#define Cart_SFX_Sound_Expander    -106 
#define Cart_SFX_Sound_Sampler     -107 
#define Cart_MIDI_Passport         -108  //Supported
#define Cart_MIDI_Datel            -109  //Supported
#define Cart_MIDI_Sequential       -110  //Supported
#define Cart_MIDI_Namesoft         -111  //Supported
#define Cart_MIDI_Maplin           -112
#define Cart_DS12C887RTC           -113
#define Cart_TFE                   -116
#define Cart_Turbo232              -117
#define Cart_SwiftLink             -118  //Supported
#define Cart_ACIA                  -119
#define Cart_Plus60K               -120
#define Cart_Plus256K              -121
#define Cart_C64_256K              -122
#define Cart_CPM                   -123
#define Cart_DebugCart             -124


struct StructHWID_IOH_Assoc
{ 
   uint16_t HWID;
   uint8_t  IOH;
};

StructHWID_IOH_Assoc HWID_IOH_Assoc[]=
{
           //HWID                  IOH
   (uint16_t)Cart_MIDI_Datel,      IOH_MIDI_Datel,
   (uint16_t)Cart_MIDI_Sequential, IOH_MIDI_Sequential,
   (uint16_t)Cart_MIDI_Passport,   IOH_MIDI_Passport,
   (uint16_t)Cart_MIDI_Namesoft,   IOH_MIDI_NamesoftIRQ,
   (uint16_t)Cart_SwiftLink,       IOH_Swiftlink,
   (uint16_t)Cart_EpyxFastload,    IOH_EpyxFastLoad,
   (uint16_t)Cart_MagicDesk,       IOH_MagicDesk,
   (uint16_t)Cart_Dinamic,         IOH_Dinamic,
   (uint16_t)Cart_Oceantype1,      IOH_Ocean1,
   (uint16_t)Cart_FunPlayPowerPlay,IOH_FunPlay,
   (uint16_t)Cart_SuperGames      ,IOH_SuperGames,
   (uint16_t)Cart_C64GameSystem3  ,IOH_C64GameSystem3,
   (uint16_t)Cart_EasyFlash       ,IOH_EasyFlash,
   (uint16_t)Cart_ZaxxonSuper     ,IOH_ZaxxonSuper,
   
};

#define StrSIDInfoSize    (5*40+5) // max 5 *full* lines + 1 blank line

#define CRT_MAIN_HDR_LEN  0x40
#define CRT_CHIP_HDR_LEN  0x10
#define MAX_CRT_CHIPS     128

struct StructCrtChip
{
   uint8_t *ChipROM;
   uint16_t LoadAddress;
   uint16_t ROMSize;
   uint16_t BankNum;
};

struct StructExt_ItemType_Assoc
{ 
   char    Extension[4];
   uint8_t ItemType;
};

StructExt_ItemType_Assoc Ext_ItemType_Assoc[]=
{
  //Ext ,  ItemType
   "prg",  rtFilePrg,
   "crt",  rtFileCrt,
   "hex",  rtFileHex,
   "p00",  rtFileP00,
   "sid",  rtFileSID,
   //"c64",  rtFilePrg,  //makefile output, not always prg...
};

