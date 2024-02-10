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
void PollingHndlr_TeensyROM();                           
void InitHndlr_TeensyROM();                           

stcIOHandlers IOHndlr_TeensyROM =
{
  "TeensyROM",              //Name of handler
  &InitHndlr_TeensyROM,     //Called once at handler startup
  &IO1Hndlr_TeensyROM,      //IO1 R/W handler
  NULL,                     //IO2 R/W handler
  NULL,                     //ROML Read handler, in addition to any ROM data sent
  NULL,                     //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_TeensyROM,  //Polled in main routine
  NULL,                     //called at the end of EVERY c64 cycle
};

volatile uint8_t* IO1;  //io1 space/regs
volatile uint16_t StreamOffsetAddr, StringOffset = 0;
volatile char*    ptrSerialString; //pointer to selected serialstring
char SerialStringBuf[MaxPathLength] = "err"; // used for message passing to C64, up to full path length
volatile uint8_t doReset = true;
const unsigned char *HIROM_Image = NULL;
const unsigned char *LOROM_Image = NULL;
volatile uint8_t eepAddrToWrite, eepDataToWrite;
StructMenuItem *MenuSource;
uint16_t SelItemFullIdx = 0;  //logical full index into menu for selected item
uint16_t NumItemsFull;  //Num Items in Current Menu
uint8_t *XferImage = NULL; //pointer to image being transfered to C64 
uint32_t XferSize = 0;  //size of image being transfered to C64
uint8_t ASCIItoPETSCII[128]=
{
 /*   ASCII   */  //PETSCII
 /*   0 'null'*/    0,
 /*   1 '' */    0,
 /*   2 '' */    0,
 /*   3 '' */    0,
 /*   4 '' */    0,
 /*   5 '' */    0,
 /*   6 '' */    0,
 /*   7 '' */    0,
 /*   8 ''  */    0,
 /*   9 '\t'  */   32, // tab -> space
 /*  10 '\n'  */   10, //let this slide: won't print anything in petscii, but maybe if prog is in terminal mode?
 /*  11 ''  */    0,
 /*  12 ''  */    0,
 /*  13 '\r'  */   13,
 /*  14 ''  */    0,
 /*  15 ''  */    0,
 /*  16 '' */    0,
 /*  17 '' */    0,
 /*  18 '' */    0,
 /*  19 '' */    0,
 /*  20 '' */    0,
 /*  21 '' */    0,
 /*  22 '' */    0,
 /*  23 '' */    0,
 /*  24 '' */    0,
 /*  25 ''  */    0,
 /*  26 '' */    0,
 /*  27 '' */    0,
 /*  28 ''  */    0,
 /*  29 ''  */    0,
 /*  30 ''  */    0,
 /*  31 ''  */    0,
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
 /* 127 '' */   95, // delete            ->   left arrow
};

extern bool EthernetInit();
extern void MenuChange();
extern void HandleExecution();
extern bool PathIsRoot();
extern void LoadDirectory(FS *sourceFS);
extern void FreeDriveDirMenu();
extern void RedirectEmptyDriveDirMenu();
extern void IOHandlerInitToNext();
extern void ParseSIDHeader(const char *filename);
extern stcIOHandlers* IOHandler[];
extern char DriveDirPath[];
extern uint8_t RAM_Image[];
extern char* StrSIDInfo;
extern char StrMachineInfo[];

#define DecToBCD(d) ((int((d)/10)<<4) | ((d)%10))

//#define ToPETSCII(x) (x==95 ? 32 : x>64 ? x^32 : x)
#define ToPETSCII(x) ASCIItoPETSCII[(x) & 0x7f]

