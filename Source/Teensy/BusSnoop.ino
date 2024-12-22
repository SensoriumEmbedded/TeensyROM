
uint32_t *BusBitCount;
volatile uint32_t BusSampleCount;

#define BusSampleMaxSize   100000
#define BusSampleTimeoutmS   2000
#define NumBusBits             25

FLASHMEM void BusAnalysis()
{
   Serial.printf("\nTaking max %d samples in %dmS\n", BusSampleMaxSize, BusSampleTimeoutmS);
#ifndef DbgFab0_3plus
   Serial.printf("Snooping data bus on C64 writes only (Fab 0.2x)\n", BusSampleMaxSize, BusSampleTimeoutmS);
#endif
   
   BusBitCount = (uint32_t*)calloc(NumBusBits*2, sizeof(uint32_t));
   BusSampleCount = 0;
   
   uint32_t StartTime = millis();
   fBusSnoop = &BusCount; 
   delay(1); // BusCount is never called if this isn't present
   while (BusSampleCount<BusSampleMaxSize && millis()-StartTime<BusSampleTimeoutmS); //wait to fill buffer or timeout
   fBusSnoop = NULL;

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

#ifdef DbgFab0_3plus
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