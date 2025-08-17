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

#ifdef Dbg_SerSwift
   //RxIn 497+551=1048
   //big 8-bit playground buffer for testing
  PROGMEM const uint8_t TestBuf[] ={
      168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,168,13,32,32,
      18,28,220,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,162,162,162,32,172,162,162,162,172,162,162,162,
      187,32,44,146,166,13,32,32,18,220,32,32,161,146,223,18,32,32,32,32,162,146,169,18,32,172,161,32,146,223,18,32,
      146,161,18,152,190,153,162,30,188,28,161,146,161,18,32,150,162,129,188,146,32,18,154,32,156,32,5,32,28,161,206,223,
      146,13,32,32,18,220,146,190,18,162,146,158,58,46,28,184,188,183,158,58,46,58,28,188,32,158,46,18,28,162,146,158,
      58,32,28,190,18,153,187,146,30,162,18,151,172,146,129,172,28,187,18,150,32,146,129,162,18,31,172,146,32,156,187,18,
      5,32,146,156,172,18,28,190,180,146,219,165,13,32,32,18,156,220,163,163,163,163,163,163,163,163,163,163,163,163,163,163,
      163,163,146,161,18,30,32,146,32,18,129,32,146,28,188,150,190,18,129,32,146,32,18,154,32,5,161,146,161,18,152,32,
      156,161,167,175,175,146,13,32,32,18,220,32,220,32,32,32,32,32,32,32,32,32,32,46,32,32,32,188,146,151,188,18,
      129,162,146,28,190,18,156,190,146,161,18,31,162,154,162,146,156,190,5,188,151,190,18,162,154,161,172,31,190,146,220,13,
      32,32,154,188,18,185,185,185,185,168,175,175,175,164,164,164,164,39,185,185,185,185,185,185,185,185,164,175,175,175,175,185,
      185,185,146,156,184,31,168,59,13,32,32,151,167,18,31,32,32,32,32,163,163,220,163,183,183,183,183,184,32,32,32,32,
      32,166,220,32,183,163,163,163,163,168,32,32,32,168,146,151,165,13,32,32,167,18,5,32,32,146,161,18,32,31,32,32,
      5,190,32,167,32,31,32,5,32,31,32,5,190,32,167,32,32,31,32,5,190,32,188,165,31,32,5,167,32,31,166,5,
      167,32,32,188,146,151,165,13,32,32,167,18,5,32,31,163,5,167,32,31,32,32,5,32,31,163,5,167,32,31,32,5,
      32,31,32,5,32,31,44,163,5,32,31,163,5,167,32,31,163,5,167,32,31,32,5,167,32,146,187,18,167,32,31,163,
      5,32,146,151,165,13,32,32,170,18,5,32,31,32,5,167,32,31,32,32,5,32,31,166,5,167,187,32,172,31,32,5,
      32,31,33,32,5,32,31,32,5,167,32,31,32,5,167,32,31,58,5,167,32,32,167,32,31,166,5,32,146,151,165,13,
      32,32,167,18,5,32,32,146,190,18,32,31,166,32,5,32,32,167,31,182,5,32,31,32,32,5,32,146,182,18,182,32,
      32,146,161,18,32,31,46,5,167,32,31,32,5,167,32,146,188,18,167,32,31,161,5,32,146,151,165,13,32,32,167,18,
      5,32,146,31,166,18,166,5,32,146,31,58,18,166,5,32,146,31,33,18,5,32,146,31,221,18,5,32,31,187,166,5,
      32,31,46,5,167,32,31,166,5,167,32,31,187,5,167,32,31,172,5,167,32,146,31,58,18,5,167,32,146,31,39,18,
      5,32,146,151,165,13,32,32,167,18,5,32,146,31,33,166,18,5,32,32,170,32,146,31,58,18,5,32,146,31,33,18,
      5,32,146,32,18,31,182,5,32,32,167,32,146,32,18,167,187,32,172,146,188,18,32,146,190,18,32,146,151,164,18,5,
      167,32,32,172,146,151,165,13,32,32,32,163,163,163,152,172,187,151,163,163,163,163,163,152,191,151,163,163,18,152,191,146,
      151,163,163,163,163,163,152,191,18,191,146,151,163,163,163,163,163,152,172,18,32,146,187,151,163,163,13,32,32,32,32,172,
      18,162,162,152,162,162,146,187,32,32,151,172,18,172,32,152,32,187,146,187,32,32,18,151,190,187,162,152,162,172,188,146,
      32,32,151,172,18,172,32,152,187,146,187,13,32,32,32,32,18,149,161,146,151,185,18,188,190,146,152,185,161,32,32,18,
      149,172,151,32,32,32,152,32,187,146,32,32,18,151,172,32,32,32,152,32,187,146,32,32,32,18,151,187,185,172,146,13,
      32,32,32,32,32,18,149,191,146,32,32,151,191,32,32,32,32,18,149,188,146,32,32,18,151,190,146,32,32,32,149,190,
      18,190,151,162,162,188,146,152,188,32,32,32,18,149,191,151,32,146,191,13,13,32,32,32,195,85,82,82,69,78,84,76,
      89,32,68,79,87,78,32,70,79,82,32,77,65,73,78,84,69,78,65,78,67,69,44,13,32,32,32,32,32,32,80,76,
      69,65,83,69,32,67,65,76,76,32,66,65,67,75,32,76,65,84,69,82,46,13,13,13,13,13,13
   };
