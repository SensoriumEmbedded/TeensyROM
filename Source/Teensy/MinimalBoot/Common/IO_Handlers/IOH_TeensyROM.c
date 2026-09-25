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


//IO Handler for TeensyROM

void IO1Hndlr_TeensyROM(uint8_t Address, bool R_Wn);
void IO2Hndlr_TeensyROM(uint8_t Address, bool R_Wn);
void PollingHndlr_TeensyROM();
void InitHndlr_TeensyROM();

stcIOHandlers IOHndlr_TeensyROM =
{
  "TeensyROM",              //Name of handler
  &InitHndlr_TeensyROM,     //Called once at handler startup
  &IO1Hndlr_TeensyROM,      //IO1 R/W handler
  &IO2Hndlr_TeensyROM,      //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_TeensyROM,  //Polled in main routine
  NULL,                     //called at the end of EVERY c64 cycle
};

int16_t SidSpeedAdjust = 0;
bool    SidLogConv = false; //true=Log, false=linear
volatile uint8_t* IO1;  //io1 space/regs
//StreamOffsetAddr indexes XferImage/RAM_Image, both sized by uint32_t XferSize, and the
//end-of-transfer tests are "++StreamOffsetAddr >= XferSize".  As a uint16_t it wrapped
//before it could reach any XferSize >= 65536, so those tests never fired and the C64 was
//never told the transfer had ended -- reachable with an ordinary >64KiB .txt/.seq, which
//LoadFile admits up to RAM_ImageSize (128KiB).  StringOffset stays 16-bit: it indexes
//strings bounded by MaxPathLength, and widening it would only cost DTCM.
volatile uint32_t StreamOffsetAddr = 0;
volatile uint16_t StringOffset = 0;
volatile char*    ptrSerialString; //pointer to selected serialstring
char SerialStringBuf[MaxPathLength+6] = "err"; // used for message passing to C64, up to full path length
volatile uint8_t doReset = true;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;
volatile uint8_t eepDataToWrite;
volatile uint16_t eepAddrToWrite;
StructMenuItem *MenuSource;
uint16_t SelItemFullIdx = 0;  //logical full index into menu for selected item
uint16_t NumItemsFull;  //Num Items in Current Menu
uint8_t *XferImage = NULL; //pointer to image being transferred to C64
uint32_t XferSize = 0;  //size of image being transferred to C64
bool NetListenEnable = false;

//Both halves of a menu index come from the C64: the item byte it just wrote, and
//rwRegPageNumber, which is stored raw.  Their product was never compared against
//NumItemsFull, and page 0 makes it negative -- which this uint16_t turns into ~65500.
//MenuSource[] elements carry two pointers, and one of the dereference sites is inside
//isrPHI2, so an out-of-range index is a wild read, not a wrong menu entry.  Clamp where
//the index is formed rather than at each use, and stay silent: ISR context cannot print.
//Returning 0 is only safe because every loader leaves at least one item: DriveDirLoad.ino
//substitutes an "<Empty>" entry when a directory scan finds none, and LoadDxxDirectory
//(D64.ino) adds the up-directory entry before any early return, including its error
//paths.  Those two are the invariant -- RedirectEmptyDriveDirMenu only covers
//DriveDirMenu being NULL, which is a different condition.  If a future loader can leave
//NumItemsFull at 0, the zero here becomes an out-of-range index and needs revisiting.
//There is deliberately no NumItemsFull == 0 test below: with NumItemsFull 0 the third
//term already catches every Idx >= 0 and the first catches every Idx < 0, so such a test
//cannot change the result for any input, and returning 0 for an empty menu would be out
//of range anyway.  The callers that cannot survive an empty menu guard themselves --
//see Next/LastFileType in StatusFunctions.c.
inline uint16_t MenuIdxFromRegs(uint8_t ItemOnPage)
{
   int32_t Idx = (int32_t)ItemOnPage + ((int32_t)IO1[rwRegPageNumber] - 1) * MaxItemsPerPage;
   if (Idx < 0 || Idx >= (int32_t)NumItemsFull) return 0;
   return (uint16_t)Idx;
}

//Clamping the index where it is formed is not the same as clamping it where it is used, and
//the two are separated by whatever the C64 does in between.  The bound is a third C64-controlled
//input alongside the item byte and the page byte: one write to rWRegCurrMenuWAIT swaps the menu,
//and both NumItemsFull and MenuSource change while a formed SelItemFullIdx stays where it was.
//Nothing re-checked it, so a 15-item menu's index 14 survived into a 10-item one and got
//dereferenced -- twice from inside isrPHI2, where MenuSource[] elements hand out two pointers.
//So re-check at the dereference, against the count in force now.  Answer NULL rather than
//substituting item 0: a caller that cannot tell "out of range" from "the first item" acts on the
//wrong file and says nothing.  SetMenu() (Teensy.ino) keeps the base and the count consistent
//for this read.  Callers that form the index immediately above their own dereference are already
//in range by MenuIdxFromRegs' construction and are left alone.
inline StructMenuItem* MenuItemSel()
{
   const uint16_t Idx = SelItemFullIdx;   //one read: the ISR writes this too
   if (MenuSource == NULL || Idx >= NumItemsFull) return NULL;  //covers NumItemsFull==0
   return &MenuSource[Idx];
}

