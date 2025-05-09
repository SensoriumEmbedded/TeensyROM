// MIT License
// 
// Copyright (c) 2024 Travis Smith
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


//IO Handler for MIDI ASID SysEx streams

IntervalTimer ASIDPlaybackTimer;

void IO1Hndlr_ASID(uint8_t Address, bool R_Wn);  
void PollingHndlr_ASID();                           
void InitHndlr_ASID();                           

stcIOHandlers IOHndlr_ASID =
{
  "ASID Player",           //Name of handler
  &InitHndlr_ASID,       //Called once at handler startup
  &IO1Hndlr_ASID,              //IO1 R/W handler
  NULL,                        //IO2 R/W handler
  NULL,                        //ROML Read handler, in addition to any ROM data sent
  NULL,                        //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_ASID,          //Polled in main routine
  NULL,                        //called at the end of EVERY c64 cycle
};

enum ASIDregsMatching  //synch with ASIDPlayer.asm
{
   // registers:
   ASIDAddrReg         = 0xc2,   // (Read only)  Data type and SID Address Register 
   ASIDDataReg         = 0xc4,   // (Read only)  ASID data, increment queue Tail 
   ASIDQueueUsed       = 0xc8,   // (Read only)  Current Queue amount used
   ASIDContReg         = 0xca,   // (Write only) Control Reg 
   ASIDVoiceMuteReg    = 0xcc,   // (write only) bits 0-2 set voice muting

   // Control Reg Commands
   // Timer controls match TblMsgTimerState, start at 0
   ASIDContTimerOff    = 0x00,   //disable Frame Timer
   ASIDContTimerOnAuto = 0x01,   //enable Frame Timer, auto seed time
   ASIDContTimerOn50Hz = 0x02,   //enable Frame Timer, 50Hz seed time
   NumTimerStates      = 0x03,   //Always last, number of states
   // ...                    
   ASIDContIRQOn       = 0x10,   //enable ASID IRQ
   ASIDContIRQOff      = 0x11,   //disable ASID IRQ
   ASIDContExit        = 0x12,   //Disable IRQ, Send TR to main menu
   // ...              
   ASIDContBufTiny     = 0x20,   //Set buffer to size Tiny  
   ASIDContBufSmall    = 0x21,   //Set buffer to size Small  
   ASIDContBufMedium   = 0x22,   //Set buffer to size Medium 
   ASIDContBufLarge    = 0x23,   //Set buffer to size Large  
   ASIDContBufXLarge   = 0x24,   //Set buffer to size XLarge 
   ASIDContBufXXLarge  = 0x25,   //Set buffer to size XXLarge
   ASIDContBufFirstItem= ASIDContBufTiny,    //First seq item on list
   ASIDContBufLastItem = ASIDContBufXXLarge, //Last seq item on list
   ASIDContBufMask     = 0x07,   //Mask to get zero based item #

   // queue message types/masks
   ASIDAddrType_Skip   = 0x00,   // No data/skip, also indicates End Of Frame
   ASIDAddrType_Char   = 0x20,   // Character data
   ASIDAddrType_Start  = 0x40,   // ASID Start message
   ASIDAddrType_Stop   = 0x60,   // ASID Stop message
   ASIDAddrType_SID1   = 0x80,   // Lower 5 bits are SID1 reg address
   ASIDAddrType_SID2   = 0xa0,   // Lower 5 bits are SID2 reg address 
   ASIDAddrType_SID3   = 0xc0,   // Lower 5 bits are SID3 reg address
   ASIDAddrType_Error  = 0xe0,   // Error from parser
                       
   ASIDAddrType_Mask   = 0xe0,   // Mask for Type
   ASIDAddrAddr_Mask   = 0x1f,   // Mask for Address
}; //end enum synch

#define RegValToBuffSize(X)  (1<<((X & ASIDContBufMask)+8)); // 256, 512, 1024, 2048, 4096, 8192; make sure MIDIRxBufSize is >= max (8192)         
#define ASIDRxQueueUsed      ((RxQueueHead>=RxQueueTail)?(RxQueueHead-RxQueueTail):(RxQueueHead+ASIDQueueSize-RxQueueTail))
#define FramesBetweenChecks  12    //frames between frame alignments check & timing adjust
#define SIDFreq50HzuS        19975 // (PAL) 19950=50.13Hz (SFII, real C64 HW),  20000=50.0Hz (DeepSID)
                                   //Splitting the difference for now, todo: standardize when DS updated!
