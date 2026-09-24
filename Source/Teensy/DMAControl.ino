

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
// it covers every caller including the ones that never ask.
//
// What neither of them guards is a C64 that is switched off.  The cartridge port is this
// board's only supply -- PCB/PCB_Assembly.md has the Teensy's USB 5 V trace cut during
// assembly so the two cannot back-feed -- so a C64 that is off takes the Teensy with it.
// Measured with the C64 switched off: /dev/cu.usbmodem* disappears and no command can be
// sent, let alone answered.  The state these guards are for is a C64 still supplying 5 V
// that has stopped clocking: a fault, or the moment either side of the power switch.
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

// Give up on a transfer and put the bus back the way a completed CloseDMA would have left
// it, rather than return with DMA still asserted over a machine that may come back.
//
// The state goes first.  isrPHI2 dispatches on it, so clearing it is what stops the ISR
// taking another transfer cycle; the pause after gives a DMATransferISR already inside a
// byte time to finish one.  Only then are the ports torn down -- DMAByte drives the
// address ports as well as the data ports, and an abort landing mid-byte can find them
// still output.  On a dead bus none of that is running and the pause costs 4 uS.
static FLASHMEM void AbortDMA()
{
   DMA_State = DMA_S_DisableReady;
   delayMicroseconds(4);

   SetDMADeassert;
   SetAddrPortDirIn;
   SetAddrBufsIn;
   SetDataPortDirIn;
   SetDataBufIn;
}

// Two bounds, because two different things can stop a wait from ever ending.
//
// A dead bus: LastCycCnt advances on every PHI2 edge whether or not the handshake
// progresses, so a stamp that stops moving is direct evidence nothing is clocking.  That
// looks the same at every transfer length, which is why it is checked on activity rather
// than against a deadline.
//
// A live bus that never reaches the state: here a deadline is the only instrument, and
// CeilingmS is the caller's, because the two waits are different shapes.  The ISR breaks
// one case of this itself -- DMA_TIMEOUT_CYCLES gives up on a continuous *read* loop and
// asserts anyway.  It does not break the continuous-write case; ISRs.c leaves that branch
// unbounded on the reasoning that continuous writes are not a real situation, which holds
// for a 6510 and not for a bus held by something else, and asserting DMA out of a write
// cycle is exactly what the safe-freeze sequence exists to avoid.  So the ISR cannot end
// that one safely and the caller has to.
static FLASHMEM bool WaitForDMAState(uint8_t Target, uint32_t CeilingmS)
{
   uint32_t Seen = LastCycCnt;
   uint32_t Quiet = millis();
   const uint32_t Began = Quiet;

   while (DMA_State != Target)
   {
      if (millis() - Began >= CeilingmS) { AbortDMA(); return false; }

      if (LastCycCnt != Seen) { Seen = LastCycCnt; Quiet = millis(); } //bus is alive, keep waiting
      else if (millis() - Quiet >= 5) { AbortDMA(); return false; }
   }
   return true;
}

// The handshake waits do not scale with anything: the safe-freeze sequence needs one
// read cycle, and the ISR's own 5000-cycle read-loop timeout is ~5 mS at the ~1 MHz PHI2
// this board is built for.  50 mS is ten times that, so it cannot fire on a bus that is
// merely busy, and it still bounds the write case the ISR leaves open.
#define DMA_HANDSHAKE_CEILING_mS  50

// The transfer does scale: one PHI2 cycle per byte at best, ~1 uS each, and DMAByte skips
// every cycle the VIC has taken the bus for.  Four times the ideal covers stolen cycles
// with room to spare -- 312 mS for a full 64 KiB read -- and the floor keeps a short
// transfer from being held to a deadline shorter than the handshake's.
static FLASHMEM uint32_t DMATransferCeilingmS(uint32_t Length)
{
   return DMA_HANDSHAKE_CEILING_mS + (Length * 4) / 1000;
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
   if (!WaitForDMAState(DMA_S_TransferReady, DMA_HANDSHAKE_CEILING_mS)) return false;
   DMA_State = DMA_S_TransferExecuting;
   if (!WaitForDMAState(DMA_S_TransferComplete, DMATransferCeilingmS(Length))) return false;

   delayMicroseconds(2); //wait a couple cycles in case of restart, moved to transfer start

   Printf_dbg("DMA %s addr $%04x:$%04x (len: $%04x) StCyc: %lu\n", (RnW ? "Read":"Write"), StartAddr, StartAddr+Length-1, Length, DMACycleCount);
   return true;
}

FLASHMEM bool CloseDMA()
{
   DMA_State = DMA_S_StartDisable;
   return WaitForDMAState(DMA_S_DisableReady, DMA_HANDSHAKE_CEILING_mS);
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