void getNtpTime() 
{
   if (!EthernetInit()) 
   {
      IO1[rRegLastSecBCD]  = 0;      
      IO1[rRegLastMinBCD]  = 0;      
      IO1[rRegLastHourBCD] = 0;      
      return;
   }
   
   unsigned int localPort = 8888;       // local port to listen for UDP packets
   const char timeServer[] = "us.pool.ntp.org"; // time.nist.gov     NTP server

   udp.begin(localPort);
   
   const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
   byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
   
   Serial.printf("Updating time from: %s\n", timeServer);
   while (udp.parsePacket() > 0) ; // discard any previously received packets
   
   // send an NTP request to the time server at the given address
   // set all bytes in the buffer to 0
   memset(packetBuffer, 0, NTP_PACKET_SIZE);
   // Initialize values needed to form NTP request
   packetBuffer[0] = 0b11100011;   // LI, Version, Mode
   packetBuffer[1] = 0;     // Stratum, or type of clock
   packetBuffer[2] = 6;     // Polling Interval
   packetBuffer[3] = 0xEC;  // Peer Clock Precision
   // 8 bytes of zero for Root Delay & Root Dispersion
   packetBuffer[12]  = 49;
   packetBuffer[13]  = 0x4E;
   packetBuffer[14]  = 49;
   packetBuffer[15]  = 52;
   // all NTP fields have been given values, now send a packet requesting a timestamp:
   udp.beginPacket(timeServer, 123); // NTP requests are to port 123
   udp.write(packetBuffer, NTP_PACKET_SIZE);
   udp.endPacket();

   uint32_t beginWait = millis();
   while (millis() - beginWait < 1500) 
   {
      int size = udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) 
      {
         udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
         uint32_t secsSince1900;
         // convert four bytes starting at location 40 to a long integer
         secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
         secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
         secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
         secsSince1900 |= (unsigned long)packetBuffer[43];
         Serial.printf("Received NTP Response in %d mS\n", (millis() - beginWait));

         //since we don't need the date, leaving out TimeLib.h all together
         IO1[rRegLastSecBCD] = DecToBCD(secsSince1900 % 60);
         secsSince1900 = secsSince1900/60 + 30*(int8_t)IO1[rwRegTimezone]; //to  minutes, offset timezone (30 min increments)
         IO1[rRegLastMinBCD] = DecToBCD(secsSince1900 % 60);
         secsSince1900 = (secsSince1900/60) % 24; //to hours
         if (secsSince1900 >= 12) IO1[rRegLastHourBCD] = 0x80 | DecToBCD(secsSince1900-12); //change to 0 based 12 hour and add pm flag
         else IO1[rRegLastHourBCD] =DecToBCD(secsSince1900); //default to AM (bit 7 == 0)
   
         Serial.printf("Time: %02x:%02x:%02x %sm\n", (IO1[rRegLastHourBCD] & 0x7f) , IO1[rRegLastMinBCD], IO1[rRegLastSecBCD], (IO1[rRegLastHourBCD] & 0x80) ? "p" : "a");        
         return;
      }
   }
   Serial.println("NTP Response timeout!");
}

void WriteEEPROM()
{
   Printf_dbg("Wrote $%02x to EEP addr %d\n", eepDataToWrite, eepAddrToWrite);
   EEPROM.write(eepAddrToWrite, eepDataToWrite);
}

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

FLASHMEM void MakeBuildInfo()
{
   //Serial.printf("\nBuild Date/Time: %s  %s\nCPU Freq: %lu MHz   Temp: %.1f°C\n", __DATE__, __TIME__, (F_CPU_ACTUAL/1000000), tempmonGetTemp());
   sprintf(SerialStringBuf, "       FW: %s, %s\r\n   Teensy: %luMHz  %.1fC", __DATE__, __TIME__, (F_CPU_ACTUAL/1000000), tempmonGetTemp());
}

FLASHMEM void MakeBuildCPUInfoStr()
{
   FreeDriveDirMenu(); //Will mess up navigation if not on TR menu!
   RedirectEmptyDriveDirMenu(); //OK since we're on the TR settings screen
  
   uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
   //Serial.printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);
      
   uint8_t NumChips = RAM2blocks();
   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
   NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
   //Serial.printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
  
   CrtMax += NumChips*8;
   char FreeStr[20];
   sprintf(FreeStr, "  %luk free\r", (uint32_t)(CrtMax*1.004));  //larger File size due to header info.

   MakeBuildInfo();
   strcat(SerialStringBuf, FreeStr);
}

void UpDirectory()
{
   //non-root of Teensy, SD or USB drive only
   if(PathIsRoot()) return;

   if(IO1[rWRegCurrMenuWAIT] == rmtTeensy) MenuChange(); //back to root, only 1 dir level
   else
   {   
      char * LastSlash = strrchr(DriveDirPath, '/'); //find last slash
      if (LastSlash == NULL) return;
      LastSlash[0] = 0;  //terminate it there 
      if (IO1[rWRegCurrMenuWAIT] == rmtSD) LoadDirectory(&SD); 
      else LoadDirectory(&firstPartition); 
      IO1[rwRegCursorItemOnPg] = 0;
      IO1[rwRegPageNumber]     = 1;
   }
}

