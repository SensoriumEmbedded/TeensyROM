// SPDX-License-Identifier: MIT

// The TR block device (docs/TR_Block_Device.md) in IO2, the page GeoRAM leaves free, beside the USB
//  mouse's registers. The ISR side only fills registers and the window; the SD card is touched
//  only from the main loop: PollTRBlock from GeoRAM's polling handler, and SDFullInit.

#include "../../../BlockDevice/TRBlockImage.h"

#define TRBlock_FlushIdlemS  250

static struct TRBlock TRBlockDev;
static struct TRBlockImage TRBlockImg;
static struct TRBlockBackend TRBlockBack;
// An FsFile, not an SD.h File: File::flush() discards whether the sync reached the card
static FsFile TRBlockFile;
static bool TRBlockDirty;
static uint32_t TRBlockLastWritemS;
static char *TRBlockPath;
static struct { uint32_t FirstSector, CardSerial; } TRBlockMounted;

FLASHMEM static bool TRBlockFileRead(void *Context, uint32_t Offset, uint8_t *Block)
{
   return TRBlockFile.seekSet(Offset) && TRBlockFile.read(Block, GEOS_SECTOR_SIZE) == GEOS_SECTOR_SIZE;
}

FLASHMEM static bool TRBlockFileWrite(void *Context, uint32_t Offset, const uint8_t *Block)
{
   if (!TRBlockFile.seekSet(Offset) || TRBlockFile.write(Block, GEOS_SECTOR_SIZE) != GEOS_SECTOR_SIZE) return false;
   TRBlockDirty = true;
   TRBlockLastWritemS = millis();
   return true;
}

// GEOS never sends FLUSH, so a failed sync stays dirty for the idle flush to retry
FLASHMEM static bool TRBlockFileFlush(void *Context)
{
   static bool FailureLogged;

   if (TRBlockFile.sync())
   {
      TRBlockDirty = FailureLogged = false;
      return true;
   }
   TRBlockDirty = true;
   TRBlockLastWritemS = millis();
   if (!FailureLogged) Serial.printf("TR block image: writes are not reaching the card; retrying\n");
   FailureLogged = true;
   return false;
}

FLASHMEM static bool TRBlockOpen()
{
   uint64_t Size;

   if (!TRBlockFile.open(&SD.sdfs, TRBlockPath, O_RDWR)) return false;
   Size = TRBlockFile.size();
   TRBlockImg = (struct TRBlockImage){ Size <= UINT32_MAX ? GeosFormatForSize(Size) : NULL, true, NULL,
                                       TRBlockFileRead, TRBlockFileWrite, TRBlockFileFlush };
   if (!TRBlockImg.Format)
   {
      TRBlockFile.close();
      return false;
   }
   TRBlockBack = TRBlockImageBackend(&TRBlockImg);
   return true;
}

// Close and reopen around an SD.begin that can run while GeoRAM has an image mounted, as SDFullInit does.
//  SD.begin rebuilds the volume in place under any open file and drops its cached, unwritten sectors.
FLASHMEM void TRBlockClose()
{
   TRBlockBack.Context = NULL;
   if (TRBlockFile.isOpen() && !TRBlockFile.close()) Serial.printf("TR block image: last writes did not reach the card\n");
   TRBlockDirty = false;
}

FLASHMEM static uint32_t TRBlockCardSerial()
{
   cid_t CID;

   return SD.sdfs.card()->readCID(&CID) ? CID.psn : 0;
}

FLASHMEM static bool TRBlockIsMountedFile()
{
   return TRBlockFile.firstSector() == TRBlockMounted.FirstSector && TRBlockCardSerial() == TRBlockMounted.CardSerial;
}

// Every D64 is the same size, so a swapped card's image at the same path would take GEOS's cached BAM and directory
FLASHMEM void TRBlockReopen()
{
   if (!TRBlockPath || !TRBlockOpen() || TRBlockIsMountedFile()) return;
   TRBlockClose();
   Serial.printf("TR block image: %s on the card is not the one mounted; left unmounted\n", TRBlockPath);
}

FLASHMEM void TRBlockUnmount()
{
   TRBlockClose();
   free(TRBlockPath);
   TRBlockPath = NULL;
}

// The image stays mounted across handler restarts, so a launcher can mount it and then start the handler
FLASHMEM bool TRBlockMount(const char *Path)
{
   TRBlockUnmount();
   TRBlockPath = strdup(Path);
   if (TRBlockPath && TRBlockOpen())
   {
      TRBlockMounted = { TRBlockFile.firstSector(), TRBlockCardSerial() };
      return true;
   }
   TRBlockUnmount();
   return false;
}

FLASHMEM void InitTRBlock()
{
   TRBlockReset(&TRBlockDev);
}

FLASHMEM void PollTRBlock()
{
   TRBlockService(&TRBlockDev, &TRBlockBack);
   if (TRBlockDirty && millis() - TRBlockLastWritemS > TRBlock_FlushIdlemS) TRBlockFileFlush(NULL);
}

// One IO access to the page the mouse registers start: the mouse owns the offsets below the block device's.
// A read that reaches DataPortWriteWait late is latched wrong on some C64s, so the window is answered first.
__attribute__((always_inline)) inline void TRPageIO(uint8_t Address, bool R_Wn)
{
   uint8_t WindowOffset = Address - TRBlockWindow;

   if (R_Wn)
   {
      int Data;

      if (WindowOffset < TRBLOCK_WINDOW_SIZE)
      {
         DataPortWriteWaitLog(TRBlockDev.Window[WindowOffset]);
         return;
      }
      if (Address < TRBlockCmd)
      {
         USBMouseIORead(Address);
         return;
      }
      Data = TRBlockReadAt(&TRBlockDev, Address);
      if (Data != TRBLOCK_UNSERVED) DataPortWriteWaitLog(Data);
   }
   else if (TRBlockTakes(Address))
   {
      uint8_t Data = DataPortWaitRead();
      TRBlockWriteAt(&TRBlockDev, Address, Data);
      TraceLogAddValidData(Data);
   }
}
