

#ifdef Fab04_FullDMACapable

bool DMA_RnW, DMA_FixC64Addr;
uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
uint8_t *DMA_Buffer;

//These assume Fab04_DataBufAlwaysEnabled
__attribute__((always_inline)) inline uint8_t DataPortWaitReadDMA()
{  // for "normal" (non-VIC) C64 write cycles
   //takes a little longer than the nS_DataSetup=220 non-DMA case, too soon = bad reads
   WaitUntil_nS_fine(nS_DMADataSetup);
   uint32_t DataIn = ReadGPIO7;
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}

__attribute__((always_inline)) inline void DataPortWriteWaitDMA(uint8_t Data)
{  // for "normal" (non-VIC) C64 read cycles only
   SetDataBufOut; //buffer out first
   SetDataPortDirOut; //then set data ports to outputs

   uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN10_PORTSET = RegBits;
   CORE_PIN10_PORTCLEAR = ~RegBits & GP7_DataMask;

   WaitUntil_nS_fine(nS_DMADataHold);  // nS_DataHold = 390 (err), 470 OK, 430 OK(?)
   //not checking Phi2 state due to tight timing and early in cycle call can cause early exit

   SetDataPortDirIn; //set data ports back to inputs/default
   SetDataBufIn;     //then set buffer dir to input
}

// Asked by the callers that have something better to do than attempt a transfer -- the
// blank-then-reboot paths skip a blank nobody could see rather than pay for the wait.
// It is not what keeps a dead bus from hanging the board; WaitForDMAState below is, and
// it covers every caller including the ones that never ask.  An earlier version of this
// comment claimed the two callers here were the only ones reachable with no C64 clocking.
// They are not: WriteC64MemCommand and ReadC64MemCommand answer a remote command on any
// channel, ahead of the busy check, and never ask.
//
// isrPHI2 stamps LastCycCnt from ARM_DWT_CYCCNT at its top, before any branch, so a
// change in it is direct evidence that the handshake can complete.  5 mS is ~5000 edges
// at the ~1 MHz PHI2 this board is built for, which is why a live C64 cannot read as
// dead; a dead one costs 5 mS and the caller skips a blank nobody could see anyway.
FLASHMEM bool C64IsClockingPHI2()
{
   const uint32_t Seen = LastCycCnt;
   const uint32_t Began = millis();
   while (millis() - Began < 5) if (LastCycCnt != Seen) return true;
   return false;
}

// Nothing is clocking the bus, so the handshake that normally releases DMA cannot run --
// and with isrPHI2 not firing there is no ISR mid-update of this state to race.  Drop the
// request line and put the data port back the way a completed CloseDMA would have left
// it, rather than return with DMA still asserted over a machine that may come back.
static FLASHMEM void AbortDMA()
{
   SetDMADeassert;
   SetDataPortDirIn;
   SetDataBufIn;
   DMA_State = DMA_S_DisableReady;
}

// Bounded by bus activity rather than by elapsed time.  How long a transfer legitimately
// takes is set by its length -- a 64 KiB read is ~65 mS at the ~1 MHz PHI2 this board is
// built for -- so any fixed deadline either cuts a long transfer short or leaves a stalled
// one spinning for most of a second.  A stopped bus looks the same at every length:
// LastCycCnt advances on every PHI2 edge whether or not the handshake progresses, so
// waiting on *activity* bounds all four waits with one predicate and no per-caller tuning.
//
// Only a dead bus ends the wait here.  A live bus that never reaches the state -- the
// continuous-read case -- is the ISR's to break, and DMA_TIMEOUT_CYCLES already does.
static FLASHMEM bool WaitForDMAState(uint8_t Target)
{
   uint32_t Seen = LastCycCnt;
   uint32_t Began = millis();

   while (DMA_State != Target)
   {
      if (LastCycCnt != Seen) { Seen = LastCycCnt; Began = millis(); } //bus is alive, keep waiting
      else if (millis() - Began >= 5) { AbortDMA(); return false; }
   }
   return true;
}