#define SIDFreq60HzuS        16715 // (NTSC)

//ASID protocol packet types
#define APT_StartPlaying     0x4c
#define APT_StopPlayback     0x4d
#define APT_DisplayChars     0x4f
#define APT_SID1RegData      0x4e
#define APT_SID2RegData      0x50
#define APT_SID3RegData      0x51
#define APT_WriteOrder       0x30 
#define APT_ContFramerate    0x31
#define APT_SIDTypes         0x32

#ifdef DbgMsgs_IO  //Debug msgs mode
   #define Printf_dbg_SysExInfo {Serial.printf("\nSysEx: size=%d, data=", size); for(uint16_t Cnt=0; Cnt<size; Cnt++) Serial.printf(" $%02x", data[Cnt]);Serial.println();}
#else //Normal mode
   #define Printf_dbg_SysExInfo {}
#endif

#ifdef DbgSignalASIDIRQ
bool DbgInputState;  //togles LED on SysEx arrival
bool DbgOutputState; //togles debug signal on IRQ assert
#endif

#ifdef Dbg_SerASID
int32_t MaxB, MinB;
uint32_t MaxT, MinT;
uint32_t BufByteTarget;
#endif

bool QueueInitialized, FrameTimerMode;
int32_t DeltaFrames;
uint32_t ASIDQueueSize, NumPackets, TotalInituS, ForceIntervalUs, TimerIntervalUs;
uint8_t MutedVoiceFlags;

uint8_t ASIDidToReg[] = 
{
 //reg#,// bit/id
    0,  // 00
    1,  // 01
    2,  // 02
    3,  // 03
    5,  // 04
    6,  // 05
    7,  // 06
           
    8,  // 07
    9,  // 08
   10,  // 09
   12,  // 10
   13,  // 11
   14,  // 12
   15,  // 13

   16,  // 14
   17,  // 15
   19,  // 16
   20,  // 17
   21,  // 18
   22,  // 19
   23,  // 20

   24,  // 21
    4,  // 22
   11,  // 23
   18,  // 24
    4,  // 25 <= secondary for reg 04
   11,  // 26 <= secondary for reg 11
   18,  // 27 <= secondary for reg 18
};

void InitTimedASIDQueue()
{
   Printf_dbg("\nQueue Init\n");
   RxQueueHead = RxQueueTail = 0;
   ASIDPlaybackTimer.end();  // Stop the timer (if on)
   QueueInitialized = false;
   NumPackets = TotalInituS = 0;
}

void AddToASIDRxQueue(uint8_t Addr, uint8_t Data)
{
  if (ASIDRxQueueUsed >= ASIDQueueSize-2)
  {
     //Printf_dbg(">");
     Printf_dbg("\n**Queue Overflow\n");
     if (FrameTimerMode) InitTimedASIDQueue(); 
     return;
  }
  
  MIDIRxBuf[RxQueueHead] = Addr;
  MIDIRxBuf[RxQueueHead+1] = Data;
  
  if (RxQueueHead == ASIDQueueSize-2) RxQueueHead = 0;
  else RxQueueHead +=2;
}

void SetASIDIRQ()
{   
   if (FrameTimerMode) return;
   
   if(MIDIRxIRQEnabled)
   {
      SetIRQAssert;
   #ifdef DbgSignalASIDIRQ      
      DbgOutputState = !DbgOutputState;
      if (DbgOutputState) SetDebugAssert;
      else SetDebugDeassert;
   #endif
   }
   else
   {
      Printf_dbg("ASID IRQ not enabled\n");
      RxQueueHead = RxQueueTail = 0;
   }
}

void AddErrorToASIDRxQueue()
{
   AddToASIDRxQueue(ASIDAddrType_Error, 0);
   SetASIDIRQ();
}