void SetCursorToItemNum(uint16_t ItemNum)
{
   IO1[rwRegPageNumber] = ItemNum/MaxItemsPerPage +1;
   IO1[rwRegCursorItemOnPg] = ItemNum % MaxItemsPerPage;
   IO1[rRegNumItemsOnPage] = (NumItemsFull > IO1[rwRegPageNumber]*MaxItemsPerPage ? MaxItemsPerPage : NumItemsFull-(IO1[rwRegPageNumber]-1)*MaxItemsPerPage);
}

FLASHMEM void NextPicture()
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg] + (IO1[rwRegPageNumber]-1) * MaxItemsPerPage;
   uint16_t InitItemNum = SelItemFullIdx;
   
   do
   {
      if (++SelItemFullIdx >= NumItemsFull) SelItemFullIdx=0;
      if (MenuSource[SelItemFullIdx].ItemType == rtFileKla ||
          MenuSource[SelItemFullIdx].ItemType == rtFileArt)
      {
         SetCursorToItemNum(SelItemFullIdx);
         return;
      }
   } while (SelItemFullIdx != InitItemNum); //just 1 time through, but should stop on same initial one unless changed externally
}

FLASHMEM void LastPicture()
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg] + (IO1[rwRegPageNumber]-1) * MaxItemsPerPage;
   uint16_t InitItemNum = SelItemFullIdx;
   
   do
   {
      if (SelItemFullIdx == 0) SelItemFullIdx = NumItemsFull-1;
      else SelItemFullIdx--;
      
      if (MenuSource[SelItemFullIdx].ItemType == rtFileKla ||
          MenuSource[SelItemFullIdx].ItemType == rtFileArt)
      {
         SetCursorToItemNum(SelItemFullIdx);
         return;
      }
   } while (SelItemFullIdx != InitItemNum); //just 1 time through, but should stop on same initial one unless changed externally   
}

void SearchForLetter()
{
   uint16_t ItemNum = 0;
   uint8_t SearchFor = IO1[wRegSearchLetterWAIT];
   
   //ascii upper case (toupper) matches petscii lower case ('a'=65)
   while (ItemNum < NumItemsFull)
   {
      if (toupper(MenuSource[ItemNum].Name[0]) >= SearchFor)
      {
         SetCursorToItemNum(ItemNum);
         return;
      }
      ItemNum++;
   }
}

FLASHMEM void LoadMainSIDforXfer()
{
   XferImage = RAM_Image; 
   XferSize  = sizeof(SIDforBackground); 
   memcpy(XferImage, SIDforBackground, XferSize);
   ParseSIDHeader("Main Background SID"); //assuming it passes for buit-in
}

void (*StatusFunction[rsNumStatusTypes])() = //match RegStatusTypes order
{
   &MenuChange,          // rsChangeMenu 
   &HandleExecution,     // rsStartItem  
   &getNtpTime,          // rsGetTime    
   &IOHandlerInitToNext, // rsIOHWinit   
   &WriteEEPROM,         // rsWriteEEPROM
   &MakeBuildCPUInfoStr, // rsMakeBuildCPUInfoStr
   &UpDirectory,         // rsUpDirectory
   &SearchForLetter,     // rsSearchForLetter
   &LoadMainSIDforXfer,  // rsLoadSIDforXfer
   &NextPicture,         // rsNextPicture    
   &LastPicture,         // rsLastPicture    

};


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
   
   float Frequency = 440*pow(1.059463094359,note-60);  
   uint32_t RegVal = Frequency*16777216/NTSCBusFreq;
   
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


