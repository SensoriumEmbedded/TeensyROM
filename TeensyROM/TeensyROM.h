
#define SerialTimoutMillis 500
uint32_t StartCycCnt;


#define PHI2_PIN            1  
#define Reset_Btn_In_PIN         31  
const uint8_t InputPins[] = {
   19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,  //address bus
   2, 3, 4, 5, PHI2_PIN, 0,         // IO1n, IO2n, ROML, ROMH, PHI2_PIN, R_Wn
   28, 29, Reset_Btn_In_PIN,    //DOT clk, BA, Reset button
   };

const uint8_t OutputPins[] = {
   35, 9, 32, // DataCEn, ExROM, Game
   30, 25, 24,   //DMA, NMI, IRQ
   34,33,     //LED, debug
   6,    //Reset_Out_PIN,
   };

#define ReadGPIO6          (*(volatile uint32_t *)IMXRT_GPIO6_ADDRESS)
#define GP6_R_Wn(r)        (r & CORE_PIN0_BITMASK)
#define GP6_Address(r)     ((r >> CORE_PIN19_BIT) & 0xFFFF)  // bits 16-31 contain address bus, in order 

#define ReadGPIO9          (*(volatile uint32_t *)IMXRT_GPIO9_ADDRESS)
#define GP9_IO1n(r)        (r & CORE_PIN2_BITMASK)
#define GP9_IO2n(r)        (r & CORE_PIN3_BITMASK)
#define GP9_ROML(r)        (r & CORE_PIN4_BITMASK)
#define GP9_ROMH(r)        (r & CORE_PIN5_BITMASK)
#define GP9_BA(r)          (r & CORE_PIN29_BITMASK)

#define ReadGPIO7          (*(volatile uint32_t *)IMXRT_GPIO7_ADDRESS)
#define GP7_DataMask       0xF000F   //CORE_PIN10,12,11,13,8,7,36,37_BITMASK
#define SetDataPortDirOut  CORE_PIN10_DDRREG |= GP7_DataMask
#define SetDataPortDirIn   CORE_PIN10_DDRREG &= ~GP7_DataMask

#define DataBufDisable     CORE_PIN35_PORTSET = CORE_PIN35_BITMASK
#define DataBufEnable      CORE_PIN35_PORTCLEAR = CORE_PIN35_BITMASK

#define SetResetAssert     CORE_PIN6_PORTCLEAR = CORE_PIN6_BITMASK  //active low
#define SetResetDeassert   CORE_PIN6_PORTSET = CORE_PIN6_BITMASK
//#define SetResetAssert     { CORE_PIN6_DDRREG  |= CORE_PIN6_BITMASK; CORE_PIN6_PORTCLEAR = CORE_PIN6_BITMASK; }   //make output, drive low
//#define SetResetRelease    CORE_PIN6_DDRREG  &= ~CORE_PIN6_BITMASK      //make input (OC)
#define ReadGPIO8          (*(volatile uint32_t *)IMXRT_GPIO8_ADDRESS)
#define ReadButton         (ReadGPIO8 & CORE_PIN31_BITMASK)

#define SetExROMAssert     CORE_PIN9_PORTCLEAR = CORE_PIN9_BITMASK  //active low
#define SetExROMDeassert   CORE_PIN9_PORTSET = CORE_PIN9_BITMASK
#define SetGameAssert      CORE_PIN32_PORTCLEAR = CORE_PIN32_BITMASK //active low
#define SetGameDeassert    CORE_PIN32_PORTSET = CORE_PIN32_BITMASK 
#define SetDMAAssert       CORE_PIN30_PORTCLEAR = CORE_PIN30_BITMASK  //active low
#define SetDMADeassert     CORE_PIN30_PORTSET = CORE_PIN30_BITMASK
#define SetNMIAssert       CORE_PIN25_PORTCLEAR = CORE_PIN25_BITMASK //active low
#define SetNMIDeassert     CORE_PIN25_PORTSET = CORE_PIN25_BITMASK 
#define SetIRQAssert       CORE_PIN24_PORTCLEAR = CORE_PIN24_BITMASK //active low
#define SetIRQDeassert     CORE_PIN24_PORTSET = CORE_PIN24_BITMASK 

#define SetLEDOn           CORE_PIN34_PORTSET = CORE_PIN34_BITMASK
#define SetLEDOff          CORE_PIN34_PORTCLEAR = CORE_PIN34_BITMASK 
#define SetDebug2Assert    CORE_PIN33_PORTSET = CORE_PIN33_BITMASK
#define SetDebugDeassert  CORE_PIN33_PORTCLEAR = CORE_PIN33_BITMASK 

//Use only inside Phi2 isr:
//#define RESET_CYCLECOUNT   { ARM_DEMCR |= ARM_DEMCR_TRCENA; ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA; ARM_DWT_CYCCNT = 0; }
#define WaitUntil_nS(N)    while((ARM_DWT_CYCCNT-StartCycCnt) < ((F_CPU_ACTUAL>>16) * N) / (1000000000UL>>16))
    //Could reduce or use whole cycle counts instead of nS... while(ARM_DWT_CYCCNT < N * 0.816)      F_CPU_ACTUAL=816000000  /1000000000 = 0.816
// all times starting from Phi2 rising (interrupt). 
#define nS_RWnReady        20  //Phi2 rise to RWn valid, takes ~30nS past Phi2 to go low for write
#define nS_PLAprop         100 //delay through PLA to decode address (IO1/2, ROML/H), have measured >100nS from Phi2 to IO1 (delayed through PLA, etc)
#define nS_DataSetup       325 //On a write, when to latch data bus. spec calls for 150-200nS min to Data valid for write opperation (TMDS)
#define nS_DataHold        400 //On a read, when to stop driving the data bus, spec calls for >430

__attribute__((always_inline)) inline void DataPortWriteWait(uint8_t Data)
{
   DataBufEnable; 
   register uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN7_PORTSET = RegBits;
   CORE_PIN7_PORTCLEAR = ~RegBits & GP7_DataMask;
   WaitUntil_nS(nS_DataHold);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   DataBufDisable;
}

__attribute__(( always_inline )) inline uint8_t DataPortWaitRead()
{
   SetDataPortDirIn; //set data ports to inputs         //data port set to read previously
   DataBufEnable; //enable external buffer
   WaitUntil_nS(nS_DataSetup);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   register uint32_t DataIn = ReadGPIO7;
   DataBufDisable;
   SetDataPortDirOut; //set data ports to outputs (default)
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}