void DecodeSendSIDRegData(uint8_t SID_ID, uint8_t *data, unsigned int size) 
{
   unsigned int NumRegs = 0; //number of regs to write
   
   //Printf_dbg("SID$%02x reg data\n", SID_ID);
   for(uint8_t maskNum = 0; maskNum < 4; maskNum++)
   {
      for(uint8_t bitNum = 0; bitNum < 7; bitNum++)
      {
         if(data[3+maskNum] & (1<<bitNum))
         { //reg is to be written
            uint8_t RegVal = data[11+NumRegs];
            if(data[7+maskNum] & (1<<bitNum)) RegVal |= 0x80;
            AddToASIDRxQueue((SID_ID | ASIDidToReg[maskNum*7+bitNum]), RegVal);
            #ifdef DbgMsgs_IO  
               //// Debug msgs for secondary reg or higher access
               //if(maskNum*7+bitNum>24)
               //{
               //   Serial.printf("High Reg: %d(->%d) = %d", maskNum*7+bitNum, ASIDidToReg[maskNum*7+bitNum], RegVal);
               //   Printf_dbg_SysExInfo;
               //}
            #endif  
            NumRegs++;
            //Printf_dbg("#%d: reg $%02x = $%02x\n", NumRegs, ASIDidToReg[maskNum*7+bitNum], RegVal);
         }
      }
   }
   if(12+NumRegs > size)
   {
      AddErrorToASIDRxQueue();
      Printf_dbg("-->More regs expected (%d) than provided (%d)", NumRegs, size-12);    
      Printf_dbg_SysExInfo;
   }
   SetASIDIRQ();  
   if (FrameTimerMode) AddToASIDRxQueue(ASIDAddrType_Skip, 0); //mark End Of Frame
}

FASTRUN void SendTimedASID()
{ //called by timer to send next ASID frame (activate IRQ)
  
   //if (!TimerIntervalUs || !QueueInitialized) return;
   
   uint32_t LocalASIDRxQueueUsed = ASIDRxQueueUsed; 
   
   if (LocalASIDRxQueueUsed < 2)
   {
      //Printf_dbg("<");
      Printf_dbg("\n**Queue Underflow\n");
      InitTimedASIDQueue(); //re-buffer if queue empty
      return;
   }
   
   if (MIDIRxIRQEnabled) // && LocalASIDRxQueueUsed > 1)
   {
      SetIRQAssert; //Trigger read by C64, start of frame
      DeltaFrames++; // qualify as SID1 frame start?
   #ifdef DbgSignalASIDIRQ
      DbgOutputState = !DbgOutputState;
      if (DbgOutputState) SetDebugAssert;
      else SetDebugDeassert;
   #endif
   }
   
   //adjust timer interval if based on frame offset from stream, if needed
   static uint16_t HystCount = 0;
   
   if (HystCount)
   {
      HystCount--;
      return;
   }
   
   if (DeltaFrames)
   {  //only adjust if off by 1 or more frames
      TimerIntervalUs += DeltaFrames; //adjust timer by 1uS for each frame off of aligned 
      HystCount = FramesBetweenChecks; //frames between frame alignments check & timing adjust
      ASIDPlaybackTimer.update(TimerIntervalUs);  //current interval is completed, then the next interval begins with this setting 
      
#ifdef Dbg_SerASID
      Serial.printf("%+d", DeltaFrames);
      
      int32_t DeltaTarget = LocalASIDRxQueueUsed - BufByteTarget;
      if (DeltaTarget>MaxB)
      {
         MaxB = DeltaTarget;
         Serial.printf("\n*MaxB:%+d  ", MaxB);
      }
      if (DeltaTarget<MinB)
      {
         MinB = DeltaTarget;
         Serial.printf("\n*MinB:%+d  ", MinB);
      }
      if (TimerIntervalUs>MaxT)
      {
         MaxT = TimerIntervalUs;
         //Serial.printf("\n*MaxT:%lu uS", MaxT);
      }
      if (TimerIntervalUs<MinT)
      {
         MinT = TimerIntervalUs;
         //Serial.printf("\n*MinT:%lu uS", MinT);
      }
#endif

   }  
}

//MIDI input handlers for HW Emulation _________________________________________________________________________

