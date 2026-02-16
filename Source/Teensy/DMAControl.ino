

#ifdef Fab04_FullDMACapable


volatile uint8_t DataVal = 0x55;
bool DMA_RnW = false; //true=read, false=write

#define C64Address    0x00FE

__attribute__((always_inline)) inline uint8_t DataPortWaitDMARead()
{  // for "normal" (non-VIC) C64 write cycles
#ifndef Fab04_DataBufAlwaysEnabled
   SetDataPortDirIn; //set data ports to inputs         //data port set to read previously
   DataBufEnable; //enable external buffer
#endif
   WaitUntil_nS(320);  //nS_DataSetup=220  //EXPERIMENTATION NEEDED: takes a little longer in DMA
   uint32_t DataIn = ReadGPIO7;
#ifndef Fab04_DataBufAlwaysEnabled
   DataBufDisable;
   SetDataPortDirOut; //set data ports to outputs (default)
#endif
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}

FLASHMEM void PerformDMA()
{
   DMA_State = DMA_S_StartTransfer;

   while (DMA_State != DMA_S_DisableReady) delay(1);  //block until finished
   
   Serial.printf("DMA R/W addr $%04x : $%02x = %d\n", C64Address, DataVal, DataVal);
}

void DMATransfer()
{
   // called from Phi2 isr when (DMA_State == DMA_S_TransferReady)
   
   if (!GP9_BA(ReadGPIO9)) return;  // bus not available, skip until it is

   uint32_t RegAddrBits = (C64Address << 16);

   //EXPERIMENTATION NEEDED:
   //Find a better way to do this, address needs to be latched early in cycle
   //   Call this during VIC cycle?  BA transitions high ~100nS into it.  Could catch and wait there.
   //   Work without wait if we skip the first cycle after BA?
   //   or a way to pull in?
   
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
   //phi2 has gone low..........................................................................
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   //phi2 has gone high..........................................................................
   StartCycCnt = ARM_DWT_CYCCNT;

   SetAddrBufsOut;   //set address buffers to output
   SetAddrPortDirOut;//set address ports to output   
   //set address port value:
   CORE_PIN19_PORTSET = RegAddrBits;
   CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;

   //eventually need a pointer/counter for data to be sent/received
   if (DMA_RnW)
   {  //Read Cycle: 
      //leave R/*W as input (Pulled Up, Read)
      //DataVal = DataPortWaitRead();
      DataVal = DataPortWaitDMARead(); //wait for data, read it
   }
   else
   {  //Write Cycle:
      SetRWOutWrite;
      DataPortWriteWait(DataVal);
      SetRWInput; //set R/*W back to input
   }

   SetAddrPortDirIn;//set address ports to input
   SetAddrBufsIn;   //set address buffers to input
   DMA_State = DMA_S_StartDisable;
}

#endif