#endif


FLASHMEM void ServiceSerial(Stream *ThisCmdChannel)
{  //ThisCmdChannel->available() confirmed before calling
   CmdChannel = ThisCmdChannel;

   if (CurrentIOHandler == IOH_TR_BASIC) return; //special case, handler will take care of serial input
   
   uint16_t inVal = CmdChannel->read();
   switch (inVal)
   {
      case 0x64: //'d' command from app
         if(!SerialAvailabeTimeout()) return;
         inVal = (inVal<<8) | CmdChannel->read(); //READ NEXT BYTE
         
         //only commands available when busy:
         if (inVal == ResetC64Token) //Reset C64
         {
            CmdChannel->println("Reset cmd received");  //UI looks for this string, do not change.
            BtnPressed = true;
            return;
         }
         else if (inVal == LaunchFileToken) //Launch File
         {
            LaunchFile();
            return;
         }
         else if (inVal == C64PauseOnToken) //pause C64 via DMA, during next bad line
         {
            DMA_State = DMA_S_StartActive;
            // DMA_State = DMA_S_Start_BA_Active; //doesn't work with U64? scope it...
            SendU16(AckToken);
            return;
         }
         else if (inVal == C64PauseOffToken) //un-pause C64 DMA
         {
            DMA_State = DMA_S_StartDisable;
            SendU16(AckToken);
            return;
         }
            
         if (CurrentIOHandler != IOH_TeensyROM)
         {
            SendU16(FailToken);
            CmdChannel->print("Busy!\n");
            return;
         }
         //TeensyROM IO Handler is active...
         
         switch (inVal)
         {
            case PingToken:  //ping
               CmdChannel->printf("TeensyROM %s ready!\n", strVersionNumber);
               break;
            case SendFileToken: //file x-fer pc->TR
            case PostFileToken:  // v2 file x-fer pc->TR.  For use with v2 UI.
               PostFileCommand();
               break;
            case CopyFileToken:
               CopyFileCommand();
               break;
            case DeleteFileToken:
               DeleteFileCommand();
               break;
            case GetFileToken:
               GetFileCommand();
               break;
            case GetDirectoryToken:  // v2 directory listing from TR
               GetDirectoryCommand();
               break;
            case PauseSIDToken: //Pause SID
               if(RemotePauseSID()) SendU16(AckToken);
               else SendU16(FailToken);
               break;
            case SIDSpeedLinToken: //Set playback speed (Linear)
            case SIDSpeedLogToken: //Set playback speed (Logrithmic)
               if(RemoteSetSIDSpeed(inVal==SIDSpeedLogToken)) SendU16(AckToken);
               else SendU16(FailToken);
               break;
            case SetSIDSongToken: //Set sub-tune
               if(SetSIDSong()) SendU16(AckToken);
               else SendU16(FailToken);
               break;
            case SIDVoiceMuteToken: //Set Individual Voice muting
               if(RemoteSetSIDVoiceMute()) SendU16(AckToken);
               else SendU16(FailToken);
               break;
            case SetColorToken: //Set a TR UI color value
               if(SetColorRef()) SendU16(AckToken);
               else SendU16(FailToken);
               break;
            case DebugToken: //'dg'Test/debug
               //for (int a=0; a<256; a++) CmdChannel->printf("\n%3d, // %3d   '%c'", ToPETSCII(a), a, a);
               //PrintDebugLog();
               //nfcInit();
               //Printf_dbg("isFab2x: %d\n", isFab2x()); 
               USBHostSerial.print("USB Host Serial\n");
               Serial.print("Sent:USB Host Serial\n");
               break;
            default:
               CmdChannel->printf("Unk cmd: 0x%04x\n", inVal); 
               break;
         }
         break;
      case 'e': //Reset EEPROM to defaults
         SetEEPDefaults();
         CmdChannel->println("Applied upon reboot");
         break;
      case 'v': //version info
         MakeBuildInfo();
         CmdChannel->printf("\nTeensyROM %s\n%s\n", strVersionNumber, SerialStringBuf);
         break;
      case 'b': //bus analysis
         BusAnalysis();
         break;
         
// *** The rest of these cases are used for debug/testing only  
     
      //case 'u':  //set up autolaunch
      //   EEPROM.write(eepAdAutolaunchName, 0); //disable auto Launch
      //   CmdChannel->printf("Autolaunch disabled\n");
      //   
      //   //EEPwriteStr(eepAdAutolaunchName, "USB:multimedia/totaleclipse-fth.prg");
      //   //CmdChannel->printf("Autolaunch set\n");
      //   
      //   //RemoteLaunch(rmtUSBDrive, "multimedia/totaleclipse-fth.prg", false);
      //   //RemoteLaunch(rmtSD, "games/minesweeper game.prg", false);
      //   //RemoteLaunch(rmtTeensy, "Cynthcart 2.0.1      +Datel MIDI ", false);
      //   break;
     
      //case 'u':  //Reboot to minimal build
      //   //EEPwriteStr(eepAdCrtBootName, "/OneLoad v5/Main- MagicDesk CRTs/Auriga.crt");
      //   EEPwriteStr(eepAdCrtBootName, "/validation/FileSize/Briley Witch Chronicles 2 v1.0.2.crt");
      //   EEPROM.write(eepAdMinBootInd, MinBootInd_ExecuteMin);         
      //   REBOOT;
      //   break;

   // q, a, i
   #ifdef Dbg_SerASID
      case 'q':   //display queue size, rate, etc
         CmdChannel->printf("\nASID Timer: Ena: %s, Qinitd: %s\n", (FrameTimerMode ? "true" : "false"), (QueueInitialized ? "true" : "false"));
         CmdChannel->printf(" Config  : QSize:%lu, FrmsBetChks:%d\n", ASIDQueueSize, FramesBetweenChecks);
         CmdChannel->printf(" Current : QUsed:%lu, QInit: %lu, DeltaFrames:%d\n", ASIDRxQueueUsed, BufByteTarget, DeltaFrames);
         CmdChannel->printf(" QInitChg: Curr: %d, Max:%+d, Min:%+d, diff:%d bytes\n", (ASIDRxQueueUsed - BufByteTarget), MaxB, MinB, MaxB-MinB);
         CmdChannel->printf(" Interval: Curr: %lu, Max:%lu, Min:%lu, diff:%lu uS\n", TimerIntervalUs, MaxT, MinT, MaxT-MinT);
         break;
      case 'a': //ASID Packet load
      {
         uint8_t VolReg = 0;
         uint16_t RegsInPacket = 20, NumPackets = 1000;  // (ASIDQueueSize-2)/2;
         uint32_t StartMicros, MaxMicros = 0, MinMicros = 100000;
         for(uint32_t PacketNum = 0; PacketNum<NumPackets; PacketNum++)
         {
            for(uint8_t CmdNum=0; CmdNum<RegsInPacket; CmdNum++)
            {
               AddToASIDRxQueue(ASIDAddrType_SID1 | 0x18, VolReg); //toggle volume reg
               VolReg ^= 0x0f;
            }
            StartMicros = micros();
            SetASIDIRQ();
            while(ASIDRxQueueUsed) if (micros() - StartMicros > 100000) break;
            StartMicros = micros() - StartMicros;
            if(StartMicros>MaxMicros) MaxMicros=StartMicros;
            if(StartMicros<MinMicros) MinMicros=StartMicros;
            CmdChannel->printf("Took %luuS to send %d Regs\n", StartMicros, RegsInPacket);
            CmdChannel->flush();
            delay(10); //allow scope time to re-arm
         }
         CmdChannel->printf(" %d packets, %luuS Min to %luuS Max\n", NumPackets, MinMicros, MaxMicros);
      }
         break;
      case 'i': //Send text via ASID
      {
         AddToASIDRxQueue(ASIDAddrType_Start, 0);
         PrintflnToASID("This is a long character test, it just  keeps going for 3 continuous lines of   text. 7 Just when you think it's done.");
         AddToASIDRxQueue(ASIDAddrType_Stop, 0);
         FlushASIDRxQueue();
      }
         break;
   #endif
   
   // l, c
   #ifdef Dbg_SerLog 
      case 'l':  //Show Debug Log
         PrintDebugLog();
         break;
      case 'c': //Clear Debug Log buffer
         BigBufCount = 0;
         CmdChannel->println("Buffer Reset");
         break;       
   #endif
         
   // f, x
   #ifdef Dbg_SerMem 
      case 'f': //show build info+free mem.  Menu must be idle, interferes with any serialstring in progress
         {
            MakeBuildInfo();
            CmdChannel->println("\n***** Build & Mem info *****");
            CmdChannel->println(SerialStringBuf);
            CmdChannel->printf("RAM2 Bytes Free: %lu (%luK)\n\n", RAM2BytesFree(), RAM2BytesFree()/1024);
            memInfo();
            getFreeITCM();
 
            CmdChannel->printf("\nMem usage:\n");
            uint32_t TotalSize = 0;
            if(DriveDirMenu != NULL) 
            {
               for(uint16_t Num=0; Num < NumDrvDirMenuItems; Num++) TotalSize += strlen(DriveDirMenu[Num].Name)+1;
               CmdChannel->printf("Filename chars: %lu (%luk) @ $%08x\nDriveDirMenu: %lu (%luk) @ $%08x\n", 
                  TotalSize, TotalSize/1024, (uint32_t)DriveDirMenu[0].Name,
                  MaxMenuItems*sizeof(StructMenuItem), MaxMenuItems*sizeof(StructMenuItem)/1024, (uint32_t)DriveDirMenu);
               TotalSize += MaxMenuItems*sizeof(StructMenuItem);
            }
            CmdChannel->printf("DriveDirMenu+Filename chars: %lu (%luk)\n", 
               TotalSize, TotalSize/1024);
            
            CmdChannel->printf("RAM_Image: %lu (%luk) @ $%08x\n", 
               sizeof(RAM_Image), sizeof(RAM_Image)/1024, (uint32_t)RAM_Image);
         
            TotalSize = 0;
            uint32_t TotalStructSize = sizeof(TeensyROMMenu);
            uint32_t FileCount = 0;
            for(uint8_t ROMNum=0; ROMNum < sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]); ROMNum++)
            {
               if(TeensyROMMenu[ROMNum].ItemType == rtDirectory)
               {
                  StructMenuItem *subTROMMenu = (StructMenuItem*)TeensyROMMenu[ROMNum].Code_Image;
                  TotalStructSize += TeensyROMMenu[ROMNum].Size;
                  for(uint8_t subROMNum=0; subROMNum < TeensyROMMenu[ROMNum].Size/sizeof(StructMenuItem); subROMNum++)
                  {
                     if(subTROMMenu[subROMNum].ItemType != rtDirectory) AddAndCheckSource(subTROMMenu[subROMNum], &TotalSize, &FileCount);
                  }
               }
               else AddAndCheckSource(TeensyROMMenu[ROMNum], &TotalSize, &FileCount);
            }
            CmdChannel->printf("TeensyROMMenu/sub struct: %lu (%luk) @ $%08x\n", 
               TotalStructSize, TotalStructSize/1024, (uint32_t)TeensyROMMenu);
            CmdChannel->printf("TeensyROMMenu/sub (%d) Items: %d (%dk) of Flash\n\n", FileCount, TotalSize, TotalSize/1024);
         }
         break;
      case 'x': 
         { 
            FreeDriveDirMenu(); //Will mess up navigation if not on TR menu!
            
            uint32_t CrtMax = (RAM_ImageSize & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            CmdChannel->printf("\n\nRAM1 Buff: %luK (%lu blks)\n", CrtMax, CrtMax/8);
            
            //uint32_t RAM2Free = (RAM2BytesFree() & 0xffffe000)/1024; //round down to k bytes rounded to nearest 8k
            //CmdChannel->printf("RAM2 Free: %luK (%lu blks)\n", RAM2Free, RAM2Free/8);
            
            uint8_t NumChips = RAM2blocks();
            //CmdChannel->printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
            NumChips = RAM2blocks()-1; //do it again, sometimes get one more, minus one to match reality, not clear why
            CmdChannel->printf("RAM2 Blks: %luK (%lu blks)\n", NumChips*8, NumChips);
           
            CrtMax += NumChips*8;
            CmdChannel->printf("  CRT Max: %luK (%lu blks) ~%luK file\n", CrtMax, CrtMax/8, (uint32_t)(CrtMax*1.004));
            //larger File size due to header info.
         }
         break;
   #endif
   
   // m, p, k, r, s
   #ifdef Dbg_SerSwift
      case 'm':      
         for(uint16_t Cnt=0; Cnt<sizeof(TestBuf); Cnt++) AddRawCharToRxQueue(TestBuf[Cnt]);
         break;
      case 'p':
         AddToPETSCIIStrToRxQueueLN("0123456789abcdef");
         break;
      case 'r': // rXXXXXX  set baud Rate directly
         {
            uint32_t NewBaud = 0;
            
            for(int8_t ByteNum=6-1; ByteNum>=0; ByteNum--)
            {
               if(!SerialAvailabeTimeout())
               {
                  CmdChannel->printf("6 digits required\n");
                  return;
               }
               uint32_t ByteIn = CmdChannel->read();
               if(ByteIn<'0' || ByteIn>'9')
               {
                  CmdChannel->printf("numbers only\n");
                  return;
               }
               NewBaud += ((ByteIn-'0') * pow(10,ByteNum));
            }
            C64CycBetweenRx = 985250*8/NewBaud;
            CmdChannel->printf("Manual baud set: %lubps, %lu Cycles\n", NewBaud, C64CycBetweenRx);
            //CmdChannel->printf("CycToBaud: %lubps\n", (985250*8/C64CycBetweenRx));
         }
         break;
      case 'k': //kill/reset status/NMI
         SwiftRegStatus = SwiftStatusDefault;
         SwiftRegCommand = SwiftCmndDefault;
         SwiftRegControl = 0;
         RxQueueHead = RxQueueTail = 0;
         //SwiftRegStatus &= ~(SwiftStatusRxFull | SwiftStatusIRQ); //no longer full, ready to receive more
         SetNMIDeassert;
         CmdChannel->printf("Swiftlink Reset\n"); 
         break;
      case 's': //status            
         CmdChannel->printf("Swiftlink status:\n"); 
         CmdChannel->printf("  client is");
         if (!client.connected()) CmdChannel->printf(" Not");
         CmdChannel->printf(" connected\n");
         
         CmdChannel->printf("  Rx Queue Used: %d\n", RxQueueUsed); 
         
         CmdChannel->printf("  RxIRQ is"); 
         if((SwiftRegCommand & SwiftCmndRxIRQDis) != 0) CmdChannel->printf(" Not"); 
         CmdChannel->printf(" enabled\n"); 
         
         CmdChannel->printf("  RxIRQ is");
         if((SwiftRegStatus & SwiftStatusIRQ) == 0) CmdChannel->printf(" Not"); 
         CmdChannel->printf(" set\n");
         
         CmdChannel->printf("  Command Reg: %02x\n", SwiftRegCommand);
         CmdChannel->printf("  Control Reg: %02x\n", SwiftRegControl);
         CmdChannel->printf("   Status Reg: %02x\n", SwiftRegStatus);
         CmdChannel->printf("Enh Speed Reg: %02x\n", TurboRegEnhancedSpeed);
         CmdChannel->printf("     RxOutBuf: %02x\n", SwiftRxBuf);
         
         break;
   #endif
   
   // t...
   #ifdef Dbg_SerTimChg
      case 't': //timing commands, 2 letters and 3-4 numbers
         CmdChannel->printf("-----\n");
         switch (CmdChannel->read())
         {
            case 'm': //nS_MaxAdj change
               GetDigits(4, &nS_MaxAdj);
               break;
            case 'r': //nS_RWnReady change
               GetDigits(3, &nS_RWnReady);
               break;
            case 'p': //nS_PLAprop change
               GetDigits(3, &nS_PLAprop);
               break;
            case 's': //nS_DataSetup change
               GetDigits(3, &nS_DataSetup);
               break;
            case 'h': //nS_DataHold change
               GetDigits(3, &nS_DataHold);
               break;
            case 'v': //nS_VICStart change
               GetDigits(3, &nS_VICStart);
               break;
            case 'i': //nS_VICDHold change
               GetDigits(3, &nS_VICDHold);
               break;
            case 'd': //Set Defaults
               nS_MaxAdj    = Def_nS_MaxAdj; 
               nS_PLAprop   = Def_nS_PLAprop;  
               nS_DataSetup = Def_nS_DataSetup;  
               nS_DataHold  = Def_nS_DataHold;  
               nS_VICStart  = Def_nS_VICStart;  
               nS_VICDHold  = Def_nS_VICDHold;
               CmdChannel->printf("Defaults set\n");
               CmdChannel->printf(" EEP RW_Ready delay: ");
               if (IO1[rwRegPwrUpDefaults] & rpudRWReadyDly) 
               {
                  nS_RWnReady = Def_nS_RWnReady_dly; //delay RW read timing
                  CmdChannel->printf("On\n");
               }
               else
               {
                  nS_RWnReady  = Def_nS_RWnReady;  
                  CmdChannel->printf("Off\n");
               }
               break;
            default:
               CmdChannel->printf("No changes\n");
               break;
         }
         CmdChannel->printf("Current:    Variable  Val (Command)\n");
         CmdChannel->printf("\t   nS_MaxAdj %04d (tm####)\n", nS_MaxAdj);
         CmdChannel->printf("\t nS_RWnReady  %03d (tr###)\n", nS_RWnReady);
         CmdChannel->printf("\t  nS_PLAprop  %03d (tp###)\n", nS_PLAprop);
         CmdChannel->printf("\tnS_DataSetup  %03d (ts###)\n", nS_DataSetup);
         CmdChannel->printf("\t nS_DataHold  %03d (th###)\n", nS_DataHold);
         CmdChannel->printf("\t nS_VICStart  %03d (tv###)\n", nS_VICStart);
         CmdChannel->printf("\t nS_VICDHold  %03d (ti###)\n", nS_VICDHold);
         CmdChannel->printf("\tSet Defaults      (td)\n");
         CmdChannel->printf("\tList current vals (t)\n-----\n");
         break;  
   #endif
   
   }
}

