
#define MaxCRTKB      867   //based on calc from minimal image
                            

#define eepMagicNum         0xfeed6408 // 01: 6/22/23  net settings added 
                                       // 02: 9/07/23  Joy2 speed added
                                       // 03: 11/3/23  Browser Bookmarks added
                                       // 04: 11/4/23  Browser DL drive/path added
                                       // 05: 12/27/23 inverted default SID enable bit
                                       // 06: 3/13/24  Added eepAdDefaultSID
                                       // 07: 6/3/24   Added eepAdCrtBootName (unreleased)
                                       // 08: 7/7/24   Separate Min Boot Indicator
                                       
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
   eepAdCrtBootName   = 3163, // (MaxPathLength=300) Indicates boot to minimal code and .crt path to launch
   eepAdMinBootInd    = 3463, // (1:uint8_t)    Indicates that Minimal boot should execute eepAdCrtBootName (!=0) or passthrough (=0)
   //Max size = 4284 (4k, emulated in flash)
};


