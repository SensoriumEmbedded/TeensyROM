// SPDX-License-Identifier: MIT
// A CBM disk image (D64, D71, D81) as the block device's backend: the track and sector the driver
// sends become an offset in the image file.
#pragma once
#include "../GEOS/GeosDiskPatch.h"
#include "TRBlock.h"

#define TRBLOCK_IMAGE_WRITABLE 0x01

struct TRBlockImage
{
   const struct GeosFormat *Format;
   bool Writable;
   void *Context;
   bool (*Read)(void *Context, uint32_t Offset, uint8_t *Block);
   bool (*Write)(void *Context, uint32_t Offset, const uint8_t *Block);
   bool (*Flush)(void *Context);
};

static inline bool TRBlockImageHasSector(const struct GeosFormat *Format, uint8_t Track, uint8_t Sector)
{
   return Track >= 1 && Track <= Format->Tracks && Sector < GeosSectorsOnTrack(Format, Track);
}

static uint8_t TRBlockImageTransfer(void *Context, bool Write, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;
   uint32_t Offset;

   if (!Image->Format) return TRBlockNoImage;
   if (!TRBlockImageHasSector(Image->Format, Track, Sector)) return TRBlockBadAddress;
   Offset = GeosOffset(Image->Format, Track, Sector);
   if (!Write) return Image->Read(Image->Context, Offset, Block) ? TRBlockDone : TRBlockReadFailed;
   if (!Image->Writable) return TRBlockReadOnly;
   return Image->Write(Image->Context, Offset, Block) ? TRBlockDone : TRBlockWriteFailed;
}

static uint8_t TRBlockImageInfo(void *Context, uint8_t *Out)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;
   uint32_t Blocks;

   if (!Image->Format) return TRBlockNoImage;
   Blocks = Image->Format->Size / GEOS_SECTOR_SIZE;

   Out[0] = (uint8_t)(Image->Format - GeosFormats + 1);
   Out[1] = Image->Writable ? TRBLOCK_IMAGE_WRITABLE : 0;
   Out[2] = (uint8_t)Blocks;
   Out[3] = (uint8_t)(Blocks >> 8);
   Out[4] = (uint8_t)(Blocks >> 16);
   return TRBlockDone;
}

static uint8_t TRBlockImageFlush(void *Context)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;

   return Image->Flush(Image->Context) ? TRBlockDone : TRBlockWriteFailed;
}

static inline struct TRBlockBackend TRBlockImageBackend(struct TRBlockImage *Image)
{
   return (struct TRBlockBackend){ Image, TRBlockImageTransfer, TRBlockImageInfo, TRBlockImageFlush };
}