// F0 SysEx single call, message larger than buffer is truncated
void ASIDOnSystemExclusive(uint8_t *data, unsigned int size) 
{
   //data already contains starting f0 and ending f7
   //Printf_dbg_SysExInfo;
   #ifdef DbgSignalASIDIRQ      
      // Use *LED signal* to communicate SysEx Packet Receipt from host
      DbgInputState = !DbgInputState;
      if (DbgInputState) SetLEDOn;
      else SetLEDOff;
   #endif

   // ASID decode based on:   http://paulus.kapsi.fi/asid_protocol.txt
   // originally by Elektron SIDStation
      
   if(data[0] != 0xf0 || data[1] != 0x2d || data[size-1] != 0xf7)
   {
      AddErrorToASIDRxQueue();
      Printf_dbg("-->Invalid ASID/SysEx format");
      Printf_dbg_SysExInfo;
      return;
   }
   //appears to be a valid ASID packet.
   
   switch(data[2])
   {      
      case APT_StartPlaying: //start playing message
         AddToASIDRxQueue(ASIDAddrType_Start, 0);
         Printf_dbg("APT_StartPlaying\n");
         SetASIDIRQ();
         break;
      case APT_StopPlayback: //stop playback message
         AddToASIDRxQueue(ASIDAddrType_Stop, 0);
         Printf_dbg("APT_StopPlayback\n");
         SetASIDIRQ();
         break;
      case APT_DisplayChars: //Display Characters
         data[size-1] = 0; //replace 0xf7 with term
         Printf_dbg("APT_DisplayChars: \"%s\"\n", data+3);
         for(uint8_t CharNum=3; CharNum < size-1 ; CharNum++)
         {
            AddToASIDRxQueue(ASIDAddrType_Char, ToPETSCII(data[CharNum]));
         }
         AddToASIDRxQueue(ASIDAddrType_Char, 13);
         SetASIDIRQ();
         break;
      case APT_SID1RegData:  //SID1 reg data (primary)
         if (FrameTimerMode && !QueueInitialized)
         { //check packet receive rate during queue init
            static uint32_t LastMicros;
            uint32_t NewMicros = micros();
            
            if(NumPackets)  //skip first val to only update LastMicros
            {
               TotalInituS += (NewMicros - LastMicros);
               //Printf_dbg("Pkt %d: %lu uS, avg: %lu uS\n", NumPackets, (NewMicros- LastMicros), TotalInituS/NumPackets);
               //BigBuf[BigBufCount++] = NewMicros - LastMicros;
            }
            LastMicros = NewMicros;
   
            //if buffer 1/3 full or been >2 seconds, start timer to kick off playback...
            if (ASIDRxQueueUsed >= ASIDQueueSize/3 || TotalInituS >= 2000000) 
            {
               if (NumPackets) TimerIntervalUs = TotalInituS/NumPackets;
               else 
               { // no frames timed
                  InitTimedASIDQueue();
                  return;
               }
               
               Printf_dbg("Q Init Done\n %d frames, %lu bytes, %lu bytes/frame, %lu uS total, %lu uS/frame\n", NumPackets, ASIDRxQueueUsed, ASIDRxQueueUsed/NumPackets, TotalInituS, TimerIntervalUs);
               if (ForceIntervalUs) TimerIntervalUs = ForceIntervalUs;
               Printf_dbg(" Timer set: %lu uS\n", TimerIntervalUs);
               
#ifdef Dbg_SerASID
               BufByteTarget = ASIDRxQueueUsed;
               MaxB = MinB = 0;
               MaxT = MinT = TimerIntervalUs;
#endif
               DeltaFrames = 0;
               QueueInitialized = true;
               ASIDPlaybackTimer.begin(SendTimedASID, TimerIntervalUs);  // Start the timer
               ASIDPlaybackTimer.priority(250);  //low priority
            }
            NumPackets++;
         }
         DeltaFrames--;
         DecodeSendSIDRegData(ASIDAddrType_SID1, data, size);
         break;
      case APT_SID2RegData:  //SID2 reg data
         DecodeSendSIDRegData(ASIDAddrType_SID2, data, size);
         break;
      case APT_SID3RegData:  //SID3 reg data
         DecodeSendSIDRegData(ASIDAddrType_SID3, data, size);
         break;         
      case APT_WriteOrder:   
         Printf_dbg("APT_WriteOrder:");
         Printf_dbg_SysExInfo;
         for(uint16_t Cnt=0; Cnt<size-4; Cnt+=2)
         {
            uint8_t Regnum = Cnt/2;
            uint8_t IndexOrder = data[Cnt+3] & 0x3f;
            uint8_t WaitCycles = ((data[Cnt+3] & 0x40)<<1) | data[Cnt+4];
            Printf_dbg("   reg %02d  #%02d  %3d cyc\n", Regnum, IndexOrder, WaitCycles);
         }
         break;         
      case APT_ContFramerate:
         Printf_dbg("APT_ContFramerate:");
         Printf_dbg_SysExInfo;
         //  data0: settings
         Printf_dbg("   Expected Vid: %s\n", ((data[3]&1) ? "NTSC":"PAL")); //bit0: 0 = PAL, 1 = NTSC
         Printf_dbg("   Speed Mult: %dx\n", ((data[3]&0x1E)>>1)+1); //bits4-1: speed, 1x to 16x
         Printf_dbg("   Buffering requested: %s\n", ((data[3]&4) ? "Yes":"No")); //bit6: 1 = buffering requested by user
         
         ForceIntervalUs = (data[3]&1) ? SIDFreq60HzuS:SIDFreq50HzuS; //default to SID NTSC/PAL timing
         Printf_dbg("   Frame Delta Default: %luuS\n", ForceIntervalUs);
         //Use the speed from data0 in case:
         //* No fancy timing system exists on the client
         //* if framedelta is 0      
         
         if (size>=8)  // framedelta is optional argument
         {
            //  data1-3: framedelta uS, total 16 bits (lsb first), slowest time = 65535us = 15Hz
            uint32_t TempUs = ((data[6]&3)<<14)|(data[5]<<7)|(data[4]);
            Printf_dbg("   Frame Delta Sent: %luuS\n", TempUs);
            if(TempUs) ForceIntervalUs = TempUs;
         }
         
         //if ((data[3]&4) //bit6: 1 = buffering requested by user
         {
            //ForceIntervalUs = SIDFreq50HzuS;
            Printf_dbg("   Timer Set: %luuS (%.2fHz)\n", ForceIntervalUs, ((float)1000000/ForceIntervalUs));
            //FrameTimerMode = true;
            //InitTimedASIDQueue();
         }
         
         break;         
      case APT_SIDTypes:     
         Printf_dbg("APT_SIDTypes:");
         Printf_dbg_SysExInfo;
         Printf_dbg("   Index: %d (SID%d)\n", data[3], data[3]+1);
         Printf_dbg("   Type: %d\n", ((data[4]&1) ? 8580:6581));
         Printf_dbg("   Variant: %d\n", (data[4]>>1));
         break;         
      default:
         AddErrorToASIDRxQueue();
         Printf_dbg("-->Unexp  ASID Packet Type: $%02x", data[2]);
         Printf_dbg_SysExInfo;
   }
}


