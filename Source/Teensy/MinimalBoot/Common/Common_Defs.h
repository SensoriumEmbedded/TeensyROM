
//re-compile both minimal and full if anything changes here!

char strVersionNumber[] = "v0.7.1+2"; //*VERSION*

#define UpperAddr           0x060000  //address of upper (main) TR image, from FLASH_BASEADDRESS
#define FLASH_BASEADDRESS 0x60000000

//synch with win app:
//all commands must start with 0x64
#define SetColorToken     0x6422
#define LaunchFileToken   0x6444
#define PingToken         0x6455
#define PauseSIDToken     0x6466  //df
#define SetSIDSongToken   0x6488
#define SIDSpeedLinToken  0x6499
#define SIDSpeedLogToken  0x649A
#define SIDVoiceMuteToken 0x6433
#define C64PauseOnToken   0x6431  // C64 Paused
#define C64PauseOffToken  0x6430  // C64 Unpaused
#define DebugToken        0x6467  //dg
#define VersionInfoToken  0x6476  //dv
#define SendFileToken     0x64AA
#define PostFileToken     0x64BB
#define CopyFileToken     0x64FF
#define GetFileToken      0x64B0
#define DeleteFileToken   0x64CF
#define AckToken          0x64CC
#define GetDirectoryToken 0x64DD  // regular JSON format, to be deprecated
#define GetDirNDJSONToken 0x64DE  // NDJSON format
#define ResetC64Token     0x64EE
#define RetryToken        0x9B7E
#define FailToken         0x9B7F
#define BadSIDToken       0x9B80
#define GoodSIDToken      0x9B81
#define FWCheckToken      0x64E0  // Check firmware type
#define FWMinimalToken    0x64E1  // Minimal firmware response
#define FWFullToken       0x64E2  // Full firmware response


#define eepMagicNum         0xfeed640f // 01: 6/22/23  net settings added 
                                       // 02: 9/07/23  Joy2 speed added
                                       // 03: 11/3/23  Browser Bookmarks added
                                       // 04: 11/4/23  Browser DL drive/path added
                                       // 05: 12/27/23 inverted default SID enable bit
                                       // 06: 3/13/24  Added eepAdDefaultSID
                                       // 07: 6/3/24   Added eepAdCrtBootName
                                       // 08: 7/7/24   Separate Min Boot Indicator
                                       // 09: 10/2/24  Autolaunch Indicator
                                       // 0a: 12/29/24 RW Delay default to on
                                       // 0b: 2/13/25  12 hour clock mode by default
                                       // 0c: 6/5/25   added eepAdColorRefStart
                                       // 0d: 9/12/25  Power up defaults: added file ext, removed, RW Read delay, moved 12/24 hr clk
                                       // 0e: 10/28/25 Hot key paths, Bookmark reduction
                                       // 0f: 11/12/25 Clear beta testers
                                       
enum InternalEEPROMmap
{
   eepAdMagicNum      =    0, // (4:uint32_t)   Mismatch indicates internal EEPROM needs initialization
   eepAdPwrUpDefaults =    4, // (1:uint8_t)    power up default reg, see bit mask defs RegPowerUpDefaultMasks
   eepAdTimezone      =    5, // (1:int8_t)     signed char for timezone: UTC +14/-12 
   eepAdNextIOHndlr   =    6, // (1:uint8_t)    default IO handler to load upon TR exit
   eepAdDHCPEnabled   =    7, // (1:uint8_t)    non-0=DHCP enabled, 0=DHCP disabled
   eepAdMyMAC         =    8, // (6:uint8_t x6) My MAC address
   eepAdMyIP          =   14, // (4:uint8_t x4) My IP address (static)
   eepAdDNSIP         =   18, // (4:uint8_t x4) DNS IP address (static)
   eepAdGtwyIP        =   22, // (4:uint8_t x4) Gtwy IP address (static)
   eepAdMaskIP        =   26, // (4:uint8_t x4) Mask IP address (static)
   eepAdDHCPTimeout   =   30, // (2:uint16_t)   DNS Timeout
   eepAdDHCPRespTO    =   32, // (2:uint16_t)   DNS Response Timeout
   eepAdDLPathSD_USB  =   34, // (1:uint8_t)    Download path is on SD or USB per Drive_SD/USB
   eepAdDLPath        =   35, // (TxMsgMaxSize=128)  HTTP Download path
   eepAdBookmarks     =  163, // (MaxPathLength=300)*eepNumBookmarks (5)    Bookmark Titles and Full Paths
   eepAdDefaultSID    = 1663, // (MaxPathLength=300) Path/filename of Default SID to play in background
   eepAdCrtBootName   = 1963, // (MaxPathLength=300) Boot to minimal .crt path to launch
   eepAdMinBootInd    = 2263, // (1:uint8_t)    Minimal/full boot indicator, see MinBootIndFlags
   eepAdAutolaunchName= 2264, // (MaxPathLength=300) Autolaunch path to launch or zero length for off
   eepAdPwrUpDefaults2= 2564, // (1:uint8_t)    power up default reg, see bit mask defs RegPowerUpDefaultMasks2
   eepAdColorRefStart = 2565, // (NumColorRefs=7)  UI color references, see ColorRefOffsets
   eepAdHotKeyPaths   = 2572, // (MaxPathLength=300)*NumHotKeys (5)  Default Hot Key settings

