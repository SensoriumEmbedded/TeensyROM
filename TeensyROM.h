
#define A0_PIN             19   
#define A1_PIN             18   
#define A2_PIN             14   
#define A3_PIN             15  
                           
#define D0_PIN             10
#define D1_PIN             12
#define D2_PIN             11
#define D3_PIN             13
#define D4_PIN              8
#define D5_PIN              7
#define D6_PIN             36
#define D7_PIN             37
                           
#define DataCEn_PIN        35     
#define IO1n_PIN           24  
#define PHI2_PIN            1  
#define R_Wn_PIN            0  

#define ReadGPIO6          (*(volatile uint32_t *)IMXRT_GPIO6_ADDRESS)
#define GP_R_Wn(r)         (r & CORE_PIN0_BITMASK)
#define GP_IO1n(r)         (r & CORE_PIN24_BITMASK)
#define GP_Address(r)      ((r >> CORE_PIN19_BIT) & 0xF)  // lower nibble bits 16-19 
                           
#define ReadGPIO7          (*(volatile uint32_t *)IMXRT_GPIO7_ADDRESS)
#define GP7_DataMask       0xF000F   //CORE_PIN10,12,11,13,8,7,36,37_BITMASK
#define DataBufDisable     CORE_PIN35_PORTSET = CORE_PIN35_BITMASK
#define DataBufEnable      CORE_PIN35_PORTCLEAR = CORE_PIN35_BITMASK
#define SetDataPortWrite   CORE_PIN10_DDRREG |= GP7_DataMask
#define SetDataPortRead    CORE_PIN10_DDRREG &= ~GP7_DataMask

#define Wait_nS(S, N)      while(ARM_DWT_CYCCNT - S < ((F_CPU_ACTUAL>>16) * N) / (1000000000UL>>16)) 
// all times starting from Phi2 rising (interrupt). 
//could move to interrupts on IO1/2/ROML/H, or decode full bus instead.
#define nS_PLAprop         100 //delay through PLA to decode address (IO1/2, ROML/H), have measured >100nS from Phi2 to IO1 (delayed through PLA, etc)
#define nS_DataSetup       325 //On a write, when to latch data bus. spec calls for 150-200nS min to Data valid for write opperation (TMDS)
#define nS_DataHold        375  //On a read, when to stop driving the data bus, spec calls for >430

__attribute__((always_inline)) inline void SetDataPortOutWait(uint8_t Data, uint32_t StartCycles)
{
   DataBufEnable; 
   register uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN7_PORTSET = RegBits;
   CORE_PIN7_PORTCLEAR = ~RegBits & GP7_DataMask;
   Wait_nS(StartCycles, nS_DataHold);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   DataBufDisable;
}

__attribute__(( always_inline )) inline uint8_t DataPortRead(uint32_t StartCycles)
{
   SetDataPortRead; //set data ports to inputs         //data port set to read previously
   DataBufEnable; //enable external buffer
   Wait_nS(StartCycles, nS_DataSetup);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   register uint32_t DataIn = ReadGPIO7;
   DataBufDisable;
   SetDataPortWrite; //set data ports to outputs (default)
   return ((DataIn & 0x0F) | ((DataIn & 0xF0) >> 12));
}