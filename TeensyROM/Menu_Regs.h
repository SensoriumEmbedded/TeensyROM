
#define MaxMenuItems  254
#define MaxItemNameLength 28

uint8_t RAM_Image[65536];  //For receiving files from USB/SD/etc, should do this dynamically...

enum IO1_Registers  //offset from 0xDE00, needs to match C64 code
{
   rRegStatus,        //Busy when doing SD/USB access.  note: loc 0(DE00) gets written to at reset
   rRegStrAddrLo,     //start address of the prg file being transfered to mem
   rRegStrAddrHi,     //zero when inactive/complete (no transfer to zero page)
   rRegStreamData,    //next byte of data to transfer, auto increments when read
   wRegControl,       //RegCtlCommands: execute specific functions
   rRegPresence1,     //for HW detect: 0x55
   rRegPresence2,     //for HW detect: 0xAA
   rRegLastHourBCD,   //Last TOD Hours read
   rRegLastMinBCD,    //Last TOD Minutes read
   rRegLastSecBCD,    //Last TOD Seconds read
   rWRegCurrMenuWAIT, //RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItem,      //select Menu Item for name, type, execution, etc
   rRegNumItems,      //num items in menu list
   rRegItemType,      //regItemTypes: type of item 
   rRegItemName,      //MaxItemNameLength bytes long (incl term)
};

enum RegStatusTypes
{
   rsReady      = 0x5a,
   rsChangeMenu = 0x9d,
   rsStartItem  = 0xb1,
   rsGetTime    = 0xe6,
   //rsError      = 0x24,
};

enum RegMenuTypes
{
   rmtSD        = 0,
   rmtTeensy    = 1,
   rmtUSBHost   = 2,
   rmtUSBDrive  = 3,
};

enum RegCtlCommands
{
   rCtlVanish           = 0,
   rCtlVanishReset      = 1,
   rCtlStartSelItemWAIT = 2,
   rCtlGetTimeWAIT      = 3,
};

enum regItemTypes
{
   rtNone = 0,
   rt16k  = 1,
   rt8kHi = 2,
   rt8kLo = 3,
   rtPrg  = 4,
   rtUnk  = 5,
   rtCrt  = 6,
   rtDir  = 7,
};

struct StructMenuItem
{
  unsigned char ItemType;
  char Name[MaxItemNameLength];
  const unsigned char *Code_Image;
  uint16_t Size;
};