   //eepAdNext        = 2572+1500=4072, // Next address to be used
   //Max size = 4284 (4k, emulated in flash)
};

enum MinBootIndFlags
{
   MinBootInd_SkipMin    = 0, // skip minimal and go to main as normal first power up (and autolaunch, if enabled)
   MinBootInd_ExecuteMin = 1, // minimal boot called from main, launch CRT in minimal
   MinBootInd_FromMin    = 2, // Min returning to main menu, skip of autolaunch (if enabled)
   MinBootInd_LaunchFull = 3, // Launch command received in minimal, launch it from full
};

enum DMA_States  //used with DMA_State
{
   DMA_S_DisableReady,  //Disabled/default state
   DMA_S_ActiveReady,   //DMA asserted state
   DMA_S_TransferReady, //DMA asserted, ready for transfer
   
   DMA_S_BeginStartStates, //states higher than this request action during phi1 vic cycle
   
   DMA_S_StartDisable,     //deactivate/end DMA
   DMA_S_StartTransfer,    //activate for transfer
   DMA_S_StartActive,      //activate immediately
   DMA_S_Start_BA_Active,  //activate while BA is not asserted (bad line)
};

volatile uint8_t DMA_State = DMA_S_DisableReady;

bool (*fBusSnoop)(uint16_t Address, bool R_Wn) = NULL;    //Bus snoop routine, return true to skip out of phi2 isr

#ifdef Dbg_SerLog 
   #define BigBufSize          1000
#else
   #define BigBufSize          1
#endif
uint16_t BigBufCount = 0;
uint32_t* BigBuf = NULL;

#ifdef DbgMsgs_SW  //Swiftlink debug msgs
   #define Printf_dbg_sw Serial.printf
#else
   __attribute__((always_inline)) inline void Printf_dbg_sw(...) {};
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
#define MaxPathLength       300
#define MaxNamePathLength   (MaxPathLength+MaxItemNameLength+2)
#define MaxMenuItems        4000  //(Max Pages * MaxItemsPerPage) = 255 * 19 = 4845 max to keep page # 8-bit
#define SerialTimoutMillis  500
#define UpDirString         "/.. <Up Dir>"
#define NTSCBusFreq         1022730
#define PALBusFreq          985250


volatile uint32_t StartCycCnt, LastCycCnt=0;
   
#define PHI2_PIN            1  
#define Menu_Btn_In_PIN    31 
#define Special_Btn_In_PIN 28  //Used in v0.4+ only (SpecialButton)
#define DotClk_Debug_PIN   28 
#ifdef BiDirReset
   #define BiDir_Reset_PIN  6
#endif
const uint8_t InputPins[] = {
   19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,  //address bus
   2, 3, 4, 5, PHI2_PIN, 0,   // IO1n, IO2n, ROML, ROMH, PHI2_PIN, R_Wn
   29, Menu_Btn_In_PIN,  // BA, Reset button
   };

const uint8_t OutputPins[] = {
   35, 9, 32,   // DataCEn(0.2/3)/AddrBufDirControl(0.4), ExROM, Game
   30, 25, 24,  // DMA, NMI, IRQ
   34, 33,      // LED, debug(0.2)/RnW(0.3)
#ifndef BiDirReset
   6,   //Reset_Out_PIN,
#endif
   };

#ifdef DbgFab0_3plus
   #define SetDebugAssert      CORE_PIN28_PORTSET = CORE_PIN28_BITMASK
   #define SetDebugDeassert    CORE_PIN28_PORTCLEAR = CORE_PIN28_BITMASK 
#else  //fab 0.2x
   //debug on pin 33, but could be blue-wired to drive DataBufIn/Out as on fab 0.3
   //   let it be driven as SetDataBufIn/Out for blue-wired 0.2x boards
   #define SetDebugAssert      {} //CORE_PIN33_PORTSET = CORE_PIN33_BITMASK
   #define SetDebugDeassert    {} //CORE_PIN33_PORTCLEAR = CORE_PIN33_BITMASK 
#endif

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
#define ReadDotClkDebug     (ReadGPIO8 & CORE_PIN28_BITMASK)
                            
