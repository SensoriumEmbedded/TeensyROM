// MIT License
// 
// Copyright (c) 2023 Travis Smith
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, copy, modify, merge, publish, 
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
// the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "ROMs/TeensyROMC64.h" //TeensyROM Menu cart, stored in RAM
#include "MinimalBoot\Common\Common_Defs.h"

char strVersionNumber[] = "v0.6"; //*VERSION*

//Build options: enable debug messaging at your own risk, can cause emulation interference/fails
//#define DbgMsgs_IO     //Serial out messages (Printf_dbg): Swift, MIDI (mostly out), CRT Chip info

//less used:
// #define DbgMsgs_M2S   //MIDI2SID MIDI handler messages
// #define DbgIOTraceLog //Logs Reads/Writes to/from IO1 to BigBuf. Like debug handler but can use for others
// #define DbgCycAdjLog  //Logs ISR timing adjustments to BigBuf.
// #define DbgSpecial    //Special case logging to BigBuf
// #define Dbg_SerTimChg //Serial commands that tweak timing parameters.
// #define Dbg_SerSwift  //Serial commands that tweak SwiftLink parameters.
// #define Dbg_SerLog    //Serial commands that display log info
// #define Dbg_SerMem    //Serial commands that display memory info
// #define Dbg_SerASID   //Serial commands that test the ASID player


#define nfcScanner     //nfc scanner libs/code included in build
#define nfcStateEnabled       0
#define nfcStateBitDisabled   1
#define nfcStateBitPaused     2

#define eepBMTitleSize       75  //max chars in bookmark title
#define eepBMURLSize        225  //Max Chars in bookmark URL path
#define eepNumBookmarks       9  //Num Bookmarks saved

#define BigBufSize          5
uint16_t BigBufCount = 0;
uint32_t* BigBuf = NULL;

#ifdef nfcScanner
   #define MaxRAM_ImageSize  (184-40)  // ~18k added by host serial & nfc libs, crossed a 32k code boundry (22k more padding)
   //"626k Free"
   uint8_t Lastuid[7];  // Buffer to store the last UID read
#else
   #define MaxRAM_ImageSize  184  //normal max 
   //"666k Free"
#endif

#ifdef DbgMsgs_IO  //Debug msgs mode: reduced RAM_ImageSize
   #define Printf_dbg Serial.printf
   #define RAM_ImageSize       ((MaxRAM_ImageSize-24)*1024)
#else //Normal mode: maximize RAM_ImageSize
   __attribute__((always_inline)) inline void Printf_dbg(...) {};
   #define RAM_ImageSize       (MaxRAM_ImageSize*1024)
#endif

#define IOTLRead            0x10000
#define IOTLDataValid       0x20000
#define AdjustedCycleTiming 0x40000
#define DbgSpecialData      0x80000

#ifdef DbgIOTraceLog
   __attribute__((always_inline)) inline void TraceLogAddValidData(uint8_t data) {BigBuf[BigBufCount] |= (data<<8) | IOTLDataValid;};
#else
   __attribute__((always_inline)) inline void TraceLogAddValidData(...) {};
#endif

#define MaxItemNameLength   100
#define MaxPathLength       300   //Default SID path saved in EEPROM is tied to this
#define MaxNamePathLength   (MaxPathLength+MaxItemNameLength+2)
#define MaxMenuItems        3000  //(Max Pages * MaxItemsPerPage) = 255 * 16 = 4080 max to keep page # 8-bit
#define SerialTimoutMillis  500
#define UpDirString         "/.. <Up Dir>"
#define NTSCBusFreq         1022730
#define PALBusFreq          985250
#define DefSIDSource        rmtTeensy  // Default should always be local (rmtTeensy)
#define DefSIDPath          "/SID Cover Tunes" 
#define DefSIDName          "Sleep Dirt            Frank Zappa" 


volatile uint32_t StartCycCnt, LastCycCnt=0;
   
#define PHI2_PIN            1  
#define Reset_Btn_In_PIN    31  
const uint8_t InputPins[] = {
   19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,  //address bus
   2, 3, 4, 5, PHI2_PIN, 0,   // IO1n, IO2n, ROML, ROMH, PHI2_PIN, R_Wn
   28, 29, Reset_Btn_In_PIN,  // DOT clk, BA, Reset button
   };

const uint8_t OutputPins[] = {
   35, 9, 32,   // DataCEn, ExROM, Game
   30, 25, 24,  // DMA, NMI, IRQ
   34,33, 6,    // LED, debug, Reset_Out_PIN,
   };

#define ReadGPIO6           (*(volatile uint32_t *)IMXRT_GPIO6_ADDRESS)
#define GP6_R_Wn(r)         (r & CORE_PIN0_BITMASK)
#define GP6_Phi2(r)         (r & CORE_PIN1_BITMASK)
#define GP6_Address(r)      ((r >> CORE_PIN19_BIT) & 0xFFFF)  // bits 16-31 contain address bus, in order 
                            
#define ReadGPIO7           (*(volatile uint32_t *)IMXRT_GPIO7_ADDRESS)
#define GP7_DataMask        0xF000F   //CORE_PIN10,12,11,13,8,7,36,37_BITMASK
#define SetDataPortDirOut   CORE_PIN10_DDRREG |= GP7_DataMask
#define SetDataPortDirIn    CORE_PIN10_DDRREG &= ~GP7_DataMask
                            
#define ReadGPIO8           (*(volatile uint32_t *)IMXRT_GPIO8_ADDRESS)
#define ReadButton          (ReadGPIO8 & CORE_PIN31_BITMASK)
                            