uint8_t ASCIItoPETSCII[128]=
{
 /*   ASCII   */  //PETSCII
 /*   0 'null'*/    0,
 /*   1 '' */    0,
 /*   2 '' */    0,
 /*   3 '' */    0,
 /*   4 '' */    0,
 /*   5 '' */    0,
 /*   6 '' */    0,
 /*   7 '' */    0,
 /*   8 ''  */    0,
 /*   9 '\t'  */   32, // tab -> space
 /*  10 '\n'  */   10, //let this slide: won't print anything in petscii, but maybe if prog is in terminal mode?
 /*  11 ''  */    0,
 /*  12 ''  */    0,
 /*  13 '\r'  */   13,
 /*  14 ''  */    0,
 /*  15 ''  */    0,
 /*  16 '' */    0,
 /*  17 '' */    0,
 /*  18 '' */    0,
 /*  19 '' */    0,
 /*  20 '' */    0,
 /*  21 '' */    0,
 /*  22 '' */    0,
 /*  23 '' */    0,
 /*  24 '' */    0,
 /*  25 ''  */    0,
 /*  26 '' */    0,
 /*  27 '' */    0,
 /*  28 ''  */    0,
 /*  29 ''  */    0,
 /*  30 ''  */    0,
 /*  31 ''  */    0,
 /*  32 ' '   */   32,
 /*  33 '!'   */   33,
 /*  34 '"'   */   34,
 /*  35 '#'   */   35,
 /*  36 '$'   */   36,
 /*  37 '%'   */   37,
 /*  38 '&'   */   38,
 /*  39 '''   */   39,
 /*  40 '('   */   40,
 /*  41 ')'   */   41,
 /*  42 '*'   */   42,
 /*  43 '+'   */   43,
 /*  44 ','   */   44,
 /*  45 '-'   */   45,
 /*  46 '.'   */   46,
 /*  47 '/'   */   47,
 /*  48 '0'   */   48,
 /*  49 '1'   */   49,
 /*  50 '2'   */   50,
 /*  51 '3'   */   51,
 /*  52 '4'   */   52,
 /*  53 '5'   */   53,
 /*  54 '6'   */   54,
 /*  55 '7'   */   55,
 /*  56 '8'   */   56,
 /*  57 '9'   */   57,
 /*  58 ':'   */   58,
 /*  59 ';'   */   59,
 /*  60 '<'   */   60,
 /*  61 '='   */   61,
 /*  62 '>'   */   62,
 /*  63 '?'   */   63,
 /*  64 '@'   */   64,
 /*  65 'A'   */   97,
 /*  66 'B'   */   98,
 /*  67 'C'   */   99,
 /*  68 'D'   */  100,
 /*  69 'E'   */  101,
 /*  70 'F'   */  102,
 /*  71 'G'   */  103,
 /*  72 'H'   */  104,
 /*  73 'I'   */  105,
 /*  74 'J'   */  106,
 /*  75 'K'   */  107,
 /*  76 'L'   */  108,
 /*  77 'M'   */  109,
 /*  78 'N'   */  110,
 /*  79 'O'   */  111,
 /*  80 'P'   */  112,
 /*  81 'Q'   */  113,
 /*  82 'R'   */  114,
 /*  83 'S'   */  115,
 /*  84 'T'   */  116,
 /*  85 'U'   */  117,
 /*  86 'V'   */  118,
 /*  87 'W'   */  119,
 /*  88 'X'   */  120,
 /*  89 'Y'   */  121,
 /*  90 'Z'   */  122,
 /*  91 '['   */   91,
 /*  92 '\'   */   47, // backslash    -> forward slash
 /*  93 ']'   */   93,
 /*  94 '^'   */   94, // caret        -> up arrow
 /*  95 '_'   */  164, // underscore   -> bot line graphic char
 /*  96 '`'   */   39, // grave accent -> single quote
 /*  97 'a'   */   65,
 /*  98 'b'   */   66,
 /*  99 'c'   */   67,
 /* 100 'd'   */   68,
 /* 101 'e'   */   69,
 /* 102 'f'   */   70,
 /* 103 'g'   */   71,
 /* 104 'h'   */   72,
 /* 105 'i'   */   73,
 /* 106 'j'   */   74,
 /* 107 'k'   */   75,
 /* 108 'l'   */   76,
 /* 109 'm'   */   77,
 /* 110 'n'   */   78,
 /* 111 'o'   */   79,
 /* 112 'p'   */   80,
 /* 113 'q'   */   81,
 /* 114 'r'   */   82,
 /* 115 's'   */   83,
 /* 116 't'   */   84,
 /* 117 'u'   */   85,
 /* 118 'v'   */   86,
 /* 119 'w'   */   87,
 /* 120 'x'   */   88,
 /* 121 'y'   */   89,
 /* 122 'z'   */   90,
 /* 123 '{'   */  179, // left curly brace  ->   -|   graphic char
 /* 124 '|'   */  125, // pipe              ->    |   graphic char
 /* 125 '}'   */  171, // right curly brace ->    |-  graphic char
 /* 126 '~'   */  178, // tilde             ->   Half height T graphic char
 /* 127 '' */   95, // delete            ->   left arrow
};

