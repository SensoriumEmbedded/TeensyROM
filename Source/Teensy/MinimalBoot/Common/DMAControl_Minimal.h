#pragma once

#include <util/atomic.h>

// MinimalBoot-local copy of Source/Teensy/DMAControl.ino. Arduino builds a
// sketch from one directory and cannot import a parent sketch's .ino tab, so
// this small hardware routine must live inside the MinimalBoot sketch tree.
// Keep the implementations synchronized when DMA timing is changed upstream.

#ifdef Fab04_FullDMACapable

bool DMA_RnW, DMA_FixC64Addr;
uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
uint8_t *DMA_Buffer;
volatile bool AGIDMA_Failed;

// A healthy C64 PHI2 edge arrives about once per microsecond. These generous
// limits catch a stopped clock or wedged state machine without ever competing
// with an ordinary transfer (8K takes only a few milliseconds).
#define AGIDMA_StateTimeoutCycles (F_CPU_ACTUAL / 4u)
#define AGIDMA_Phi2TimeoutCycles  (F_CPU_ACTUAL / 10000u)

FASTRUN void AGIDMAEmergencyRelease()
{
   // Prevent the PHI2 ISR from observing an executing/start state while the
   // shared bus is only partly released. Preserve the caller's interrupt mask
   // because a PHI2 edge timeout can invoke this from inside that ISR too.
   uint32_t InterruptMask = __get_primask();
   __disable_irq();
   AGIDMA_Failed = true;

   // Stop driving every shared bus signal before releasing /DMA, then publish
   // DisableReady only after the C64 bus is completely idle.
   SetRWInput;
   SetAddrPortDirIn;
   SetAddrBufsIn;
   SetDataPortDirIn;
   SetDataBufIn;

   // Prefer the same PHI2-low release phase as the normal DMA state machine.
   // A stopped-high clock must not hold the machine forever, so the existing
   // edge deadline remains the hard upper bound before forced deassertion.
   uint32_t ReleaseStarted = ARM_DWT_CYCCNT;
   while (GP6_Phi2(ReadGPIO6) &&
          ARM_DWT_CYCCNT - ReleaseStarted <= AGIDMA_Phi2TimeoutCycles) {}
   SetDMADeassert;
   DMA_State = DMA_S_DisableReady;
   __set_primask(InterruptMask);
}

FLASHMEM bool AGIDMAWaitForState(uint8_t Expected)
{
   uint32_t Started = ARM_DWT_CYCCNT;
   while (DMA_State != Expected)
   {
      if (AGIDMA_Failed) return false;
      if (ARM_DWT_CYCCNT - Started > AGIDMA_StateTimeoutCycles)
      {
         AGIDMAEmergencyRelease();
         return false;
      }
   }
   return true;
}

__attribute__((always_inline)) inline bool AGIDMAWaitForPhi2(bool High)
{
   uint32_t Started = ARM_DWT_CYCCNT;
   while ((GP6_Phi2(ReadGPIO6) != 0) != High)
   {
      if (ARM_DWT_CYCCNT - Started > AGIDMA_Phi2TimeoutCycles)
      {
         AGIDMAEmergencyRelease();
         return false;
      }
   }
   return true;
}

// These assume Fab04_DataBufAlwaysEnabled.
__attribute__((always_inline)) inline uint8_t DataPortWaitReadDMA()
{
   WaitUntil_nS(390);
   uint32_t DataIn = ReadGPIO7;
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}

__attribute__((always_inline)) inline void DataPortWriteWaitDMA(uint8_t Data)
{
   SetDataBufOut;
   SetDataPortDirOut;

   uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN10_PORTSET = RegBits;
   CORE_PIN10_PORTCLEAR = ~RegBits & GP7_DataMask;

   WaitUntil_nS(430);
   SetDataPortDirIn;
   SetDataBufIn;
}

