// SPDX-License-Identifier: MIT
#pragma once

// PAL vs NTSC from the PHI2 period (item #25).  Timed in setup() with the C64 held in reset, so the
//   bus timing is right from the first cycle of whatever runs next, including a cartridge started
//   without the menu, which never reports the machine.  Then re-timed from loop() for as long as the
//   image runs, in a burst right after every reset release and slowly otherwise.  A C64U powers up at
//   the PAL rate and waits for the cartridge port's reset to be released before it applies a saved
//   NTSC setting, ~0.1-0.2S later but before it starts its CPU; it can also switch live from its menu
//   without a restart (Ultimate firmware, software/u64/u64_config.cc).
// C64 vs C128 still comes from the menu's report: a C128 can't be told apart at the port while it is
//   in reset or early in its boot (see DMA-Timing-Known-Issues.md #1).

#define Phi2BootPeriods    1024     //PHI2 periods timed in setup(), ~1mS
#define Phi2CheckPeriods   32       //...and per re-check from loop(), ~35uS
#define Phi2CheckBurst_mS  10       //after a reset release, re-check this often...
#define Phi2BurstFor_mS    2000     //   ...for this long, to follow a C64U's switch before it starts its CPU
#define Phi2CheckSlow_mS   2000     //otherwise this often, for a live standard change (a C64U switched from its menu)
#define Phi2BootTries      4        //a boot reading can be thrown away by a long interrupt; retry this many times
#define Phi2MaxGap         8        //periods between two stamps that can still be rounded; past this the ISR was held
#define Phi2Timeout_nS     50000    //no new stamp in 50uS: the bus isn't clocking
#define Phi2Split_pS       996000   //midway between NTSC 977.78nS and PAL 1014.97nS per cycle
#define Phi2Min_pS         940000   //outside Min/Max: not a 1MHz C64/C128 bus
#define Phi2Max_pS         1060000

uint8_t MeasuredVidStd = 0xff; //the latest PHI2 timing: rvtcNTSC, 0 for PAL, 0xff not measured
uint32_t Phi2BurstUntilmS = 0;  //millis() a post-release burst of re-checks runs until

//rvtcNTSC or 0 for PAL, from the stamp isrPHI2 takes on every rising edge (LastCycCnt), or 0xff
//   when there is no good reading.  Nothing is masked, so the ISR keeps serving the bus throughout.
//   Thread mode only runs between isrPHI2 calls and can miss a stamp, so each gap is rounded to whole
//   periods: ~800 CPU cycles either standard and only 4% apart, so a gap of up to Phi2MaxGap periods
//   rounds unambiguously.  A longer gap means the ISR was held up, by a DMA transfer run inside it
//   (DMATransferISR, the REU's DirectREU) or anything else long, and rounding it would pull the
//   reading towards the split, so the reading is thrown away instead.  So is one with a gap more
//   than a quarter period off a whole number: a stamp taken on a late ISR entry rather than on an
//   edge.  A real gap of up to Phi2MaxGap periods is at most ~16% off (the two standards' periods
//   either side of the split), so that caps what a late stamp at either end of a reading can shift
//   it by at a quarter period over the run: ~7.6nS at Phi2CheckPeriods, against 18nS to the split.
//   Needs isrPHI2 attached.
FLASHMEM uint8_t MeasureVideoStd(uint32_t Periods)
{
   const uint32_t Nominal = nSToCyc(Phi2Split_pS/1000);
   const uint32_t TimeoutCyc = nSToCyc(Phi2Timeout_nS);

   uint32_t Prev = LastCycCnt, Mark = ARM_DWT_CYCCNT, Now;
   while ((Now = LastCycCnt) == Prev) if (ARM_DWT_CYCCNT - Mark > TimeoutCyc) return 0xff; //start on a fresh stamp
   const uint32_t First = Prev = Now;
   uint32_t Count = 0;
   while (Count < Periods)
   {
      Mark = ARM_DWT_CYCCNT;
      while ((Now = LastCycCnt) == Prev) if (ARM_DWT_CYCCNT - Mark > TimeoutCyc) return 0xff;
      const uint32_t Gap = Now - Prev;
      const uint32_t Whole = (Gap + Nominal/2) / Nominal;
      const int32_t Off = (int32_t)(Gap - Whole * Nominal);
      if (Whole == 0 || Whole > Phi2MaxGap || Off > (int32_t)(Nominal/4) || Off < -(int32_t)(Nominal/4)) return 0xff;
      Count += Whole;
      Prev = Now;
   }

   const uint32_t pS = (uint64_t)(Prev - First) * 1000000000000ULL / ((uint64_t)F_CPU_ACTUAL * Count);
   if (pS < Phi2Min_pS || pS > Phi2Max_pS) return 0xff;
   return pS < Phi2Split_pS ? rvtcNTSC : 0;
}

//setup(), with the C64 held in reset: sets MeasuredVidStd, retrying a reading a long interrupt threw away
FLASHMEM uint8_t TimeVideoStdAtBoot()
{
   for (uint8_t Try = 0; Try < Phi2BootTries && MeasuredVidStd == 0xff; Try++)
      MeasuredVidStd = MeasureVideoStd(Phi2BootPeriods);
   return MeasuredVidStd;
}

//loop(), right after each reset release: start a burst of re-checks
FLASHMEM void Phi2ResetReleased()
{
   Phi2BurstUntilmS = millis() + Phi2BurstFor_mS;
}

//loop(): re-times PHI2 when due (or now, when Force), and returns the new standard when it has
//   changed from MeasuredVidStd, 0xff otherwise.  Updates MeasuredVidStd; the caller applies it.
//   A reading with no good result changes nothing.
FLASHMEM uint8_t RecheckVideoStd(bool Force)
{
   static uint32_t LastmS = 0;
   const uint32_t Now = millis();
   const bool Burst = (int32_t)(Phi2BurstUntilmS - Now) > 0;
   if (!Force && Now - LastmS < (Burst ? Phi2CheckBurst_mS : Phi2CheckSlow_mS)) return 0xff;
   LastmS = Now;
   const uint8_t Std = MeasureVideoStd(Phi2CheckPeriods);
   if (Std == 0xff || Std == MeasuredVidStd) return 0xff;
   Serial.printf("PHI2 now %s, was %s\n", Std ? "NTSC" : "PAL", MeasuredVidStd == 0xff ? "unknown" : MeasuredVidStd ? "NTSC" : "PAL");
   MeasuredVidStd = Std;
   return Std;
}

//loop() in the minimal and extension images: follow a PHI2 change, keeping C64/C128 as it was applied
FLASHMEM void FollowVideoStd()
{
   const uint8_t NewStd = RecheckVideoStd(false);
   if (NewStd != 0xff) SetVideoStdDMATiming((uint8_t)((TimingVidTODClks & ~rvtcNTSC) | NewStd));
}
