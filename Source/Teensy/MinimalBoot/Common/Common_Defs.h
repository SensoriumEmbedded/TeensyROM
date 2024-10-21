
//re-compile both minimal and full if anything changes here!

#define MaxCRTKB      875   //based on calc from minimal image

#define UpperAddr           0x040000  //address of upper (main) TR image, from FLASH_BASEADDRESS
#define FLASH_BASEADDRESS 0x60000000


//synch with win app:
//all commands must start with 0x64
#define LaunchFileToken   0x6444
#define PingToken         0x6455
#define PauseSIDToken     0x6466
#define SetSIDSongToken   0x6488
#define DebugToken        0x6467
#define SendFileToken     0x64AA
#define PostFileToken     0x64BB
#define CopyFileToken     0x64FF
#define GetFileToken      0x64B0
#define DeleteFileToken   0x64CF
#define AckToken          0x64CC
#define GetDirectoryToken 0x64DD
#define ResetC64Token     0x64EE
#define RetryToken        0x9B7E
#define FailToken         0x9B7F
#define BadSIDToken       0x9B80
#define GoodSIDToken      0x9B81


#define eepMagicNum         0xfeed6409 // 01: 6/22/23  net settings added 
                                       // 02: 9/07/23  Joy2 speed added
                                       // 03: 11/3/23  Browser Bookmarks added
                                       // 04: 11/4/23  Browser DL drive/path added
                                       // 05: 12/27/23 inverted default SID enable bit
                                       // 06: 3/13/24  Added eepAdDefaultSID
                                       // 07: 6/3/24   Added eepAdCrtBootName (unreleased)
                                       // 08: 7/7/24   Separate Min Boot Indicator
                                       // 09: 10/2/24  Autolaunch Indicator
                                       
enum InternalEEPROMmap
{
   eepAdMagicNum      =    0, // (4:uint32_t)   Mismatch indicates internal EEPROM needs initialization
   eepAdPwrUpDefaults =    4, // (1:uint8_t)    power up default reg, see bit mask defs rpudSIDPauseMask, rpudNetTimeMask
   eepAdTimezone      =    5, // (1:int8_t)     signed char for timezone: UTC +14/-12 
   eepAdNextIOHndlr   =    6, // (1:uint8_t)    default IO handler to load upon TR exit
   eepAdDHCPEnabled   =    7, // (1:uint8_t)    non-0=DHCP enabled, 0=DHCP disabled
   eepAdMyMAC         =    8, // (6:uint8_t x6) default IO handler to load upon TR exit
   eepAdMyIP          =   14, // (4:uint8_t x4) My IP address (static)
   eepAdDNSIP         =   18, // (4:uint8_t x4) DNS IP address (static)
   eepAdGtwyIP        =   22, // (4:uint8_t x4) Gtwy IP address (static)
   eepAdMaskIP        =   26, // (4:uint8_t x4) Mask IP address (static)
   eepAdDHCPTimeout   =   30, // (2:uint16_t)   DNS Timeout
   eepAdDHCPRespTO    =   32, // (2:uint16_t)   DNS Response Timeout
   eepAdDLPathSD_USB  =   34, // (1:uint8_t)    Download path is on SD or USB per Drive_SD/USB
   eepAdDLPath        =   35, // (TxMsgMaxSize=128)  HTTP Download path
   eepAdBookmarks     =  163, // (75+225)*9     Bookmark Titles and Full Paths
   eepAdDefaultSID    = 2863, // (MaxPathLength=300) Path/filename of Default SID to play in background
   eepAdCrtBootName   = 3163, // (MaxPathLength=300) Boot to minimal .crt path to launch
   eepAdMinBootInd    = 3463, // (1:uint8_t)    Indicates that Minimal boot should execute eepAdCrtBootName (!=0) or passthrough (=0)
   eepAdAutolaunchName= 3464, // (MaxPathLength=300) Autolaunch path to launch or zero length for off

   eepAdNext          = 3764, // Next address to be used
   //Max size = 4284 (4k, emulated in flash)
};

enum MinBootIndFlags
{
   MinBootInd_SkipMin    = 0, // skip minimal and go to main as normal first power up (and autolaunch, if enabled)
   MinBootInd_ExecuteMin = 1, // minimal boot called from main, launch CRT in minimal
   MinBootInd_FromMin    = 2, // Min returning to main menu, skip of autolaunch (if enabled)
};
