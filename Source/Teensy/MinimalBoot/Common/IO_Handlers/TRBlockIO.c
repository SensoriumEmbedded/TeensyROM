// SPDX-License-Identifier: MIT

// The TR block device (docs/TR_Block_Device.md) on the IO page a RAM expansion handler leaves free, beside
//  the USB mouse's registers. The ISR side only fills registers and the window; the SD card is touched
//  only from PollTRBlock, in the main loop.

#include "../../../BlockDevice/TRBlockImage.h"

#define TRBlock_FlushIdlemS  250

static struct TRBlock TRBlockDev;
static struct TRBlockImage TRBlockImg;
static struct TRBlockBackend TRBlockBack;
static File TRBlockFile;
static bool TRBlockDirty;
static uint32_t TRBlockLastWritemS;

static bool TRBlockFileRead(void *Context, uint32_t Offset, uint8_t *Block)
{
   return TRBlockFile.seek(Offset) && TRBlockFile.read(Block, GEOS_SECTOR_SIZE) == GEOS_SECTOR_SIZE;
}

static bool TRBlockFileWrite(void *Context, uint32_t Offset, const uint8_t *Block)
{
   if (!TRBlockFile.seek(Offset) || TRBlockFile.write(Block, GEOS_SECTOR_SIZE) != GEOS_SECTOR_SIZE) return false;
   TRBlockDirty = true;
   TRBlockLastWritemS = millis();
   return true;
}

static bool TRBlockFileFlush(void *Context)
{
   TRBlockFile.flush();
   TRBlockDirty = false;
   return true;
}

FLASHMEM void TRBlockUnmount()
{
   TRBlockBack.Context = NULL;
   if (TRBlockFile) TRBlockFile.close();
   TRBlockDirty = false;
}

// The image stays mounted across handler restarts, so a launcher can mount it and then start the handler
FLASHMEM bool TRBlockMount(const char *Path)
{
   uint64_t Size;

   TRBlockUnmount();
   TRBlockFile = SD.open(Path, FILE_WRITE_BEGIN);
   if (!TRBlockFile) return false;
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