FLASHMEM void AddAndCheckSource(StructMenuItem SourceMenu, uint32_t *TotalSize, uint32_t *FileCount)
{
   *FileCount += 1;
   *TotalSize += SourceMenu.Size;
   Printf_dbg(" Addr: $%08x  Size:%7d  Type:%3d  %s\n", (uint32_t)SourceMenu.Code_Image, SourceMenu.Size, SourceMenu.ItemType, SourceMenu.Name);

   if (((uint32_t)SourceMenu.Code_Image & 0xF0000000) == 0x20000000)
      CmdChannel->printf("*--> %s is using RAM!!!\n", SourceMenu.Name);
}

FLASHMEM void GetDigits(uint8_t NumDigits, uint32_t *SetInt)
{
   char inStr[NumDigits+1];
   
   for(uint8_t DigNum=0; DigNum<NumDigits; DigNum++)
   {
      if(!CmdChannel->available())
      {
         CmdChannel->println("\nNot enough Digits!\n");
         return;
      }
      inStr[DigNum] = CmdChannel->read(); 
   }
   inStr[NumDigits]=0;
   *SetInt = atol(inStr);
   CmdChannel->printf("\nVal Set to: %d\n\n", *SetInt);
}

FLASHMEM void PrintDebugLog()
{
   bool LogDatavalid = false;
   
   #ifdef DbgIOTraceLog
      CmdChannel->println("DbgIOTraceLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgCycAdjLog
      CmdChannel->println("DbgCycAdjLog enabled");
      LogDatavalid = true;
   #endif
      
   #ifdef DbgSpecial
      CmdChannel->println("DbgSpecial enabled");
      LogDatavalid = true;
   #endif
      
   if (CurrentIOHandler == IOH_Debug)
   {
      CmdChannel->println("Debug IO Handler enabled");
      LogDatavalid = true;
   }               
   
   if (!LogDatavalid)
   {
      CmdChannel->println("No logging enabled");
      return;
   }
   
   bool BufferFull = (BigBufCount == BigBufSize);
   
   if  (BufferFull) BigBufCount--; //last element invalid
   
   for(uint16_t Cnt=0; Cnt<BigBufCount; Cnt++)
   {
      CmdChannel->printf("#%04d ", Cnt);
      
      if (BigBuf[Cnt] & DbgSpecialData)
      //if(1)
      {
         CmdChannel->printf("DbgSpecialData: uS betw packets %lu\n", BigBuf[Cnt]);
         //BigBuf[Cnt] &= ~DbgSpecialData;
         //CmdChannel->printf("DbgSpecialData %04x : %02x\n", BigBuf[Cnt] & 0xFFFF, (BigBuf[Cnt] >> 24));
            //code used previously, in-situ:
               //#ifdef DbgSpecial
               //   if (BigBuf != NULL){
               //     BigBuf[BigBufCount] = Address | (HIROM_Image[Address & 0x1FFF]<<24) | DbgSpecialData;
               //     if (BigBufCount < BigBufSize) BigBufCount++;
               //   }
               //#endif
      }
      else if (BigBuf[Cnt] & AdjustedCycleTiming)
      {
         BigBuf[Cnt] &= ~AdjustedCycleTiming;
         CmdChannel->printf("skip %lu ticks = %lu nS, adj = %lu nS\n", BigBuf[Cnt], CycTonS(BigBuf[Cnt]), CycTonS(BigBuf[Cnt])-nS_MaxAdj);
      }
      else
      {
         CmdChannel->printf("%s 0xde%02x : ", (BigBuf[Cnt] & IOTLRead) ? "Read" : "\t\t\t\tWrite", BigBuf[Cnt] & 0xff);

         if (BigBuf[Cnt] & IOTLDataValid) CmdChannel->printf("%02x\n", (BigBuf[Cnt]>>8) & 0xff); //data is valid
         else CmdChannel->printf("n/a\n");
      }
   }
   
   if (BigBufCount == 0) CmdChannel->println("Buffer empty");
   if (BufferFull) CmdChannel->println("Buffer was full");
   CmdChannel->printf("Buffer Reset (Size: %d)\n", BigBufSize);
   BigBufCount = 0;
}

FLASHMEM void LaunchFile()
{            
   //   App: LaunchFileToken 0x6444
   //Teensy: AckToken 0x64CC or RetryToken/abort from minimal
   //   App: Send DriveType(1), DestPath/Name(up to MaxNamePathLength, null term)
   //        DriveTypes: (RegMenuTypes)
   //           USBDrive  = 0
   //           SD        = 1
   //           Teensy    = 2
   //Teensy: AckToken 0x64CC
   //   C64: file Launches

   //Launch file token has been received
   SendU16(AckToken);
   RegMenuTypes DriveType;
   char FileNamePath[MaxNamePathLength];
   
   if (ReceiveFileName(&DriveType, FileNamePath))
   {
      SendU16(AckToken);
      RemoteLaunch(DriveType, FileNamePath, false); //only SD and USB supported by UI
   }
}

FLASHMEM bool ReceiveFileName(RegMenuTypes* DriveType, char *FileNamePath)
{
   uint32_t RecDrive;
   if (!GetUInt(&RecDrive, 1)) return false;
   
   if (RecDrive >= rmtNumTypes) return false;
   *DriveType = (RegMenuTypes)RecDrive;

   uint16_t CharNum=0;
   while (1) 
   {
      if(!SerialAvailabeTimeout()) return false;
      FileNamePath[CharNum] = CmdChannel->read();
      if (FileNamePath[CharNum]==0) return true;
      if (++CharNum == MaxNamePathLength)
      {
         SendU16(FailToken);
         CmdChannel->print("Path too long!\n");  
         return false;
      }
   }
}

FLASHMEM bool GetUInt(uint32_t *InVal, uint8_t NumBytes)
{  //Get NumBytes (4 max) raw bytes to construct InVal
   *InVal=0;
   for(int8_t ByteNum=NumBytes-1; ByteNum>=0; ByteNum--)
   {
      if(!SerialAvailabeTimeout()) return false;
      uint32_t ByteIn = CmdChannel->read();
      *InVal += (ByteIn << (ByteNum*8));
   }
   return true;
}

FLASHMEM void SendU16(uint16_t SendVal)
{
   CmdChannel->write((uint8_t)(SendVal & 0xff));
   CmdChannel->write((uint8_t)((SendVal >> 8) & 0xff));
}

FLASHMEM void SendU32(uint32_t SendVal)
{
    CmdChannel->write((uint8_t)(SendVal & 0xff));
    CmdChannel->write((uint8_t)((SendVal >> 8) & 0xff));
    CmdChannel->write((uint8_t)((SendVal >> 16) & 0xff));
    CmdChannel->write((uint8_t)((SendVal >> 24) & 0xff));
}
   
FLASHMEM bool SerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!CmdChannel->available() && (millis() - StartTOMillis) < SerialTimoutMillis); // timeout loop
   if (CmdChannel->available()) return(true);
   
   SendU16(FailToken);
   CmdChannel->print("Timeout!\n");  
   return(false);
}