extern bool EthernetInit(void (*MsgOut)(const char *));
extern bool ForceEthernetInit(void (*MsgOut)(const char *));
extern void MenuChange();
extern void HandleExecution();
extern bool PathIsRoot();
extern void LoadDirectory(FS *sourceFS);
extern void FreeDriveDirMenu();
extern void RedirectEmptyDriveDirMenu();
extern void IOHandlerSelectInit();
extern void IOHandlerNextInit();
extern void ParseSIDHeader(const char *filename);
extern stcIOHandlers* IOHandler[];
extern char DriveDirPath[];
extern uint8_t RAM_Image[];
extern char* StrSIDInfo;
extern char* LatestSIDLoaded;
extern char StrMachineInfo[];
extern uint8_t nfcState;
extern bool SendMsgPrintfln(const char *Fmt, ...);
extern bool SendMsgPrintf(const char *Fmt, ...);
extern void nfcWriteTag(const char* TxtMsg);
extern void nfcInit();
extern void EEPreadNBuf(uint16_t addr, uint8_t* buf, uint16_t len);
extern void EEPwriteNBuf(uint16_t addr, const uint8_t* buf, uint16_t len);
extern void EEPwriteStr(uint16_t addr, const char* buf);
extern bool LoadFile(FS *sourceFS, const char* FilePath, StructMenuItem* MyMenuItem);
extern bool SDFullInit();
extern bool USBFileSystemWait();
extern void MountDxxFile();
extern void EEPRemoteLaunch(uint16_t eepAdNameToLaunch);
extern volatile uint8_t BtnPressed;
extern void EEPreadStr(uint16_t addr, char* buf);
extern Bounce SpecialBtnBounce;

#define DecToBCD(d) ((int((d)/10)<<4) | ((d)%10))

//#define ToPETSCII(x) (x==95 ? 32 : x>64 ? x^32 : x)
#define ToPETSCII(x) ASCIItoPETSCII[(x) & 0x7f]

FLASHMEM uint8_t RAM2blocks()
{  //see how many 8k banks will fit in RAM2
   char *ptrChip[70]; //64 8k blocks would be 512k (size of RAM2)
   uint8_t ChipNum = 0;
   while(1)
   {
      ptrChip[ChipNum] = (char *)malloc(8192);
      if (ptrChip[ChipNum] == NULL) break;
      ChipNum++;
   }
   for(uint8_t Cnt=0; Cnt < ChipNum; Cnt++) free(ptrChip[Cnt]);
   //Serial.printf("Created/freed %d  8k blocks (%dk total) in RAM2\n", ChipNum, ChipNum*8);
   return ChipNum;
}

//FLASHMEM void MakeBuildCPUInfoStr()
//{
//   FreeDriveDirMenu(); //Will mess up navigation if not on TR menu!
//   RedirectEmptyDriveDirMenu(); //OK since we're on the TR settings screen
//
//   uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
//   //Serial.printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);
//
//   uint8_t NumChips = RAM2blocks();
//   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
//   NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
//   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
//
//   CrtMax += NumChips*8;
//   char FreeStr[20];
//   sprintf(FreeStr, "  %luk free\r", (uint32_t)(CrtMax*1.004));  //larger File size due to header info.
//
//   MakeBuildInfo();
//   strcat(SerialStringBuf, FreeStr);
//}

bool SetSIDSpeed(bool LogConv, int16_t PlaybackSpeedIn)
{  //called from IO handler, must be quick...
   float PlaybackSpeedPct = PlaybackSpeedIn; //number from -128*256 to 127*256
   PlaybackSpeedPct = PlaybackSpeedPct/256/100;

   int32_t SIDSpeed = IO1[rRegSIDDefSpeedLo]+256*IO1[rRegSIDDefSpeedHi]; //start with default value

   if (LogConv) SIDSpeed -= SIDSpeed*PlaybackSpeedPct;
   else SIDSpeed = SIDSpeed/(PlaybackSpeedPct+1);

   //Printf_dbg("SID Speed: %+0.2f\nReg val 0x%04x\n", PlaybackSpeedPct*100, SIDSpeed);
   if(SIDSpeed > 0xffff || SIDSpeed < 1)
   {
      //Printf_dbg("Out of reg range (0001 to ffff)\n");
      return false;
   }

   IO1[rwRegSIDCurSpeedLo] = SIDSpeed & 0xff;
   IO1[rwRegSIDCurSpeedHi] = (SIDSpeed>>8) & 0xff;
   SidSpeedAdjust = PlaybackSpeedIn; //update C64 side setting
   SidLogConv = LogConv; //in case of remote change
   return true;
}

FLASHMEM void GetCurrentFilePathName(char* FilePathName, size_t Size)
{
   const StructMenuItem* Item = MenuItemSel();
   if (Item == NULL || Item->Name == NULL)
   {  //Callers print this buffer and several then write it to EEPROM as a lasting file
      //reference, so a path built from a stale selection has to be a visible refusal rather
      //than a plausible name.  Same shape as the "TR:Dir not found" exit below.
      snprintf(FilePathName, Size, "Sel out of range");
      return;
   }

   char *LclFilename = Item->Name;
   char Rand[] = "?";

   if (IO1[rwRegScratch]) LclFilename = Rand; //random dir

   if (IO1[rWRegCurrMenuWAIT] == rmtTeensy)
   {
      //figure out what menu dir we're in
      char DirName[45] = "/";

      if (MenuSource != TeensyROMMenu)
      {
         //find sub-dir
         uint8_t DirNum = 0;
         while(MenuSource != (StructMenuItem*)TeensyROMMenu[DirNum].Code_Image)
         {
            //MenuSelCpy.Code_Image;
            if (++DirNum == sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]))
            {
               Printf_dbg("TR Dir not found\n"); //what now?
               snprintf(FilePathName, Size, "TR:Dir not found");
               return;
            }
         }
         strcpy(DirName, TeensyROMMenu[DirNum].Name);
      }

      snprintf(FilePathName, Size, "TR:%s/%s", DirName, LclFilename);
   }
   else
   {
      char SDUSB[6] = "SD";
      if (IO1[rWRegCurrMenuWAIT] == rmtUSBDrive) strcpy(SDUSB, "USB");

      if (PathIsRoot()) snprintf(FilePathName, Size, "%s:/%s", SDUSB, LclFilename);
      else snprintf(FilePathName, Size, "%s:%s/%s", SDUSB, DriveDirPath, LclFilename);
   }
}