// false means the transfer did not happen and *Buffer is not what the C64 holds.  Callers
// that only drive the bus may ignore it; a caller that reports a value to someone else has
// to check, or it reports the stale contents of its own buffer as C64 memory.
FLASHMEM bool PerformDMA(DMA_Trans_RnW RnW, uint16_t StartAddr, uint8_t *Buffer, uint32_t Length, DMA_Addr_Mode FixC64Addr)
{
   //Uses DMA to Read or Write C64 memory to/from *DMABuffer
   DMA_RnW = RnW; //true=read, false=write
   DMA_Count = 0;
   DMA_StartAddr = StartAddr;
   DMA_Buffer = Buffer;
   DMA_Length = Length;
   DMA_FixC64Addr = FixC64Addr;

   DMA_State = DMA_S_StartAsynch;
   if (!WaitForDMAState(DMA_S_TransferReady)) return false;
   DMA_State = DMA_S_TransferExecuting;
   if (!WaitForDMAState(DMA_S_TransferComplete)) return false;

   delayMicroseconds(2); //wait a couple cycles in case of restart, moved to transfer start

   Printf_dbg("DMA %s addr $%04x:$%04x (len: $%04x) StCyc: %lu\n", (RnW ? "Read":"Write"), StartAddr, StartAddr+Length-1, Length, DMACycleCount);
   return true;
}

FLASHMEM bool CloseDMA()
{
   DMA_State = DMA_S_StartDisable;
   return WaitForDMAState(DMA_S_DisableReady);
}

//__attribute__((always_inline)) inline bool DMAByte()
bool DMAByte(uint8_t *Data)
{  //verifies Bus Available and sends/receives a byte to/from DMA_Buffer[DMA_Count]
   //assumes we're in the VIC cycle somewhere and StartCycCnt has been updated
   uint32_t RegAddrBits;

   if (DMA_FixC64Addr) RegAddrBits = (DMA_StartAddr << 16);
   else RegAddrBits = ((DMA_StartAddr+DMA_Count) << 16);
   
   WaitUntil_nS_fine(nS_DMABAWait);
   
   if (!GP9_BA(ReadGPIO9)) return false;  // bus not available, skip until it is
   
   CORE_PIN19_PORTSET = RegAddrBits; //set address port value to be ready for output drive
   CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;
       
   //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   WaitUntil_nS(nS_DMASetup);  //use timer instead to get just ahead of the transition.   
   //phi2 is about to go high..........................................................................      
   StartCycCnt = ARM_DWT_CYCCNT;
         
   if (DMA_RnW)
   {  //Read Cycle: 
      //leave R/*W as input (Pulled Up, Read)
      SetAddrBufsOut;   //set address buffers to output
      SetAddrPortDirOut;//set address ports to output   
      *Data = DataPortWaitReadDMA(); //wait for data, read it.  Different timing from DataPortWaitRead()
   }
   else
   {  //Write Cycle:
      SetRWOutWrite;    // <---- set this first/quickly!
      SetAddrBufsOut;   //set address buffers to output
      SetAddrPortDirOut;//set address ports to output 
      DataPortWriteWaitDMA(*Data);
      SetRWInput; //set R/*W back to input
   }
   
   SetAddrPortDirIn;//set address ports to input
   SetAddrBufsIn;   //set address buffers to input
   
   return true;
}

void DMATransferISR()
{
   // called from Phi2 isr when (DMA_State == DMA_S_TransferExecuting)
   
   if (!GP9_BA(ReadGPIO9)) return;  // bus not available, skip until it is

   //skip cycles, re-align to edge and in case first after BA
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   
   while (DMA_Count != DMA_Length)
   {
      while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
      //phi2 has gone low..........................................................................     
      StartCycCnt = ARM_DWT_CYCCNT;
      
      if (!DMAByte(&DMA_Buffer[DMA_Count])) return;

      DMA_Count++;
   }
   DMA_State = DMA_S_TransferComplete;
}

#endif
