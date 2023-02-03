
#define MaxMenuItems  254
#define MaxItemNameLength 25

uint8_t RAM_Image[65536];  //For receiving files from USB/SD/etc, should do this dynamically...

enum IO1_Registers  //offset from 0xDE00, needs to match C64 code
{
   rRegStatus,    //Busy when doing SD/USB access.  note: loc 0(DE00) gets written to at reset
   rRegStrAddrLo, //start address of the prg file being transfered to mem
   rRegStrAddrHi, //zero when inactive/complete (no transfer to zero page)
   rRegStreamData,//next byte of data to transfer, auto increments when read
   wRegControl,   //RegCtlCommands: execute specific functions
   rRegPresence1, //for HW detect: 0x55
   rRegPresence2, //for HW detect: 0xAA
   rwRegCurrMenu, //RegMenuTypes: select Menu type: SD, USB, etc
   rwRegSelItem,  //select Menu Item for name, type, execution, etc
   rRegNumItems,  //num items in menu list
   rRegItemType,  //regItemTypes: type of item 
   rRegItemName,  //MaxItemNameLength in length (incl term)
};

enum RegStatusTypes
{
   rstBusy,
   rstReady,
   rmtError,
};

enum RegMenuTypes
{
   rmtSD,
   rmtTeensy,
   rmtUSBHost,
   rmtUSBDrive,
};

enum RegCtlCommands
{
   RCtlVanish = 0,
   RCtlVanishReset,
   RCtlStartRom ,
   RCtlLoadFromSD ,
   RCtlLoadFromUSB ,
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