uint32_t RAM2BytesFree() 
{
   extern char _heap_end[];
   extern char *__brkval;
   return (_heap_end - __brkval);
}

//memory info display via:
// https://forum.pjrc.com/threads/33443-How-to-display-free-ram
// https://www.pjrc.com/store/teensy41.html#memory

#if ARDUINO_TEENSY41
  extern "C" uint8_t external_psram_size;
#endif
  
FLASHMEM void memInfo () 
{
  constexpr auto RAM_BASE   = 0x2020'0000;
  constexpr auto RAM_SIZE   = 512 << 10;
  constexpr auto FLASH_BASE = 0x6000'0000;
  
#if ARDUINO_TEENSY40
  constexpr auto FLASH_SIZE = 2 << 20;
#elif ARDUINO_TEENSY41
  //constexpr auto FLASH_SIZE = 8 << 20;
#endif

  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  extern char _stext[], _etext[], _sbss[], _ebss[], _sdata[], _edata[],
         _estack[], _heap_start[], _heap_end[], _itcm_block_count[], *__brkval;

  auto sp = (char*) __builtin_frame_address(0);

  CmdChannel->printf("MemInfo:\n");
  CmdChannel->printf("_stext        %08x\n",      _stext);
  CmdChannel->printf("_etext        %08x +%db\n", _etext, _etext - _stext);
  CmdChannel->printf("_sdata        %08x\n",      _sdata);
  CmdChannel->printf("_edata        %08x +%db\n", _edata, _edata - _sdata);
  CmdChannel->printf("_sbss         %08x\n",      _sbss);
  CmdChannel->printf("_ebss         %08x +%db\n", _ebss, _ebss - _sbss);
  CmdChannel->printf("curr stack    %08x +%db\n", sp, sp - _ebss);
  CmdChannel->printf("_estack       %08x +%db\n", _estack, _estack - sp);
  CmdChannel->printf("_heap_start   %08x\n",      _heap_start);
  CmdChannel->printf("__brkval      %08x +%db\n", __brkval, __brkval - _heap_start);
  CmdChannel->printf("_heap_end     %08x +%db\n", _heap_end, _heap_end - __brkval);
  
#if ARDUINO_TEENSY41
  extern char _extram_start[], _extram_end[], *__brkval;
  CmdChannel->printf("_extram_start %08x\n",      _extram_start);
  CmdChannel->printf("_extram_end   %08x +%db\n", _extram_end,
         _extram_end - _extram_start);
#endif
  CmdChannel->printf("\n");

  CmdChannel->printf("<ITCM>  %08x .. %08x\n",
         _stext, _stext + ((int) _itcm_block_count << 15) - 1);
  CmdChannel->printf("<DTCM>  %08x .. %08x\n",
         _sdata, _estack - 1);
  CmdChannel->printf("<RAM>   %08x .. %08x\n",
         RAM_BASE, RAM_BASE + RAM_SIZE - 1);
  CmdChannel->printf("<FLASH> %08x .. %08x\n",
         FLASH_BASE, FLASH_BASE + FLASH_SIZE - 1);
#if ARDUINO_TEENSY41
  if (external_psram_size > 0)
    CmdChannel->printf("<PSRAM> %08x .. %08x\n",
           _extram_start, _extram_start + (external_psram_size << 20) - 1);
#endif
  CmdChannel->printf("\n");

  auto stack = sp - _ebss;
  CmdChannel->printf("avail STACK (RAM1) %8d b %5d kb\n", stack, stack >> 10);

  auto heap = _heap_end - __brkval;
  CmdChannel->printf("avail HEAP  (RAM2) %8d b %5d kb\n", heap, heap >> 10);

#if ARDUINO_TEENSY41
  auto psram = _extram_start + (external_psram_size << 20) - _extram_end;
  CmdChannel->printf("avail PSRAM (ext)  %8d b %5d kb\n", psram, psram >> 10);
#endif
}