#define ReadGPIO9           (*(volatile uint32_t *)IMXRT_GPIO9_ADDRESS)
#define GP9_IO1n(r)         (r & CORE_PIN2_BITMASK)
#define GP9_IO2n(r)         (r & CORE_PIN3_BITMASK)
#define GP9_ROML(r)         (r & CORE_PIN4_BITMASK)
#define GP9_ROMH(r)         (r & CORE_PIN5_BITMASK)
#define GP9_BA(r)           (r & CORE_PIN29_BITMASK)
                            
#define DataBufDisable      CORE_PIN35_PORTSET = CORE_PIN35_BITMASK
#define DataBufEnable       CORE_PIN35_PORTCLEAR = CORE_PIN35_BITMASK
                            
#define SetResetAssert      CORE_PIN6_PORTCLEAR = CORE_PIN6_BITMASK  //active low
#define SetResetDeassert    CORE_PIN6_PORTSET = CORE_PIN6_BITMASK
#define SetExROMAssert      CORE_PIN9_PORTCLEAR = CORE_PIN9_BITMASK  //active low
#define SetExROMDeassert    CORE_PIN9_PORTSET = CORE_PIN9_BITMASK
#define SetGameAssert       CORE_PIN32_PORTCLEAR = CORE_PIN32_BITMASK //active low
#define SetGameDeassert     CORE_PIN32_PORTSET = CORE_PIN32_BITMASK 
#define SetDMAAssert        CORE_PIN30_PORTCLEAR = CORE_PIN30_BITMASK  //active low
#define SetDMADeassert      CORE_PIN30_PORTSET = CORE_PIN30_BITMASK
#define SetNMIAssert        CORE_PIN25_PORTCLEAR = CORE_PIN25_BITMASK //active low
#define SetNMIDeassert      CORE_PIN25_PORTSET = CORE_PIN25_BITMASK 
#define SetIRQAssert        CORE_PIN24_PORTCLEAR = CORE_PIN24_BITMASK //active low
#define SetIRQDeassert      CORE_PIN24_PORTSET = CORE_PIN24_BITMASK 
                            
#define SetLEDOn            CORE_PIN34_PORTSET = CORE_PIN34_BITMASK
#define SetLEDOff           CORE_PIN34_PORTCLEAR = CORE_PIN34_BITMASK 
#define SetDebugAssert      CORE_PIN33_PORTSET = CORE_PIN33_BITMASK
#define SetDebugDeassert    CORE_PIN33_PORTCLEAR = CORE_PIN33_BITMASK 
                            
#define CycTonS(N)          (N*(1000000000UL>>16)/(F_CPU_ACTUAL>>16))
#define nSToCyc(N)          (N*(F_CPU_ACTUAL>>16)/(1000000000UL>>16))

//#define RESET_CYCLECOUNT   { ARM_DEMCR |= ARM_DEMCR_TRCENA; ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA; ARM_DWT_CYCCNT = 0; }
#define WaitUntil_nS(N)     while((ARM_DWT_CYCCNT-StartCycCnt) < nSToCyc(N))
    
#define Def_nS_MaxAdj      1030  //    above this nS since last int causes adjustment, formerly 993 for NTSC only

                                 // Times from Phi2 rising (interrupt):
#define Def_nS_RWnReady      95  //    Phi2 rise to RWn valid.  
#define Def_nS_RWnReady_dly 135  //       2/4/24: Jupiter Lander ship requires 135 on NTSC Reloaded MKII (via alterationx10) 
#define Def_nS_PLAprop      150  //    delay through PLA to decode address (IO1/2, ROML/H)
#define Def_nS_DataSetup    220  //    On a C64 write, when to latch data bus.
#define Def_nS_DataHold     365  //    On a C64 read, when to stop driving the data bus
                                 //       2/1/24 v0.5.10+: updated from 350 to 365 to accomodate prg load on NTSC Reloaded MKII (via alterationx10)
                                 
                                 // Times from Phi2 falling:
#define Def_nS_VICStart     210  //    delay from Phi2 falling to look for ROMH.  Too long or short will manifest as general screen noise (missing data) on ROMH games such as JupiterLander and RadarRatRace
                                 //    Hold time for VIC cycle is same as normal cyc (nS_DataHold)

uint32_t nS_MaxAdj    = Def_nS_MaxAdj; 
uint32_t nS_RWnReady  = Def_nS_RWnReady;  
uint32_t nS_PLAprop   = Def_nS_PLAprop;  
uint32_t nS_DataSetup = Def_nS_DataSetup;  
uint32_t nS_DataHold  = Def_nS_DataHold;  
uint32_t nS_VICStart  = Def_nS_VICStart;  


__attribute__((always_inline)) inline void DataPortWriteWait(uint8_t Data)
{
   DataBufEnable; 
   uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN7_PORTSET = RegBits;
   CORE_PIN7_PORTCLEAR = ~RegBits & GP7_DataMask;
   WaitUntil_nS(nS_DataHold);  
   DataBufDisable;
}

__attribute__((always_inline)) inline void DataPortWriteWaitLog(uint8_t Data)
{
   DataPortWriteWait(Data);
   TraceLogAddValidData(Data);
}

__attribute__((always_inline)) inline uint8_t DataPortWaitRead()
{
   SetDataPortDirIn; //set data ports to inputs         //data port set to read previously
   DataBufEnable; //enable external buffer
   WaitUntil_nS(nS_DataSetup);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   uint32_t DataIn = ReadGPIO7;
   DataBufDisable;
   SetDataPortDirOut; //set data ports to outputs (default)
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}

// reboot is the same for all ARM devices
#define CPU_RESTART_ADDR	((uint32_t *)0xE000ED0C)
#define CPU_RESTART_VAL		(0x5FA0004)
#define REBOOT			(*CPU_RESTART_ADDR = CPU_RESTART_VAL)