void InitHndlr_TeensyROM()
{
   IO1[rwRegNextIOHndlr] = EEPROM.read(eepAdNextIOHndlr);  //in case it was over-ridden by .crt
   //MIDI handlers for MIDI2SID:
   usbHostMIDI.setHandleNoteOff      (M2SOnNoteOff);             // 8x
   usbHostMIDI.setHandleNoteOn       (M2SOnNoteOn);              // 9x
   usbHostMIDI.setHandleControlChange(M2SOnControlChange);       // Bx
   usbHostMIDI.setHandlePitchChange  (M2SOnPitchChange);         // Ex

   usbDevMIDI.setHandleNoteOff       (M2SOnNoteOff);             // 8x
   usbDevMIDI.setHandleNoteOn        (M2SOnNoteOn);              // 9x
   usbDevMIDI.setHandleControlChange (M2SOnControlChange);       // Bx
   usbDevMIDI.setHandlePitchChange   (M2SOnPitchChange);         // Ex
}   

void IO1Hndlr_TeensyROM(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   if (R_Wn) //High (IO1 Read)
   {
      switch(Address)
      {
         case rRegItemTypePlusIOH:
            Data = MenuSource[SelItemFullIdx].ItemType;
            if(IO1[rWRegCurrMenuWAIT] == rmtTeensy && MenuSource[SelItemFullIdx].IOHndlrAssoc != IOH_None) Data |= 0x80; //bit 7 indicates an assigned IOHandler
            DataPortWriteWaitLog(Data);  
            break;
         case rRegStreamData:
            DataPortWriteWait(XferImage[StreamOffsetAddr]);
            //inc on read, check for end:
            if (++StreamOffsetAddr >= XferSize) IO1[rRegStrAvailable]=0; //signal end of transfer
            break;
         case rwRegSerialString:
            Data = ptrSerialString[StringOffset++];
            DataPortWriteWaitLog(ToPETSCII(Data));            
            break;
         default: //used for all other IO1 reads
            DataPortWriteWaitLog(IO1[Address]); //will read garbage if above IO1Size
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
            SelItemFullIdx=Data+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;
         case rwRegStatus:
         case wRegVid_TOD_Clks:
         case wRegIRQ_ACK:
         case rwRegIRQ_CMD:
         case rwRegCodeStartPage:
         case rwRegCodeLastPage:
         case rwRegCursorItemOnPg:
            IO1[Address]=Data;
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
         case rwRegPwrUpDefaults:
            IO1[rwRegPwrUpDefaults]= Data;
            eepAddrToWrite = eepAdPwrUpDefaults;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case rwRegTimezone:
            IO1[rwRegTimezone]= Data;
            eepAddrToWrite = eepAdTimezone;
            eepDataToWrite = Data;
            IO1[rwRegStatus] = rsWriteEEPROM; //work this in the main code
            break;
         case wRegSearchLetterWAIT:
            IO1[wRegSearchLetterWAIT] = Data;
            IO1[rwRegStatus] = rsSearchForLetter; //work this in the main code
            break;
         case rwRegSerialString: //Select/build(no waiting) string to set ptrSerialString and read out serially
            StringOffset = 0;
            switch(Data)
            {
               case rsstItemName:
                  memcpy(SerialStringBuf, MenuSource[SelItemFullIdx].Name, MaxItemDispLength);
                  SerialStringBuf[MaxItemDispLength-1] = 0;
                  ptrSerialString = SerialStringBuf;
                  break;
               case rsstNextIOHndlrName:
                  ptrSerialString = IOHandler[IO1[rwRegNextIOHndlr]]->Name;
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
               case rCtlBasicReset:  
                  //SetLEDOff;
                  doReset=true;
                  IO1[rwRegStatus] = rsIOHWinit; //Support IO handlers at reset
                  break;
               case rCtlStartSelItemWAIT:
                  IO1[rwRegStatus] = rsStartItem; //work this in the main code
                  break;
               case rCtlGetTimeWAIT:
                  IO1[rwRegStatus] = rsGetTime;   //work this in the main code
                  break;
               case rCtlRunningPRG:
                  IO1[rwRegStatus] = rsIOHWinit; //Support IO handlers in PRG
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
            }
            break;
      }
   } //write
}

void PollingHndlr_TeensyROM()
{
   if (IO1[rwRegStatus] != rsReady) 
   {  //ISR requested work
      if (IO1[rwRegStatus]<rsNumStatusTypes) StatusFunction[IO1[rwRegStatus]]();
      else Serial.printf("?Stat: %02x\n", IO1[rwRegStatus]);
      Serial.flush();
      IO1[rwRegStatus] = rsReady;
   }
   usbHostMIDI.read();
   usbDevMIDI.read();
}
   