FLASHMEM int16_t FindTRMenuItem(StructMenuItem* MyMenu, uint16_t NumEntries, char* EntryName)
{
   for(uint16_t EntryNum=0; EntryNum < NumEntries; EntryNum++)
   {
      if(strcmp(MyMenu[EntryNum].Name, EntryName) == 0) return EntryNum;
   }
   return -1;
}

bool HandshakeSnoop(uint16_t Address, bool R_Wn)
{  //PRG-load IO handler swap handshake: lets C64 poll a fixed reg instead of a fixed delay.
   //Installed by the rCtlRunningPRG case below; runs ahead of CurrentIOHandler's own dispatch
   //(and works regardless of which handler is current), so it stays valid across the swap.
   if (!R_Wn || Address != 0xde00+rRegIOHSwapPoll) return false; //not ours, continue normal dispatch

   if (!HandshakeReady)
   {
      DataPortWriteWait(rihsBusy);
      return true;
   }
   DataPortWriteWait(rihsReady);
   fBusSnoop = PendingfBusSnoop; //hand off to whatever the new IO handler staged (or NULL), atomically with this read
   return true;
}

#include "StatusFunctions.c"

//MIDI input/voice handlers for MIDI2SID _________________________________________________________________________

#define NUM_VOICES 3
const char NoteName[12][3] ={" a","a#"," b"," c","c#"," d","d#"," e"," f","f#"," g","g#"};

struct stcVoiceInfo
{
  bool Available;
  uint16_t  NoteNumUsing;
};

stcVoiceInfo Voice[NUM_VOICES]=
{  //voice table for poly synth
   true, 0,
   true, 0,
   true, 0,
};

int FindVoiceUsingNote(int NoteNum)
{
  for (int VoiceNum=0; VoiceNum<NUM_VOICES; VoiceNum++)
  {
    if(Voice[VoiceNum].NoteNumUsing == NoteNum && !Voice[VoiceNum].Available) return (VoiceNum);
  }
  return (-1);
}

int FindFreeVoice()
{
  for (int VoiceNum=0; VoiceNum<NUM_VOICES; VoiceNum++)
  {
    if(Voice[VoiceNum].Available) return (VoiceNum);
  }
  return (-1);
}

void M2SOnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
   note+=3; //offset to A centered from C
   int VoiceNum = FindFreeVoice();
   if (VoiceNum<0)
   {
      IO1[rRegSIDOutOfVoices]='x';
      #ifdef DbgMsgs_M2S
       Serial.println("Out of Voices!");
      #endif
      return;
   }

   // https://ilmilou.uk/NoteFreqCalcs
   // 2^(1/12) = 1.059463094359
   float Frequency = 440*pow(1.059463094359,note-60);
   // https://codebase64.org/doku.php?id=base:how_to_calculate_your_own_sid_frequency_table
   // 256^3 = 16777216
   // IO1[wRegVid_TOD_Clks] & rvtcNTSC  //1=NTSC, 0=PAL
   uint32_t RegVal = Frequency*16777216/((IO1[wRegVid_TOD_Clks] & rvtcNTSC) ? NTSCBusFreq : PALBusFreq);

   if (RegVal > 0xffff)
   {
      #ifdef DbgMsgs_M2S
       Serial.println("Too high!");
      #endif
      return;
   }

   Voice[VoiceNum].Available = false;
   Voice[VoiceNum].NoteNumUsing = note;

   IO1[rRegSIDFreqLo1+VoiceNum*7] = RegVal;  //7 regs per voice
   IO1[rRegSIDFreqHi1+VoiceNum*7] = (RegVal>>8);
   //IO1[rRegSIDSusRel1+VoiceNum*7] = (IO1[rRegSIDSusRel1+VoiceNum*7] & 0x0f) | ((velocity<<1) & 0xf0); //Set Sustain level (0-15) from velocity (0-127)
   IO1[rRegSIDVoicCont1+VoiceNum*7] |= 0x01; //start ADSR

   IO1[rRegSIDStrStart+VoiceNum*4+0]=NoteName[note%12][0];
   IO1[rRegSIDStrStart+VoiceNum*4+1]=NoteName[note%12][1];
   IO1[rRegSIDStrStart+VoiceNum*4+2]='0'+note/12;

   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Note On, ch=");
    Serial.print(channel);
    Serial.print(", voice=");
    Serial.print(VoiceNum);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.print(", reg ");
    Serial.print(IO1[rRegSIDFreqHi1  ]);
    Serial.print(":");
    Serial.print(IO1[rRegSIDFreqLo1  ]);
    Serial.println();
   #endif
}

void M2SOnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
   note+=3; //offset to A centered from C
   IO1[rRegSIDOutOfVoices]=' ';
   int VoiceNum = FindVoiceUsingNote(note);

   if (VoiceNum<0)
   {
      #ifdef DbgMsgs_M2S
       Serial.print("No voice using note ");
       Serial.println(note);
      #endif
      return;
   }
   Voice[VoiceNum].Available = true;
   IO1[rRegSIDVoicCont1+VoiceNum*7] &= 0xFE; //stop note
   IO1[rRegSIDStrStart+VoiceNum*4+0]='-';
   IO1[rRegSIDStrStart+VoiceNum*4+1]='-';
   IO1[rRegSIDStrStart+VoiceNum*4+2]=' ';

   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Note Off, ch=");
    Serial.print(channel);
    Serial.print(", voice=");
    Serial.print(VoiceNum);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.println();
   #endif
}