//____________________________________________________________________________________________________

void InitHndlr_ASID()  
{
   Printf_dbg("ASID Queue Size: %d\n", ASIDQueueSize);

   if (MIDIRxBuf==NULL) MIDIRxBuf = (uint8_t*)malloc(MIDIRxBufSize);
   
   nfcState |= nfcStateBitPaused; // Pause NFC for time critical routine
   NVIC_DISABLE_IRQ(IRQ_ENET); // disable ethernet interrupt during ASID
   NVIC_DISABLE_IRQ(IRQ_PIT);

   MutedVoiceFlags = 0;
   ForceIntervalUs = 0;  // 0 to ignore and use timed val, 19950=PAL, 16715=NTSC
   FrameTimerMode = false; //initialize to off, synched with asm code default: memFrameTimer
   ASIDQueueSize = RegValToBuffSize(ASIDContBufMedium); //Initialize to match memBufferSize default
   InitTimedASIDQueue(); //stops timer, clears queue
   
   // SetMIDIHandlersNULL(); is called prior to this, 
   //    all other MIDI messages ignored.
   usbHostMIDI.setHandleSystemExclusive     (ASIDOnSystemExclusive);     // F0
   usbDevMIDI.setHandleSystemExclusive      (ASIDOnSystemExclusive);     // F0
}