#define ReadGPIO9           (*(volatile uint32_t *)IMXRT_GPIO9_ADDRESS)
#define GP9_IO1n(r)         (r & CORE_PIN2_BITMASK)
#define GP9_IO2n(r)         (r & CORE_PIN3_BITMASK)
#define GP9_ROML(r)         (r & CORE_PIN4_BITMASK)
#define GP9_ROMH(r)         (r & CORE_PIN5_BITMASK)
#define GP9_BA(r)           (r & CORE_PIN29_BITMASK)
             
#ifdef FullDMACapable
   #define SetAddrBufsOut      CORE_PIN35_PORTSET = CORE_PIN35_BITMASK
   #define SetAddrBufsIn       CORE_PIN35_PORTCLEAR = CORE_PIN35_BITMASK
#else             
   #define DataBufDisable      CORE_PIN35_PORTSET = CORE_PIN35_BITMASK
   #define DataBufEnable       CORE_PIN35_PORTCLEAR = CORE_PIN35_BITMASK
#endif
                           
#ifdef BiDirReset
   #define SetResetAssert      CORE_PIN6_PORTCLEAR = CORE_PIN6_BITMASK; CORE_PIN6_DDRREG |= CORE_PIN6_BITMASK   //output, active low
   #define SetResetInput       CORE_PIN6_DDRREG &= ~CORE_PIN6_BITMASK  //set as input
#else
   #define SetResetAssert      CORE_PIN6_PORTCLEAR = CORE_PIN6_BITMASK  //active low
   #define SetResetDeassert    CORE_PIN6_PORTSET = CORE_PIN6_BITMASK
#endif

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
#define SetDataBufOut       CORE_PIN33_PORTSET = CORE_PIN33_BITMASK
#define SetDataBufIn        CORE_PIN33_PORTCLEAR = CORE_PIN33_BITMASK 

#ifdef FullDMACapable
   #define SetRWOutWrite       CORE_PIN0_PORTCLEAR = CORE_PIN0_BITMASK; CORE_PIN0_DDRREG |= CORE_PIN0_BITMASK   //output, low=write
   #define SetRWInput          CORE_PIN0_DDRREG &= ~CORE_PIN0_BITMASK   //set as input

   #define GP6_AddrMask        0xFFFF0000  // bits 16-31 contain address bus, in order       
   #define SetAddrPortDirOut   CORE_PIN19_DDRREG |= GP6_AddrMask
   #define SetAddrPortDirIn    CORE_PIN19_DDRREG &= ~GP6_AddrMask
#endif                            

#define CycTonS(N)          (N*(1000000000UL>>16)/(F_CPU_ACTUAL>>16))
#define nSToCyc(N)          (N*(F_CPU_ACTUAL>>16)/(1000000000UL>>16))

//#define RESET_CYCLECOUNT   { ARM_DEMCR |= ARM_DEMCR_TRCENA; ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA; ARM_DWT_CYCCNT = 0; }
#define WaitUntil_nS(N)     while((ARM_DWT_CYCCNT-StartCycCnt) < nSToCyc(N))
    
#define Def_nS_MaxAdj      1030  //    above this nS since last int causes adjustment, formerly 993 for NTSC only

// Times from Phi2 rising (interrupt start):
#define Def_nS_RWnReady     135  //    Phi2 rise to RWn valid.  
                                 //       2/4/24: Jupiter Lander ship requires 135 on NTSC Reloaded MKII (via alterationx10) 
                                 //       12/14/24: C128 & C64C JL also needs this
                                 //       9/12/25: Removing non-delay (95nS) option in settings, set to 135nS (delayed)
#define Def_nS_PLAprop      150  //    delay through PLA to decode address (IO1/2, ROML/H)
#define Def_nS_DataSetup    220  //    On a C64 write, when to latch data bus.
#define Def_nS_DataHold     390  //    On a C64 read, when to stop driving the data bus
                                 //       02/01/24 v0.5.10+:    From 350 to 365 to accomodate prg load on NTSC Reloaded MKII (via alterationx10)
                                 //       12/03/24 v0.6.3+M65:  365 to 375 special build to accomodate Mega65 ROML read
                                 //       12/12/24 v0.6.3+T390: Set to 390 special build for a Reloaded Mk2 using FW 20180227 from CryzleR/Frank.  V20231101 (latest) still fails below ~385  https://wiki.icomp.de/wiki/C64_reloaded_mk2#Firmware_updates
                                 //                             Also: Digitalman saw Mega65 improvements on Fiendish Freddy & Orbitz
                                 //       12/31/24 v0.6.4:      365 to 390 release build to accomodate all. Measurement show this is the max for staying within the Phi2 half cycle
                                 
