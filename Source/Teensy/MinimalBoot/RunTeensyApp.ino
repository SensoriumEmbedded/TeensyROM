

// This code provided by AndyA via the PJRC forum
// https://forum.pjrc.com/index.php?threads/teensy-4-1-dual-boot-capability.74479/post-339451

#include "core_cm7_min.h"  //stripped down version to get defines needed

const uint32_t FLASH_BASEADDRESS = 0x60000000;

bool checkForValidImage(uint32_t addressInFlash) {
  uint32_t SPIFlashConfigMagicWord = *((uint32_t*)addressInFlash);
  uint32_t VectorTableMagicWord = *((uint32_t*)(addressInFlash+0x1000));
  if ((SPIFlashConfigMagicWord == 0x42464346) && (VectorTableMagicWord==0x432000D1))
    return true;
 
  Serial.println("Invalid magic numbers, no flash image found");
  return false;
}

typedef  void (*pFunction)(void);

FLASHMEM bool runApp(uint32_t offsetFromStart) {
  uint32_t imageStartAddress = FLASH_BASEADDRESS+offsetFromStart;
  if (!checkForValidImage(imageStartAddress))
    LoopForever();
    //return false;
 
  // ivt starts 0x1000 after the start of flash. Address of start of code is 2nd vector in table.
  uint32_t firstInstructionPtr = imageStartAddress + 0x1000 + sizeof(uint32_t);
  Serial.printf("First instruction pointer is at address 0x%08X\r\n", firstInstructionPtr);
  uint32_t firstInstructionAddr = *(uint32_t*)firstInstructionPtr;

  // very basic sanity check, code should start after the ivt but not too far into the image.
  if ( (firstInstructionAddr < (imageStartAddress+0x1000)) || (firstInstructionAddr > (imageStartAddress+0x3000)) ) 
  {
    Serial.printf("Address of first instruction %08X isn't sensible for location in flash. Image was probably incorrectly built\r\n", firstInstructionAddr);
    LoopForever();
    //return false;
  }
  Serial.printf("Jumping to code at 0x%08X\r\n", firstInstructionAddr);
  delay(10); // give the serial port time to output so we see that message.

  pFunction Target_Code_Address = (pFunction) firstInstructionAddr;
  disableCache();
  Target_Code_Address();
  Serial.println("Shouldn't be here");
  LoopForever();
}

void LoopForever()
{
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

