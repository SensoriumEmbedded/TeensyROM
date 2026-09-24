

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
// It is not the main thing that keeps a dead bus from hanging the board; WaitForDMAState
// below is, and it covers every caller including the ones that never ask.  What neither
// covers is a bus that stops while DMATransferISR is inside an edge wait -- the third case
// named over WaitForDMAState -- and there declining to start a transfer is all there is.
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
// The release is the ISR's to make, not ours.  isrPHI2 drops DMA from inside the falling
// edge it has already re-aligned to (ISRs.c:158), because the C64 and C128 publish the same
// rule -- DMA moves only while PHI2 is low (docs/Architecture/DMA-Timing-Known-Issues.md:166)
// -- and on a C128 it also drives the MMU's bus-direction hand-off, where a marginal
// reversal can let a write land at the wrong address.  Dropping it from here would land
// uniformly at random inside the cycle, about half of it mid-access.  So while the bus is
// clocking this asks for the controlled release and waits for it; the unsynchronised drop
// is the fallback for a bus that is not clocking, where there is no phase to get wrong and
// no ISR left to do it.
//
// No pause guards the ports.  Every caller of this is FLASHMEM and runs in thread mode
// (Constraints.md:45), and thread mode cannot preempt a handler on a single core, so a
// DMATransferISR "already inside a byte" is not a state that exists.  DMAByte restores the
// ports on both its exits, so the four below are belt-and-braces for an abort that lands
// between transfers rather than inside one.
static FLASHMEM void AbortDMA()
{
   uint32_t Seen = LastCycCnt;
   uint32_t Began = millis();

   DMA_State = DMA_S_StartDisable;
   while (DMA_State != DMA_S_DisableReady)
   {
      if (LastCycCnt != Seen) { Seen = LastCycCnt; Began = millis(); } //still clocking, let it land
      else if (millis() - Began >= 5) { SetDMADeassert; DMA_State = DMA_S_DisableReady; }
   }

   SetAddrPortDirIn;
   SetAddrBufsIn;
   SetDataPortDirIn;
   SetDataBufIn;
}

// Two bounds here, for two of the three things that can stop a wait from ever ending. The
// third is not this function's to catch and is not caught: DMATransferISR's edge waits
// (DMAControl.ino, "Find phi2 falling") have no exit but the pin changing, so a clock that
// stops while the ISR is inside one spins it forever at priority 16 -- thread mode never
// runs again and neither bound below can fire. That window is most of a long transfer.
// Bounding those waits means putting a deadline in the hottest path in the firmware, which
// wants a bus to test it on rather than a reviewer; it is recorded, not fixed.
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
      // Re-read before giving up. isrPHI2 preempts thread mode, so it can reach Target
      // between the loop's test and this one -- and aborting then would report a transfer
      // that actually landed as one that did not, which for a write means a host that
      // retries writes it twice. One volatile read is the whole of the window.
      if (millis() - Began >= CeilingmS || (LastCycCnt == Seen && millis() - Quiet >= 5))
      {
         if (DMA_State == Target) return true;
         AbortDMA();
         return false;
      }

      if (LastCycCnt != Seen) { Seen = LastCycCnt; Quiet = millis(); } //bus is alive, keep waiting
   }
   return true;
}

// The handshake waits do not scale with anything: the safe-freeze sequence needs one
// read cycle, and the ISR's own 5000-cycle read-loop timeout is ~5 mS at the ~1 MHz PHI2
// this board is built for.  50 mS is ten times that, so it cannot fire on a bus that is
// merely busy, and it still bounds the write case the ISR leaves open.
#define DMA_HANDSHAKE_CEILING_mS  50

// The transfer does scale: one PHI2 cycle per byte at best, ~1 uS each, and DMAByte skips
// every cycle the VIC has taken the bus for.  Four times the ideal plus the flat floor is
// 312 for a full 64 KiB read, against a modelled worst case of ~268 with every display line
// a badline and eight sprites on it (NTSC; PAL is ~182).  So the margin at the maximum
// length is the flat term, not the multiplier -- about 14% -- and it does not get thinner,
// because 65535 is all a 16-bit address space can ask for.
//
// Those are mS of millis(), which is not wall clock here: SysTick sits at priority 32 and
// isrPHI2 at 16, and a blocked tick is lost rather than accumulated, so a long transfer
// under-counts. The error is in the safe direction -- the deadline arrives later than it
// reads -- but it means this number is a floor on the elapsed time, not a bound on it, and
// a host timeout sized from it should be larger.
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
