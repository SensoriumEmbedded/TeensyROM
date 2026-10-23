// SPDX-License-Identifier: MIT

// Berkeley Softworks geoRAM: a 256-byte window at $DE00 onto the page selected by two
//  write-only registers, $DFFE (page 0-63 within a 16K block) and $DFFF (16K block).
//  No DMA, so it runs on every TR, in C64 and C128 mode.
// ref: VICE src/c64/cart/georam.c

void IO1Hndlr_GeoRAM(uint8_t Address, bool R_Wn);
void IO2Hndlr_GeoRAM(uint8_t Address, bool R_Wn);
void InitHndlr_GeoRAM();
void PollingHndlr_GeoRAM();
extern void (*fSpecialBtnChange)(bool Up_nDn);

stcIOHandlers IOHndlr_GeoRAM =
{
  "GeoRAM",            //Name of handler
  &InitHndlr_GeoRAM,   //Called once at handler startup
  &IO1Hndlr_GeoRAM,    //IO1 R/W handler
  &IO2Hndlr_GeoRAM,    //IO2 R/W handler
  NULL,                //ROML Read handler, in addition to any ROM data sent
  NULL,                //ROMH Read handler, in addition to any ROM data sent
  &PollingHndlr_GeoRAM, //Polled in main routine
  NULL,                //called at the end of EVERY c64 cycle
};

#define GeoRAM_Filename      "SD:/georam.ram"
#define MAX_GEORAM_BANKS     64   // 512K, the original geoRAM
#define MIN_GEORAM_BANKS     16   // 128K
#define GeoRAM_PagesPerBlock 64
#define GeoRAM_BanksPerBlock (16384 / RAM_Bank_Size)

// VICE decodes the registers across $DF80-$DFFF (A0 picks which); $DF00-$DF7F is the USB mouse's.
#define GeoRAM_RegsBase      0x80
#define GeoRAM_RegBlockBit   0x01

// volatile: the IO ISRs read the page only while there are banks, so the two stores
//  must reach memory in the order written
static volatile uint8_t NumGeoRAM_Banks = 0;
static uint8_t *GeoRAM_Bank[MAX_GEORAM_BANKS];
static uint8_t GeoRAM_Block, GeoRAM_PageNum;
static uint8_t * volatile GeoRAM_Page;

static void GeoRAM_SelectPage()
{
   if (NumGeoRAM_Banks == 0) return;
   // the unused high register bits wrap, as on real hardware
   uint32_t Offset = ((GeoRAM_Block & (NumGeoRAM_Banks / GeoRAM_BanksPerBlock - 1)) * GeoRAM_PagesPerBlock
                      + (GeoRAM_PageNum & (GeoRAM_PagesPerBlock - 1))) * 256;
   GeoRAM_Page = GeoRAM_Bank[Offset / RAM_Bank_Size] + (Offset % RAM_Bank_Size);
}

FLASHMEM void FreeGeoRAM()
{
   // the handler is still installed and the C64 may be running: the IO ISRs must stop
   //  touching the banks before the first one is freed
   uint8_t NumBanks = NumGeoRAM_Banks;
   NumGeoRAM_Banks = 0;
   FreeRAMBanks(GeoRAM_Bank, &NumBanks);
}

FLASHMEM void SpecialBtn_GeoRAM(bool Up_nDn)
{
   // on button release save GeoRAM contents
   if(Up_nDn) SaveRAMBanksReplacing(GeoRAM_Bank, NumGeoRAM_Banks, GeoRAM_Filename);
}

FLASHMEM void InitHndlr_GeoRAM()
{
   FreeGeoRAM();
   GeoRAM_Block = GeoRAM_PageNum = 0;
   InitUSBMouse();
   uint8_t NumBanks = AllocRAMBanks(GeoRAM_Bank, MAX_GEORAM_BANKS, MIN_GEORAM_BANKS);
   if (NumBanks == 0)
   {
      Serial.printf("GeoRAM is disabled\n");
      return;
   }
   GeoRAM_Page = GeoRAM_Bank[0];
   NumGeoRAM_Banks = NumBanks;
   fSpecialBtnChange = &SpecialBtn_GeoRAM;
   Serial.printf("GeoRAM Size: %dK bytes\n", NumBanks * RAM_Bank_Size / 1024);
   LoadRAMBanks(GeoRAM_Bank, NumBanks, GeoRAM_Filename);
}

FLASHMEM void PollingHndlr_GeoRAM()
{
   PollUSBMouse();
}

void IO1Hndlr_GeoRAM(uint8_t Address, bool R_Wn)
{
   if (NumGeoRAM_Banks == 0) return;   // disabled: $DE00 stays open bus, so software sees no geoRAM
   if (R_Wn) DataPortWriteWaitLog(GeoRAM_Page[Address]);
   else
   {
      uint8_t Data = DataPortWaitRead();
      GeoRAM_Page[Address] = Data;
      TraceLogAddValidData(Data);
   }
}

void IO2Hndlr_GeoRAM(uint8_t Address, bool R_Wn)
{
   if (Address < GeoRAM_RegsBase)
   {
      if (R_Wn) USBMouseIORead(Address);
      return;
   }
   if (R_Wn) return;   // write-only registers
   uint8_t Data = DataPortWaitRead();
   if (Address & GeoRAM_RegBlockBit) GeoRAM_Block = Data;
   else GeoRAM_PageNum = Data;
   GeoRAM_SelectPage();
   TraceLogAddValidData(Data);
}
