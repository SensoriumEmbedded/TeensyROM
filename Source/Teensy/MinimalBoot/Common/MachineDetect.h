// SPDX-License-Identifier: MIT
#pragma once

// PAL vs NTSC from the PHI2 period (item #25).  Taken in setup() with the C64 held in reset, so
//   the bus timing is right from the first cycle of whatever runs next, including a cartridge
//   started without the menu, which never reports the machine.  C64 vs C128 still comes from the
//   menu's report: a C128 can't be told apart at the port while it is in reset or early in its
//   boot (see DMA-Timing-Known-Issues.md #1).

#define Phi2Periods        1024     //PHI2 periods timed, ~1mS
#define Phi2Timeout_nS     50000    //no new stamp in 50uS: the bus isn't clocking
#define Phi2Split_pS       996000   //midway between NTSC 977.78nS and PAL 1014.97nS per cycle
#define Phi2Min_pS         940000   //outside Min/Max: not a 1MHz C64/C128 bus
#define Phi2Max_pS         1060000

uint8_t MeasuredVidStd = 0xff; //MeasureVideoStd() at boot: rvtcNTSC, 0 for PAL, 0xff not measured

//rvtcNTSC or 0 for PAL, from the stamp isrPHI2 takes on every rising edge (LastCycCnt), or 0xff
//   when the bus isn't clocking at a C64/C128 rate.  Nothing is masked, so the ISR keeps serving
//   the bus throughout.  Thread mode only runs between isrPHI2 calls and can miss a stamp, so each
//   gap is rounded to whole periods: ~800 CPU cycles either standard and only 4% apart, so a gap
//   of up to a dozen periods rounds unambiguously.  Needs isrPHI2 attached; takes ~1mS.
FLASHMEM uint8_t MeasureVideoStd()
{
   const uint32_t Nominal = nSToCyc(Phi2Split_pS/1000);
   const uint32_t TimeoutCyc = nSToCyc(Phi2Timeout_nS);

   uint32_t Prev = LastCycCnt, Mark = ARM_DWT_CYCCNT, Now;
   while ((Now = LastCycCnt) == Prev) if (ARM_DWT_CYCCNT - Mark > TimeoutCyc) return 0xff; //start on a fresh stamp
   const uint32_t First = Prev = Now;
   uint32_t Periods = 0;
   while (Periods < Phi2Periods)
   {
      Mark = ARM_DWT_CYCCNT;
      while ((Now = LastCycCnt) == Prev) if (ARM_DWT_CYCCNT - Mark > TimeoutCyc) return 0xff;
      const uint32_t Whole = (Now - Prev + Nominal/2) / Nominal;
      if (Whole == 0) return 0xff; //two stamps under half a period apart: not a 1MHz bus
      Periods += Whole;
      Prev = Now;
   }

   const uint32_t pS = (uint64_t)(Prev - First) * 1000000000000ULL / ((uint64_t)F_CPU_ACTUAL * Periods);
   if (pS < Phi2Min_pS || pS > Phi2Max_pS) return 0xff;
   return pS < Phi2Split_pS ? rvtcNTSC : 0;
}