void M2SOnControlChange(uint8_t channel, uint8_t control, uint8_t value)
{

   #ifdef DbgMsgs_M2S
    Serial.print("MIDI Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", value=");
    Serial.print(value);
    Serial.println();
   #endif
}

void M2SOnPitchChange(uint8_t channel, int pitch)
{

   #ifdef DbgMsgs_M2S
    Serial.print("Pitch Change, ch=");
    Serial.print(channel, DEC);
    Serial.print(", pitch=");
    Serial.println(pitch, DEC);
    Serial.printf("     0-6= %02x, 7-13=%02x\n", pitch & 0x7f, (pitch>>7) & 0x7f);
   #endif
}


//__________________________________________________________________________________


FLASHMEM void InitHndlr_TeensyROM()
{
   IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
   //MIDI handlers for MIDI2SID:
   if(IO1[rwRegMIDISettings] & rMIDISetNoteOffOnEn)
   {
      usbHostMIDI.setHandleNoteOff      (M2SOnNoteOff);             // 8x
      usbDevMIDI.setHandleNoteOff       (M2SOnNoteOff);             // 8x
      usbHostMIDI.setHandleNoteOn       (M2SOnNoteOn);              // 9x
      usbDevMIDI.setHandleNoteOn        (M2SOnNoteOn);              // 9x
   }

   //These are for debug only, only note on/off is actually used
   if(IO1[rwRegMIDISettings] & rMIDISetControlChangeEn)
   {
      usbHostMIDI.setHandleControlChange(M2SOnControlChange);       // Bx
      usbDevMIDI.setHandleControlChange (M2SOnControlChange);       // Bx
   }
   if(IO1[rwRegMIDISettings] & rMIDISetPitchChangeEn)
   {
      usbHostMIDI.setHandlePitchChange  (M2SOnPitchChange);         // Ex
      usbDevMIDI.setHandlePitchChange   (M2SOnPitchChange);         // Ex
   }
}

void IO2Hndlr_TeensyROM(uint8_t Address, bool R_Wn)
{
   if (Address == IO2Scratch)
   {
      if (R_Wn) DataPortWriteWaitLog(IO1[wRegIRQNMITest]);  //High (IO2 Read)
      else IO1[wRegIRQNMITest] = DataPortWaitRead();  //Low (IO2 Write)
   }
}

void SetVideoStdTiming()
{  //from the machine type MainMenu.asm reported in IO1[wRegVid_TOD_Clks]; also used by the td serial command
   //called from IO1 handler, do not FLASHMEM
   if (IO1[wRegVid_TOD_Clks] & rvtcNTSC)
   {
      nS_MaxAdj = Def_nS_MaxAdjNTSC;
      if (IO1[wRegVid_TOD_Clks] & rvtcC128)
      {
         nS_DMASetup     = Def_nS_DMASetupNTSC128;
         nS_DMADataSetup = Def_nS_DMADataSetupNTSC128;
         nS_DMADataHold  = Def_nS_DMADataHoldNTSC128;
      }
      else
      {
         nS_DMASetup     = Def_nS_DMASetupNTSC;
         nS_DMADataSetup = Def_nS_DMADataSetupNTSC;
         nS_DMADataHold  = Def_nS_DMADataHoldNTSC;
      }
   }
   else
   {  //PAL C128 has no measured set of its own yet
      nS_MaxAdj       = Def_nS_MaxAdjPAL;
      nS_DMASetup     = Def_nS_DMASetupPAL;
      nS_DMADataSetup = Def_nS_DMADataSetupPAL;
      nS_DMADataHold  = Def_nS_DMADataHoldPAL;
   }

   sprintf(StrMachineInfo, "C%d  %s Vid  %s", (IO1[wRegVid_TOD_Clks] & rvtcC128) ? 128 : 64,
      (IO1[wRegVid_TOD_Clks] & rvtcNTSC) ? "NTSC" : "PAL",
      (IO1[wRegVid_TOD_Clks] & rvtc60Hz) ? "6" : "5");
}

void IO1Hndlr_TeensyROM(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case rRegItemTypePlusIOH:
         {  //ISR context: cannot print, so an out-of-range selection reports the type that
            //already means "not a valid item" (HandleExecution says so) rather than reading
            //whatever sits past the end of the menu now in place.
            const StructMenuItem* Item = MenuItemSel();
            Data = (Item == NULL) ? rtNone : Item->ItemType;
            if(Item != NULL && IO1[rWRegCurrMenuWAIT] == rmtTeensy && Item->IOHndlrAssoc != IOH_None) Data |= 0x80; //bit 7 indicates an assigned IOHandler
            DataPortWriteWaitLog(Data);
         }
            break;
         case rRegStreamData:
            //Same class as the StatusFunction[] guard: an index the C64 advances, with
            //nothing checking it before the deref.  XferSize is 0 until an image is
            //staged, so this also covers XferImage still being NULL at power-up.
            //rRegStrAvailable only *tells* the C64 where the end is; it cannot stop it.
            DataPortWriteWait(StreamOffsetAddr < XferSize ? XferImage[StreamOffsetAddr] : 0);
            //inc on read, check for end.  Stops at XferSize rather than counting past it:
            //the guard above makes running past harmless, but it is still a counter the
            //C64 advances with nothing stopping it, and at uint32_t it would take 2^32
            //reads to wrap back into the buffer instead of 2^16.
            if (StreamOffsetAddr < XferSize) StreamOffsetAddr++;
            if (StreamOffsetAddr >= XferSize) IO1[rRegStrAvailable]=0; //signal end of transfer
            break;
         case rwRegSerialString:
            //ptrSerialString is NULL until a selector is written below, and StringOffset
            //only ever counted up -- a C64 reading past the terminator walked off the end
            //of whichever string was selected, a byte per read.  Stop at the NUL instead.
            Data = (ptrSerialString == NULL) ? 0 : ptrSerialString[StringOffset];
            if (Data != 0) StringOffset++;
            DataPortWriteWaitLog(ToPETSCII(Data));
            break;
         default: //used for all other IO1 reads
            //The ISR masks Address to 0..255 but IO1 is only IO1Size long, so DE68..DEFF
            //used to hand the C64 heap past the allocation, a byte per read.  Zero is
            //also the right answer for the one high register that lands here:
            //rRegIOHSwapPoll reads rihsBusy (0x00) = keep polling, where garbage had a
            //1-in-256 chance of reading as rihsReady.
            DataPortWriteWaitLog(Address < IO1Size ? IO1[Address] : 0);
            break;
      }
   }
   else  // IO1 write
   {
      Data = DataPortWaitRead();
      TraceLogAddValidData(Data);
      switch(Address)
      {
         case rwRegSelItemOnPage:
            SelItemFullIdx = MenuIdxFromRegs(Data);
         case rwRegStatus:
         case wRegIRQ_ACK:
         case rwRegIRQ_CMD:
         case rwRegCodeStartPage:
         case rwRegCodeLastPage:
         case rwRegCursorItemOnPg:
         case rwRegSIDSongNumZ:
         case rwRegSIDCurSpeedHi:
         case rwRegSIDCurSpeedLo:
         case rwRegScratch:
         case wRegIRQNMITest:
            IO1[Address]=Data;
            break;

         case wRegGameExROMCtl:
            if (Data & 1) SetGameAssert; //rtBin8kHi or rtBin16k
            else SetGameDeassert;  //8kLo or None
            if (Data & 2) SetExROMAssert;  //rtBin16k or 8kLo
            else SetExROMDeassert;  //rtBin8kHi or None
            break;
         case wRegVid_TOD_Clks:
            IO1[wRegVid_TOD_Clks]=Data;
            SetVideoStdTiming(); //make NTSC/PAL/C128 specific timing tweaks upon discovery
            break;
         case rwRegPageNumber:
            IO1[rwRegPageNumber]=Data;
            IO1[rRegNumItemsOnPage] = (NumItemsFull > Data*MaxItemsPerPage ? MaxItemsPerPage : NumItemsFull-(Data-1)*MaxItemsPerPage);
            break;
         case rwRegNextIOHndlr:
            if (Data & 0x80) Data = LastSelectableIOH; //wrap around to last item if negative
            else if (Data > LastSelectableIOH) Data = 0; //wrap around to first item if above max
            IO1[rwRegNextIOHndlr]= Data;
            eepAddrToWrite = eepAdNextIOHndlr;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rWRegCurrMenuWAIT:
            IO1[rWRegCurrMenuWAIT]=Data;
            IO1[rwRegStatus] = rsChangeMenu; //work this in the main code
            break;
         case rwRegMIDISettings:
            IO1[rwRegMIDISettings]= Data;
            eepAddrToWrite = eepAdMIDISettings;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegMIDISettings2:
            IO1[rwRegMIDISettings2]= Data;
            eepAddrToWrite = eepAdMIDISettings2;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegPwrUpDefaults:
            IO1[rwRegPwrUpDefaults]= Data;
            eepAddrToWrite = eepAdPwrUpDefaults;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegPwrUpDefaults2:
            IO1[rwRegPwrUpDefaults2]= Data;
            eepAddrToWrite = eepAdPwrUpDefaults2;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegPwrUpDefaults3:
            IO1[rwRegPwrUpDefaults3]= Data;
            eepAddrToWrite = eepAdPwrUpDefaults3;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegTimezone:
            IO1[rwRegTimezone]= Data;
            eepAddrToWrite = eepAdTimezone;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegColorRefStart ... (rwRegColorRefStart+NumColorRefs-1):
            IO1[Address]= Data;
            eepAddrToWrite = Address-rwRegColorRefStart +eepAdColorRefStart;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case wRegSearchLetterWAIT:
            IO1[wRegSearchLetterWAIT] = Data;
            IO1[rwRegStatus] = rsSearchForLetter; //work this in the main code
            break;

         case wRegSIDSpeedChange:
            {
               int16_t SidSpeedAdjustTemp = SidSpeedAdjust;
               switch(Data)
               {
                  case rsscIncMajor:
                     SidSpeedAdjustTemp+=2*256;  // 2%
                     break;
                  case rsscDecMajor:
                     SidSpeedAdjustTemp-=2*256;
                     break;
                  case rsscIncMinor:
                     SidSpeedAdjustTemp+=64;  // 0.25%
                     break;
                  case rsscDecMinor:
                     SidSpeedAdjustTemp-=64;
                     break;
                  case rsscSetDefault:
                     SidSpeedAdjustTemp=0;
                     //SidLogConv = false; //def to linear
                     break;
                  case rsscToggleLogLin:
                     SidSpeedAdjustTemp=0;
                     SidLogConv = !SidLogConv;
                     break;
               }
               SetSIDSpeed(SidLogConv, SidSpeedAdjustTemp); //regs & settings updated if pass
            }
            break;
         case rwRegSerialString: //Select/build(no waiting) string to set ptrSerialString and read out serially
            StringOffset = 0;
            switch(Data)
            {
               case rsstItemName:
               {  //This memcpy follows a pointer stored *in* the menu item, so an index past
                  //the end of the menu copies 35 bytes from whatever a stray word happens to
                  //point at -- and then hands them to the C64 a byte at a time.  ISR context
                  //cannot print, so say it in the string itself, as "?Stat"/"?IOH" do.
                  const StructMenuItem* Item = MenuItemSel();
                  if (Item == NULL || Item->Name == NULL)
                  {
                     strcpy(SerialStringBuf, "?Item");
                     ptrSerialString = SerialStringBuf;
                     break;
                  }
                  memcpy(SerialStringBuf, Item->Name, MaxItemDispLength);
                  SerialStringBuf[MaxItemDispLength-1] = 0; //Trim to length, if needed
                  if ((IO1[rwRegPwrUpDefaults] & rpudShowExtension) == 0 &&
                      Item->ItemType > rtDirectory &&
                      IO1[rWRegCurrMenuWAIT] != rmtTeensy)
                  { // if not show ext, not dir or unknown, not a TR Menu: terminate before extension
                     char *pDot = strrchr(SerialStringBuf, '.'); //find last dot
                     if (pDot != NULL) *pDot = 0; //terminate there
                  }
                  ptrSerialString = SerialStringBuf;
               }
                  break;
               case rsstNextIOHndlrName:
               {  //Same range rule the other two IOHandler[] index sites apply
                  //(IOHandlers.ino, Min_DriveDirLoad.ino).  This register is not only
                  //written by the C64: it is also loaded raw from EEPROM (line ~516),
                  //where a byte saved by a build with more handlers -- or a corrupt
                  //cell -- reads past the table and derefs whatever is there as ->Name.
                  const uint8_t NextIOH = IO1[rwRegNextIOHndlr];
                  ptrSerialString = (NextIOH < IOH_Num_Handlers) ?
                     IOHandler[NextIOH]->Name : (char*)"?IOH";
               }
                  break;
               case rsstSerialStringBuf:
                  //assumes SerialStringBuf built first...(FWUpd msg or BuildInfo)
                  ptrSerialString = SerialStringBuf;
                  break;
               case rsstVersionNum:
                  ptrSerialString = strVersionNumber;
                  break;
               case rsstSIDInfo:
                  ptrSerialString = StrSIDInfo;
                  break;
               case rsstMachineInfo:
                  ptrSerialString = StrMachineInfo;
                  break;
               case rsstSIDSpeed:
               {
                  int32_t DefSIDSpeed = IO1[rRegSIDDefSpeedLo]+256*IO1[rRegSIDDefSpeedHi];
                  int32_t CurSIDSpeed = IO1[rwRegSIDCurSpeedLo]+256*IO1[rwRegSIDCurSpeedHi];
                  sprintf(SerialStringBuf, "%0.2f%%  ", (float)DefSIDSpeed/CurSIDSpeed*100);
                  ptrSerialString = SerialStringBuf;
               }
                  break;
               case rsstSIDSpeedCtlType:
                  strcpy(SerialStringBuf, (SidLogConv ? "Log" : "Lin"));
                  ptrSerialString = SerialStringBuf;
                  break;
               case rsstShortDirPath:
                  {
                     uint16_t Len = strlen(DriveDirPath);
                     if (Len >= 40)
                     {
                        strcpy(SerialStringBuf, "...");
                        strcat(SerialStringBuf, DriveDirPath+Len-36);
                        ptrSerialString = SerialStringBuf;
                     }
                     else ptrSerialString = DriveDirPath;
                  }
                  break;
               default:
                  //An unhandled selector used to leave ptrSerialString pointing at
                  //whatever the previous one selected, so the C64 silently read back a
                  //stale string.  Say so instead -- same choice as "?Stat" above.
                  ptrSerialString = (char*)"?SerStr";
                  break;
            }
            break;

         case wRegControl:
            switch(Data)
            {
               case rCtlVanishROM:
                  SetGameDeassert;
                  SetExROMDeassert;
                  LOROM_Image = NULL;
                  HIROM_Image = NULL;
                  break;
// No longer Used:
//               case rCtlBasicReset:
//                  //SetLEDOff;
//                  doReset=true;
//                  IO1[rwRegStatus] = rsIOHWNextInit; //Support IO handler at reset
//                  break;
               case rCtlStartSelItemWAIT:
                  IO1[rwRegStatus] = rsStartItem; //work this in the main code
                  break;
               case rCtlSetRTCfromNetWAIT:
                  IO1[rwRegStatus] = rsSetRTCfromNet;   //work this in the main code
                  break;
               case rCtlKERNALPreStartWAIT:
                  IO1[rwRegStatus] = rsKERNALPreStart;   //work this in the main code
                  break;
               case rCtlC64TODfromRTCWAIT:
                  IO1[rwRegStatus] = rsC64TODfromRTC;
                  break;
               case rCtlRunningPRG:
                  IO1[rwRegStatus] = rsIOHWSelInit; //Support IO handlers in PRG
                  HandshakeReady = false;     //reset in case a prior PRG load left it set
                  PendingfBusSnoop = NULL;
                  fBusSnoop = &HandshakeSnoop; //armed now, so the C64's very next read already sees rihsBusy
                  break;
               case rCtlMakeInfoStrWAIT:
                  IO1[rwRegStatus] = rsMakeBuildCPUInfoStr; //work this in the main code
                  break;
               case rCtlUpDirectoryWAIT:
                  IO1[rwRegStatus] = rsUpDirectory; //work this in the main code
                  break;
               case rCtlLoadSIDWAIT:
                  IO1[rwRegStatus] = rsLoadSIDforXfer; //work this in the main code
                  break;
               case rCtlNextPicture:
                  IO1[rwRegStatus] = rsNextPicture; //work this in the main code
                  break;
               case rCtlLastPicture:
                  IO1[rwRegStatus] = rsLastPicture; //work this in the main code
                  break;
               case rCtlWriteNFCTagCheckWAIT:
                  IO1[rwRegStatus] = rsWriteNFCTagCheck; //work this in the main code
                  break;
               case rCtlWriteNFCTagWAIT:
                  IO1[rwRegStatus] = rsWriteNFCTag; //work this in the main code
                  break;
               case rCtlNFCReEnableWAIT:
                  IO1[rwRegStatus] = rsNFCReEnable; //work this in the main code
                  break;
               case rCtlReturnToMainMenu:
                  BtnPressed = true;
                  break;
               case rCtlRebootTeensyROM:
                  RebootTR();
                  break;
               case rCtlSetBackgroundSIDWAIT:
                  IO1[rwRegStatus] = rsSetBackgroundSID; //work this in the main code
                  break;
               case rCtlSetKERNALBinWAIT:
                  IO1[rwRegStatus] = rsSetKERNALBin; //work this in the main code
                  break;
               case rCtlSetREUFileWAIT:
                  IO1[rwRegStatus] = rsSetREUFile; //work this in the main code
                  break;
               case rCtlSetAutoLaunchWAIT:
                  IO1[rwRegStatus] = rsSetAutoLaunch; //work this in the main code
                  break;
               case rCtlClearAutoLaunchWAIT:
                  IO1[rwRegStatus] = rsClearAutoLaunch; //work this in the main code
                  break;
               case rCtlNextTextFile:
                  IO1[rwRegStatus] = rsNextTextFile; //work this in the main code
                  break;
               case rCtlLastTextFile:
                  IO1[rwRegStatus] = rsLastTextFile; //work this in the main code
                  break;
               case rCtlMountDxxFileWAIT:
                  IO1[rwRegStatus] = rsMountDxxFile; //work this in the main code
                  break;
               case rCtlHotKeySetLaunch:
                  IO1[rwRegStatus] = rsHotKeySetLaunch; //work this in the main code
                  break;
               case rCtlNetListenInitWAIT:
                  IO1[rwRegStatus] = rsNetListenInit; //work this in the main code
                  break;
               case rCtlExtPortCheckWAIT:
                  IO1[rwRegStatus] = rsExtPortCheck; //work this in the main code
                  break;
               case rCtlExpPortDMAWAIT:
                  IO1[rwRegStatus] = rsExpPortDMA; //work this in the main code
                  break;
               case rCtlForceEthInitWAIT:
                  IO1[rwRegStatus] = rsForceEthInit; //work this in the main code
                  break;
               case rCtlMakeExtHostStrWAIT:
                  IO1[rwRegStatus] = rsMakeExtHostStr; //work this in the main code
                  break;
               case rCtlUninstallExtHostWAIT:
                  IO1[rwRegStatus] = rsUninstallExtHost; //work this in the main code
                  break;
               case rCtlMakeStrWAIT_First ... rCtlMakeStrWAIT_Last:
                  IO1[wRegControl] = Data; //preserve for later use
                  IO1[rwRegStatus] = rsMakeFilenameStr; //work this in the main code
                  break;
               case rCtlRTCAdjWAIT_First...rCtlRTCAdjWAIT_Last:
                  IO1[wRegControl] = Data; //preserve for later use
                  IO1[rwRegStatus] = rsRTCAdjust; //work this in the main code
                  break;
            }
            break;
      }
   } //write
}