FLASHMEM void  getFreeITCM() 
{ // end of CODE ITCM, skip full 32 bits
   uint32_t *ptrFreeITCM;  // Set to Usable ITCM free RAM
   uint32_t  sizeofFreeITCM; // sizeof free RAM in uint32_t units.
   uint32_t  SizeLeft_etext;
   extern char _stext[], _etext[];
   
   CmdChannel->println("\ngetFreeITCM:");
   SizeLeft_etext = (32 * 1024) - (((uint32_t)&_etext - (uint32_t)&_stext) % (32 * 1024));
   sizeofFreeITCM = SizeLeft_etext - 4;
   sizeofFreeITCM /= sizeof(ptrFreeITCM[0]);
   ptrFreeITCM = (uint32_t *) ( (uint32_t)&_stext + (uint32_t)&_etext + 4 );
   CmdChannel->printf( "Size of Free ITCM in Bytes = %u\n", sizeofFreeITCM * sizeof(ptrFreeITCM[0]) );
   CmdChannel->printf( "Start of Free ITCM = %u [%X] \n", ptrFreeITCM, ptrFreeITCM);
   CmdChannel->printf( "End of Free ITCM = %u [%X] \n", ptrFreeITCM + sizeofFreeITCM, ptrFreeITCM + sizeofFreeITCM);
   //for ( uint32_t ii = 0; ii < sizeofFreeITCM; ii++) ptrFreeITCM[ii] = 1;
   //uint32_t jj = 0;
   //for ( uint32_t ii = 0; ii < sizeofFreeITCM; ii++) jj += ptrFreeITCM[ii];
   //CmdChannel->printf( "ITCM DWORD cnt = %u [#bytes=%u] \n", jj, jj*4);
}


