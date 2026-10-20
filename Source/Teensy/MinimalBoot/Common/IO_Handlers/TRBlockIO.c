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

// One IO access to the page the mouse registers start: the mouse's and the block device's offsets do not overlap
void TRPageIO(uint8_t Address, bool R_Wn)
{
   if (R_Wn)
   {
      USBMouseIORead(Address);
      if (TRBlockServes(Address)) DataPortWriteWaitLog(TRBlockReadAt(&TRBlockDev, Address));
   }
   else if (TRBlockTakes(Address))
   {
      uint8_t Data = DataPortWaitRead();
      TRBlockWriteAt(&TRBlockDev, Address, Data);
      TraceLogAddValidData(Data);
   }
}