FLASHMEM bool PerformDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer,
                         uint32_t Length, bool FixC64Addr)
{
   AGIDMA_Failed = false;
   DMA_RnW = RnW;
   DMA_Count = 0;
   DMA_StartAddr = StartAddr;
   DMA_Buffer = Buffer;
   DMA_Length = Length;
   DMA_FixC64Addr = FixC64Addr;

   DMA_State = DMA_S_StartAsynch;
   if (!AGIDMAWaitForState(DMA_S_TransferReady)) return false;
   DMA_State = DMA_S_TransferExecuting;
   if (!AGIDMAWaitForState(DMA_S_TransferComplete)) return false;

   delayMicroseconds(2);
   Printf_dbg("DMA %s addr $%04x:$%04x (len: $%04x) StCyc: %lu\n",
              (RnW ? "Read" : "Write"), StartAddr, StartAddr + Length - 1,
              Length, DMACycleCount);
   return true;
}

// Continue a scatter/gather operation without releasing /DMA between
// segments. PerformDMA() safely acquires the bus for the first segment;
// AGIContinueDMA() re-arms only the transfer engine while the C64 remains
// paused, and CloseDMA() releases the bus once after the final segment.
FLASHMEM bool AGIContinueDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer,
                             uint32_t Length, bool FixC64Addr)
{
   if (AGIDMA_Failed || DMA_State != DMA_S_TransferComplete || !Length)
   {
      AGIDMAEmergencyRelease();
      return false;
   }

   DMA_RnW = RnW;
   DMA_Count = 0;
   DMA_StartAddr = StartAddr;
   DMA_Buffer = Buffer;
   DMA_Length = Length;
   DMA_FixC64Addr = FixC64Addr;
   DMA_State = DMA_S_TransferExecuting;
   if (!AGIDMAWaitForState(DMA_S_TransferComplete)) return false;

   // Match PerformDMA's established post-transfer settling interval before
   // another discontinuous address is armed.  /DMA remains asserted, so the
   // C64 cannot consume its synchronous mailbox timeout between actor planes.
   delayMicroseconds(2);
   return true;
}

FLASHMEM bool CloseDMA()
{
   if (AGIDMA_Failed)
   {
      AGIDMAEmergencyRelease();
      return false;
   }
   DMA_State = DMA_S_StartDisable;
   return AGIDMAWaitForState(DMA_S_DisableReady);
}

bool DMAByte(uint8_t *Data)
{
   uint32_t RegAddrBits;
   if (DMA_FixC64Addr) RegAddrBits = (DMA_StartAddr << 16);
   else RegAddrBits = ((DMA_StartAddr + DMA_Count) << 16);

   WaitUntil_nS(200);
   if (!GP9_BA(ReadGPIO9)) return false;

   CORE_PIN19_PORTSET = RegAddrBits;
   CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;

   WaitUntil_nS(nS_DMASetup);
   StartCycCnt = ARM_DWT_CYCCNT;

   if (DMA_RnW)
   {
      SetAddrBufsOut;
      SetAddrPortDirOut;
      *Data = DataPortWaitReadDMA();
   }
   else
   {
      SetRWOutWrite;
      SetAddrBufsOut;
      SetAddrPortDirOut;
      DataPortWriteWaitDMA(*Data);
      SetRWInput;
   }

   SetAddrPortDirIn;
   SetAddrBufsIn;
   return true;
}

void DMATransferISR()
{
   if (!GP9_BA(ReadGPIO9)) return;

   if (!AGIDMAWaitForPhi2(false)) return;
   if (!AGIDMAWaitForPhi2(true)) return;
   if (!AGIDMAWaitForPhi2(false)) return;
   if (!AGIDMAWaitForPhi2(true)) return;
   if (!AGIDMAWaitForPhi2(false)) return;
   if (!AGIDMAWaitForPhi2(true)) return;

   while (DMA_Count != DMA_Length)
   {
      if (!AGIDMAWaitForPhi2(false)) return;
      StartCycCnt = ARM_DWT_CYCCNT;
      if (!DMAByte(&DMA_Buffer[DMA_Count])) return;
      DMA_Count++;
   }
   DMA_State = DMA_S_TransferComplete;
}

#endif
