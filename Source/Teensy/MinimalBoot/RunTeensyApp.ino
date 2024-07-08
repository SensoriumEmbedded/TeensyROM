

// This code provided by AndyA via the PJRC forum
// https://forum.pjrc.com/index.php?threads/teensy-4-1-dual-boot-capability.74479/post-339451

#include "core_cm7_min.h"  //stripped down version to get defines needed

#define  FLASH_BASEADDRESS    0x60000000

typedef  void (*pFunction)(void);

FLASHMEM void runMainTRApp() 
{
  uint32_t imageStartAddress = FLASH_BASEADDRESS + UpperAddr; //point to main TR image

  // check For Valid Image: SPIFlashConfigMagicWord and VectorTableMagicWord
  if ((*((uint32_t*)imageStartAddress) != 0x42464346) || (*((uint32_t*)(imageStartAddress+0x1000)) != 0x432000D1))
  {
     ErrorLoopForever("Invalid magic numbers, no flash image found");
  }

  // ivt starts 0x1000 after the start of flash. Address of start of code is 2nd vector in table.
  uint32_t firstInstructionPtr = imageStartAddress + 0x1000 + sizeof(uint32_t);
  Serial.printf("First instruction ptr: 0x%08X\n", firstInstructionPtr);
  uint32_t firstInstructionAddr = *(uint32_t*)firstInstructionPtr;

  // very basic sanity check, code should start after the ivt but not too far into the image.
  // Address of first instruction %08X isn't sensible for location in flash. Image was probably incorrectly built
  if ( (firstInstructionAddr < (imageStartAddress+0x1000)) || (firstInstructionAddr > (imageStartAddress+0x3000)) ) 
  {
    Serial.printf("%08X ", firstInstructionAddr);
    ErrorLoopForever("Unexp first inst loc");
  }
  Serial.printf("Jumping to code at 0x%08X\n", firstInstructionAddr);
  delay(10); // give the serial port time to output so we see that message.

  pFunction Target_Code_Address = (pFunction) firstInstructionAddr;
  disableCache();
  Target_Code_Address();
  
  ErrorLoopForever("Shouldn't be here");
}

FLASHMEM void ErrorLoopForever(const char *Msg)
{
  Serial.print("\nError: ");
  Serial.println(Msg);
  while(true) 
  {
    Serial.print(".");
    delay(1000);
  }
}

// disable and invalidate both caches. Disable MPU.
// assembled from bits of core_cm7.h
// uses various #defines from that file that will need to be included.
// probably some library functions we could call instead of this.
FLASHMEM void disableCache() {

  SCB_MPU_CTRL = 0; // turn off MPU
  SYST_CSR = 0; // turn off system tick
  // disable all interrupts
  for (int i=0;i<NVIC_NUM_INTERRUPTS;i++) {
    NVIC_DISABLE_IRQ(i);
  }

  uint32_t ccsidr;
  uint32_t sets;
  uint32_t ways;

  SCB->CSSELR = 0U; /*(0U << 1U) | 0U;*/  /* Level 1 data cache */
  asm("dsb");

  SCB->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;  /* disable D-Cache */
  asm("dsb");

  ccsidr = SCB->CCSIDR;
                                            /* clean & invalidate D-Cache */
  sets = (uint32_t)(CCSIDR_SETS(ccsidr));
  do {
    ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
    do {
      SCB->DCCISW = (((sets << SCB_DCCISW_SET_Pos) & SCB_DCCISW_SET_Msk) |
                     ((ways << SCB_DCCISW_WAY_Pos) & SCB_DCCISW_WAY_Msk)  );
    } while (ways-- != 0U);
  } while(sets-- != 0U);

  asm("dsb");
  asm("isb");
  SCB->CCR &= ~(uint32_t)SCB_CCR_IC_Msk;  /* disable I-Cache */
  SCB->ICIALLU = 0UL;                     /* invalidate I-Cache */
  asm("dsb");
  asm("isb");
}

