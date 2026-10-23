// SPDX-License-Identifier: MIT

// The TR block device (docs/TR_Block_Device.md) on the IO page a RAM expansion handler leaves free, beside
//  the USB mouse's registers. The ISR side only fills registers and the window; the SD card is touched
//  only from the main loop: PollTRBlock from GeoRAM's polling handler, and SDBegin.

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

static bool TRBlockFileRead(void *Context, uint32_t Offset, uint8_t *Block)
{
   return TRBlockFile.seekSet(Offset) && TRBlockFile.read(Block, GEOS_SECTOR_SIZE) == GEOS_SECTOR_SIZE;
}

static bool TRBlockFileWrite(void *Context, uint32_t Offset, const uint8_t *Block)
{
   if (!TRBlockFile.seekSet(Offset) || TRBlockFile.write(Block, GEOS_SECTOR_SIZE) != GEOS_SECTOR_SIZE) return false;
   TRBlockDirty = true;
   TRBlockLastWritemS = millis();
   return true;
}

static bool TRBlockFileFlush(void *Context)
{
   TRBlockDirty = false;
   return TRBlockFile.sync();
}

static FLASHMEM bool TRBlockOpen()
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

static FLASHMEM void TRBlockClose()
{
   TRBlockBack.Context = NULL;
   if (TRBlockFile.isOpen() && !TRBlockFile.close()) Serial.printf("TR block image: last writes did not reach the card\n");
   TRBlockDirty = false;
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
   if (TRBlockPath && TRBlockOpen()) return true;
   TRBlockUnmount();
   return false;
}

// Every SD.begin in the main image goes through here. SD.begin rebuilds the volume in place under
//  any open file and drops its cached, unwritten sectors, so the image is closed around it.
FLASHMEM bool SDBegin()
{
   bool Began;

   TRBlockClose();
   Began = SD.begin(BUILTIN_SDCARD);
   if (TRBlockPath) TRBlockOpen();
   return Began;
}

FLASHMEM void InitTRBlock()
{
   TRBlockReset(&TRBlockDev);
}

void PollTRBlock()
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
