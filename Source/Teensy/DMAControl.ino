

#ifdef Fab04_FullDMACapable


volatile uint8_t DataVal = 0x55;
bool DMA_RnW, DMA_FixC64Addr;
uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
uint8_t *DMA_Buffer;

//These assume Fab04_DataBufAlwaysEnabled
__attribute__((always_inline)) inline uint8_t DataPortWaitReadDMA()
{  // for "normal" (non-VIC) C64 write cycles
   WaitUntil_nS(370);  //nS_DataSetup=220  //takes a little longer for read data DMA
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
   
   WaitUntil_nS(430);  // nS_DataHold = 390 (err), 470 OK, 430 OK(?)
   //not checking Phi2 state due to tight timing and early in cycle call can cause early exit

   SetDataPortDirIn; //set data ports back to inputs/default
   SetDataBufIn;     //then set buffer dir to input
}

FLASHMEM void PerformDMA(bool RnW, uint16_t StartAddr, uint8_t *Buffer, uint32_t Length, bool FixC64Addr)
{
   //Uses DMA to Read or Write C64 memory to/from *DMABuffer
   DMA_RnW = RnW; //true=read, false=write
   DMA_Count = 0;
   DMA_StartAddr = StartAddr;
   DMA_Buffer = Buffer;
   DMA_Length = Length;
   DMA_FixC64Addr = FixC64Addr;
   
   DMA_State = DMA_S_StartTransfer;
   while (DMA_State != DMA_S_TransferComplete) delayMicroseconds(2);  //block until finished

   delayMicroseconds(2); //wait a couple cycles in case of restart

   Printf_dbg("DMA %s addr $%04x:$%04x (len: $%04x)\n", (RnW ? "Read":"Write"), StartAddr, StartAddr+Length-1, Length);
}

FLASHMEM void CloseDMA()
{
   DMA_State = DMA_S_StartDisable;
   while (DMA_State != DMA_S_DisableReady ) delayMicroseconds(2);  //block main loop until finished
}

void DMATransferISR()
{
   // called from Phi2 isr when (DMA_State == DMA_S_TransferReady)
   
   if (!GP9_BA(ReadGPIO9)) return;  // bus not available, skip until it is
   uint32_t RegAddrBits;

   //skip one cycle, re-align to edge and in cade first after BA
   while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
   while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
   
   while (DMA_Count != DMA_Length)
   {
      while(GP6_Phi2(ReadGPIO6)); //Find phi2 falling
      //phi2 has gone low..........................................................................     
      StartCycCnt = ARM_DWT_CYCCNT;
      
      if (DMA_FixC64Addr) RegAddrBits = (DMA_StartAddr << 16);
      else RegAddrBits = ((DMA_StartAddr+DMA_Count) << 16);
   
      WaitUntil_nS(200);  //BA transitions high ~100nS in
      if (!GP9_BA(ReadGPIO9)) return;  // bus not available, skip until it is
      
      CORE_PIN19_PORTSET = RegAddrBits; //set address port value to be ready for output drive
      CORE_PIN19_PORTCLEAR = ~RegAddrBits & GP6_AddrMask;
          
      //while(!GP6_Phi2(ReadGPIO6)); //Find phi2 rising
      WaitUntil_nS(400);  //use timer instead to get just ahead of the transition.     
      //phi2 has gone high..........................................................................      
      StartCycCnt = ARM_DWT_CYCCNT;
            
      if (DMA_RnW)
      {  //Read Cycle: 
         //leave R/*W as input (Pulled Up, Read)
         SetAddrBufsOut;   //set address buffers to output
         SetAddrPortDirOut;//set address ports to output   
         DMA_Buffer[DMA_Count] = DataPortWaitReadDMA(); //wait for data, read it.  Different timing from DataPortWaitRead()
      }
      else
      {  //Write Cycle:
         SetRWOutWrite;    // <---- set this first/quickly!
         SetAddrBufsOut;   //set address buffers to output
         SetAddrPortDirOut;//set address ports to output 
         DataPortWriteWaitDMA(DMA_Buffer[DMA_Count]);
         SetRWInput; //set R/*W back to input
      }

      SetAddrPortDirIn;//set address ports to input
      SetAddrBufsIn;   //set address buffers to input
      
      DMA_Count++;
   }
   DMA_State = DMA_S_TransferComplete;
}

#endif
