
uint32_t *BusBitCount;
volatile uint32_t BusSampleCount;

#define BusSampleMaxSize   100000
#define BusSampleTimeoutmS   2000
#define NumBusBits             25

FLASHMEM void BusAnalysis()
{
   if (fBusSnoop!=NULL || PendingfBusSnoop!=NULL)
   {
      Serial.printf("\nBus Snoop function is busy, aborting\n");
      return;
   }
   Serial.printf("\nTaking max %d samples in %dmS\n", BusSampleMaxSize, BusSampleTimeoutmS);
#if !defined(DbgFab0_3plus) && !defined(Fab04_DataBufAlwaysEnabled)
   Serial.printf("Snooping data bus on C64 writes only (Fab 0.2x)\n");
#endif
   
   BusBitCount = (uint32_t*)calloc(NumBusBits*2, sizeof(uint32_t));
   BusSampleCount = 0;
   
   uint32_t StartTime = millis();
   fBusSnoop = &BusCount; 
   delay(1); // BusCount is never called if this isn't present
   while (BusSampleCount<BusSampleMaxSize && millis()-StartTime<BusSampleTimeoutmS); //wait to fill buffer or timeout
   if (fBusSnoop == &BusCount) fBusSnoop = NULL;  //in case something else took over the snoop function

   Serial.printf("Took %d samples in %dmS\n", BusSampleCount, millis()-StartTime);
   Serial.printf(" bit     Highs     Lows     Total\n");

   for(uint8_t BitNum=0; BitNum<NumBusBits; BitNum++)
   {
      if(BitNum<16) Serial.printf(" A%02d", BitNum);
      else if(BitNum<24) Serial.printf("  D%d", BitNum-16);
      else Serial.printf("R/Wn");
      
      Serial.printf(" %9d%9d%9d\n", BusBitCount[BitNum*2+1], BusBitCount[BitNum*2], BusBitCount[BitNum*2]+BusBitCount[BitNum*2+1]);
   }
   
   free(BusBitCount); BusBitCount=NULL;
}
         
bool BusCount(uint16_t Address, bool R_Wn)
{

#if defined(DbgFab0_3plus) || defined(Fab04_DataBufAlwaysEnabled)
   //can snoop C64 Read or Write Data with fab 0.3+
   SetDataBufIn; //force data buffer to input, regardless of R/W
   {
#else
   //can only snoop C64 Write data with fab 0.2x (to avoid bus contention)
   if(!R_Wn)     
   {
#endif
      uint8_t Data = DataPortWaitRead();
      //store Data 7:0 in BusBitCount 23:16
      for(uint8_t BitNum=16; BitNum<24; BitNum++) BusBitCount[BitNum*2+((Data & (1<<(BitNum-16))) ? 1:0)]++;       
   }

   // store R/nW in BusBitCount 24
   BusBitCount[24*2+(R_Wn ? 1:0)]++;
   
   //store Address 15:0 in BusBitCount 15:0
   for(uint8_t BitNum=0; BitNum<16; BitNum++) BusBitCount[BitNum*2+((Address & (1<<BitNum)) ? 1:0)]++; 
      
   BusSampleCount++;
   return true;
}
#ifdef Dbg_SerBusLat
// The Cortex-M7's DWT_EXCCNT counts cycles spent entering and leaving exceptions, so a sample
// it moved during was interrupted. imxrt.h names neither it nor its enable bit.
#define DWT_EXCCNT       (*(volatile uint32_t *)0xE000100C)
#define DWT_EXCEVTENA    (1 << 18)

#define nS_BusLatStart    600  // isrPHI2 has returned by now, and an idle sample ends before the next rise
#define nS_BusLatNoPhi2  1500
#define nS_BusLatGiveUp 100000
#define BusLatSamples    4000

enum : uint8_t { BL_Baseline, BL_GPIO6, BL_USBSTS, BL_ENDPTPRIME, BL_USBSTSThenGPIO6,
                             BL_WriteThenGPIO6, BL_NumProbes };

// FASTRUN so no flash fetch lands inside the timed accesses. Unmasked: started just after a
// Phi2 stamp, a sample ends before the next rise, and one an exception landed in is dropped.
static FASTRUN __attribute__((noinline)) bool BusLatSample(uint8_t Probe, uint32_t &Cycles)
{
   const uint32_t Began = ARM_DWT_CYCCNT;
   while (true)
   {
      uint32_t Since = ARM_DWT_CYCCNT - LastCycCnt;
      if (Since < nSToCyc(nS_BusLatStart) || Since > nSToCyc(nS_BusLatNoPhi2)) break;
      if (ARM_DWT_CYCCNT - Began > nSToCyc(nS_BusLatGiveUp)) return false;
   }
   const uint8_t Exceptions = DWT_EXCCNT;
   uint32_t Start = ARM_DWT_CYCCNT, Value;
   switch (Probe)
   {
      case BL_Baseline:        break;
      case BL_GPIO6:           Value = ReadGPIO6; (void)Value; break;
      case BL_USBSTS:          Value = USB1_USBSTS; (void)Value; break;
      case BL_ENDPTPRIME:      Value = USB1_ENDPTPRIME; (void)Value; break;
      case BL_USBSTSThenGPIO6: Value = USB1_USBSTS; Value = ReadGPIO6; (void)Value; break;
      case BL_WriteThenGPIO6:  USB1_ENDPTSTATUS = 0; Value = ReadGPIO6; (void)Value; break; // read-only: the write is ignored
      default:                 return false;
   }
   asm volatile("dsb" ::: "memory");  // Device-before-Strongly-ordered orders the accesses, not the load's data arriving
   Cycles = ARM_DWT_CYCCNT - Start;
   return (uint8_t)DWT_EXCCNT == Exceptions;
}

FLASHMEM void BusLatency()
{
   static const char Names[BL_NumProbes][32] PROGMEM = {
      "nothing (counter reads and DSB)", "GPIO6 read", "USB1 USBSTS read", "USB1 ENDPTPRIME read",
      "GPIO6 read after a USBSTS read", "GPIO6 read after a USB1 write"};
   const uint32_t DwtCtrl = ARM_DWT_CTRL;
   ARM_DWT_CTRL = DwtCtrl | DWT_EXCEVTENA;
   CmdChannel->printf(PSTR("Bus latency, %d tries each, in CPU cycles at %lu MHz:\n"), BusLatSamples, F_CPU_ACTUAL / 1000000);
   for (uint8_t Probe = 0; Probe < BL_NumProbes; Probe++)
   {
      uint32_t Min = UINT32_MAX, Max = 0, Kept = 0;
      for (uint32_t Try = 0; Try < BusLatSamples; Try++)
      {
         uint32_t Cycles;
         if (!BusLatSample(Probe, Cycles)) continue;
         Kept++;
         if (Cycles < Min) Min = Cycles;
         if (Cycles > Max) Max = Cycles;
      }
      if (Kept) CmdChannel->printf(PSTR("%-34s %5lu kept  min %4lu  max %4lu  (%lu..%lu nS)\n"), Names[Probe], Kept,
                                   Min, Max, CycTonS(Min), CycTonS(Max));
      else CmdChannel->printf(PSTR("%-34s %5lu kept\n"), Names[Probe], Kept);
   }
   ARM_DWT_CTRL = DwtCtrl;
}
#endif