// Times from Phi2 falling:
#define Def_nS_VICStart     210  //    delay from Phi2 falling to look for ROMH.  Too long or short will manifest as general screen noise (missing data) on ROMH games such as JupiterLander and RadarRatRace
#define Def_nS_VICDHold     365  //    On a C64 VIC cycle read, when to stop driving the data bus.  Higher breaks UltiMax carts on NTSC
#define Def_nS_DMAAssert    200  //    delay from Phi2 falling to DMA assertion when activating

uint32_t nS_MaxAdj    = Def_nS_MaxAdj; 
uint32_t nS_RWnReady  = Def_nS_RWnReady;  
uint32_t nS_PLAprop   = Def_nS_PLAprop;  
uint32_t nS_DataSetup = Def_nS_DataSetup;  
uint32_t nS_DataHold  = Def_nS_DataHold;  
uint32_t nS_VICStart  = Def_nS_VICStart;  
uint32_t nS_VICDHold  = Def_nS_VICDHold;
uint32_t nS_DMAAssert = Def_nS_DMAAssert;

__attribute__((always_inline)) inline void DataPortWriteWait(uint8_t Data)
{  // for "normal" (non-VIC) C64 read cycles only
#ifdef DataBufAlwaysEnabled
   SetDataBufOut; //buffer out first
   SetDataPortDirOut; //then set data ports to outputs
#else
   DataBufEnable; 
#endif

   uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN10_PORTSET = RegBits;
   CORE_PIN10_PORTCLEAR = ~RegBits & GP7_DataMask;
   
   //WaitUntil_nS(nS_DataHold);  
   uint32_t Cyc_DataHold = nSToCyc(nS_DataHold); //avoid calculating every time
   while((ARM_DWT_CYCCNT-StartCycCnt) < Cyc_DataHold)
      if(!GP6_Phi2(ReadGPIO6)) break; //make sure Phi2 is still high, about 50nS of overshoot into VIC cycle if detected
   
#ifdef DataBufAlwaysEnabled
   SetDataPortDirIn; //set data ports back to inputs/default
   SetDataBufIn;     //then set buffer dir to input
#else
   DataBufDisable;
#endif
}

__attribute__((always_inline)) inline void DataPortWriteWaitVIC(uint8_t Data)
{  // for C64 VIC read cycles only
#ifdef DataBufAlwaysEnabled
   SetDataBufOut; //buffer out first
   SetDataPortDirOut; //then set data ports to outputs
#else
   DataBufEnable; 
#endif

   uint32_t RegBits = (Data & 0x0F) | ((Data & 0xF0) << 12);
   CORE_PIN10_PORTSET = RegBits;
   CORE_PIN10_PORTCLEAR = ~RegBits & GP7_DataMask;
   WaitUntil_nS(nS_VICDHold);  

#ifdef DataBufAlwaysEnabled
   SetDataPortDirIn; //set data ports back to inputs/default
   SetDataBufIn;     //then set buffer dir to input
#else
   DataBufDisable;
#endif
}

__attribute__((always_inline)) inline void DataPortWriteWaitLog(uint8_t Data)
{  // for "normal" (non-VIC) C64 read cycles only
   DataPortWriteWait(Data);
   TraceLogAddValidData(Data);
}

__attribute__((always_inline)) inline uint8_t DataPortWaitRead()
{  // for "normal" (non-VIC) C64 write cycles
#ifndef DataBufAlwaysEnabled
   SetDataPortDirIn; //set data ports to inputs         //data port set to read previously
   DataBufEnable; //enable external buffer
#endif
   WaitUntil_nS(nS_DataSetup);  //could poll Phi2 for falling edge...  only 30nS typ hold time
   uint32_t DataIn = ReadGPIO7;
#ifndef DataBufAlwaysEnabled
   DataBufDisable;
   SetDataPortDirOut; //set data ports to outputs (default)
#endif
   return ((DataIn & 0x0F) | ((DataIn >> 12) & 0xF0));
}

// reboot is the same for all ARM devices
#define CPU_RESTART_ADDR   ((uint32_t *)0xE000ED0C)
#define CPU_RESTART_VAL	   (0x5FA0004)
#define REBOOT             (*CPU_RESTART_ADDR = CPU_RESTART_VAL)

//C64 specific:
enum PokeColors
{
   PokeBlack   = 0 ,
   PokeWhite   = 1 ,
   PokeRed     = 2 ,
   PokeCyan    = 3 ,
   PokePurple  = 4 ,
   PokeGreen   = 5 ,
   PokeBlue    = 6 ,
   PokeYellow  = 7 ,
   PokeOrange  = 8 ,
   PokeBrown   = 9 ,
   PokeLtRed   = 10,
   PokeDrkGrey = 11,
   PokeMedGrey = 12,
   PokeLtGreen = 13,
   PokeLtBlue  = 14,
   PokeLtGrey  = 15,
};