void IO1Hndlr_ASID(uint8_t Address, bool R_Wn)
{
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      switch(Address)
      {
         case ASIDAddrReg:
            if(ASIDRxQueueUsed)
            {
               DataPortWriteWaitLog(MIDIRxBuf[RxQueueTail]); 
            }
            else  //no address to send, send error message (should not happen)
            {
               DataPortWriteWaitLog(ASIDAddrType_Error);
            }
            break;
         case ASIDDataReg:
            if(ASIDRxQueueUsed)
            {
               DataPortWriteWaitLog(MIDIRxBuf[RxQueueTail+1]);  
               RxQueueTail+=2;  //inc on data read, must happen after address read
               if (RxQueueTail == ASIDQueueSize) RxQueueTail = 0;
            }
            else  //no data to send, send 0  (should not happen)
            {
               DataPortWriteWaitLog(0);
            }
            
            if(ASIDRxQueueUsed == 0) SetIRQDeassert;  //remove IRQ if queue empty        
            else
            {
               if (FrameTimerMode && MIDIRxBuf[RxQueueTail] == ASIDAddrType_Skip) //EOFrame
               {
                  SetIRQDeassert;  //remove IRQ if End Of Frame
                  RxQueueTail+=2;  //Skip sending the EOF marker
                  if (RxQueueTail == ASIDQueueSize) RxQueueTail = 0;   
                  //DeltaFrames++;  //moved to start of packet send instead of here at end
               }
            }
            break;
         case ASIDQueueUsed:
            DataPortWriteWaitLog(32*ASIDRxQueueUsed/ASIDQueueSize);
            //{  //test bar graph display
            //   static uint8_t num = 0;
            //   DataPortWriteWaitLog(num++);
            //   if(num==32) num=0;
            //}
            break;
         //default:
            //leave other locations available for potential SID in IO1
            //DataPortWriteWaitLog(0); 
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      uint8_t Data = DataPortWaitRead(); 
      if (Address == ASIDContReg)
      {
         switch(Data)
         {
            case ASIDContTimerOff:
               FrameTimerMode = false;
               InitTimedASIDQueue();
               Printf_dbg("Timer Off\n");
               break;
            case ASIDContTimerOnAuto:
               FrameTimerMode = true;
               ForceIntervalUs = 0;
               InitTimedASIDQueue();
               Printf_dbg("Timer On-Auto\n");
               break;
            case ASIDContTimerOn50Hz:
               FrameTimerMode = true;
               ForceIntervalUs = SIDFreq50HzuS;
               InitTimedASIDQueue();
               Printf_dbg("Timer On-50Hz\n");
               break;               
            case ASIDContIRQOn:
               MIDIRxIRQEnabled = true;
               RxQueueHead = RxQueueTail = 0;
               Printf_dbg("ASIDContIRQOn\n");
               break;
            case ASIDContIRQOff:
               MIDIRxIRQEnabled = false;
               RxQueueHead = RxQueueTail = 0;
               Printf_dbg("ASIDContIRQOff\n");
               break;
            case ASIDContExit:
               MIDIRxIRQEnabled = false;
               BtnPressed = true;   //main menu
               Printf_dbg("ASIDContExit\n");
               break;
            case ASIDContBufFirstItem ... ASIDContBufLastItem:
               ASIDQueueSize = RegValToBuffSize(Data); 
               InitTimedASIDQueue();
               Printf_dbg("Buf Size set to %d (%d bytes)\n", (Data & ASIDContBufMask), ASIDQueueSize);
               break;
            default:
               Printf_dbg("Unk ASIDContReg= %02x ?\n", Data);
               break;            
         }
      }
      else if (Address == ASIDVoiceMuteReg)
      {
         Printf_dbg("ASIDVoiceMuteReg = %02x\n", Data);
         MutedVoiceFlags = Data; //update to flag main routine to write zeros to muted voice regs
         for(uint8_t bitnum=0; bitnum<3; bitnum++)
         {
            if (Data & (1<<bitnum))
            { //voice is muted, write voice CR to garbage reg (cleared in main loop)
               ASIDidToReg[22+bitnum]= 25; //point reg decoder for voice CR to bit-bucket reg (PotX read-only)
               ASIDidToReg[25+bitnum]= 26; //point secondary reg decoder for voice CR to bit-bucket reg (PotX read-only)
            }
            else
            { //voice not muted, point to default reg
               ASIDidToReg[22+bitnum]= 4+bitnum*7; //Correctly point reg decoder for voice CR 
               ASIDidToReg[25+bitnum]= 4+bitnum*7; //Correctly point secondary reg decoder for voice CR 
            }
            // reg#, bit/index  ASIDidToReg defaults
            //    4,   22
            //   11,   23
            //   18,   24
            //    4,   25 <= secondary for reg 04
            //   11,   26 <= secondary for reg 11
            //   18,   27 <= secondary for reg 18
         }
      }
      
   }  // IO1 write
}

void PollingHndlr_ASID()
{
   if(MutedVoiceFlags)
   {
      for(uint8_t bitnum=0; bitnum<3; bitnum++)
      {
         if (MutedVoiceFlags & (1<<bitnum))
         { //voice is muted, clear it
            AddToASIDRxQueue(ASIDAddrType_SID1 | (4+bitnum*7), 0); //add command to clear voice CR reg @4/11/18
            SetASIDIRQ();
         }
      }
      MutedVoiceFlags = 0;
   }
   
   if(ASIDRxQueueUsed == 0 || FrameTimerMode) //read MIDI-in data in only if ready to send to C64 (buffer empty)
   {
      usbDevMIDI.read();
      if(ASIDRxQueueUsed == 0 || FrameTimerMode) usbHostMIDI.read();   //Currently no use case (Hosted ASID source) 
   }
}