void PollingHndlr_TeensyROM()
{
   if (IO1[rwRegStatus] != rsReady)
   {  //ISR requested work
#if defined(VM_EXTENSIONS_ENABLED) && !defined(MinimumBuild)
      if (VmFail::pending())
      {  //the C64 is inside WaitForTR*, so this is the first moment it can read
         //why the last extension launch didn't run.  Restore the queued work
         //afterwards: the dispatch below reads the same register.
         const uint8_t Queued = IO1[rwRegStatus];
         VmFail::report();
         if (IO1[rwRegStatus] == rsContinue) VmFail::clear(); //the C64 read it
         IO1[rwRegStatus] = Queued;
      }
#endif
      //The null check is not redundant with the bounds check. A status code appended
      //without its StatusFunction[] entry is now a build error (the count static_assert
      //in StatusFunctions.c), but an entry written null, or a table reordered so a live
      //code lands on a null slot, still gets here. That call faults the core and reboots
      //it; the CrashReport the main image prints on the way back up (Teensy.ino) cannot
      //say which status code got there. Landing it in "?Stat" costs one compare.
      const uint8_t Status = IO1[rwRegStatus];
      if (Status<rsNumStatusTypes && StatusFunction[Status]) StatusFunction[Status]();
      else Serial.printf("?Stat: %02x\n", Status);
      Serial.flush();
      IO1[rwRegStatus] = rsReady;
   }
   usbHostMIDI.read();
   usbDevMIDI.read();
}
