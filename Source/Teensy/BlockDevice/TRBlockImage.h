// SPDX-License-Identifier: MIT
// A CBM disk image (D64, D71, D81) as the block device's backend: the track and sector the driver
// sends become an offset in the image file, by the shared engine's geometry (docs/Architecture/Disk-Images.md).
#pragma once
#include "../DiskImage/DiskImage.h"
#include "TRBlock.h"

#define TRBLOCK_IMAGE_WRITABLE 0x01

// Set Writable, Context and the callbacks, then TRBlockImageOpen. The engine keeps a pointer to the
// struct, so it stays where it was opened.
struct TRBlockImage
{
   d64_t Disk;
   bool Open, Writable;
   void *Context;
   bool (*Read)(void *Context, uint32_t Offset, uint8_t *Block);
   bool (*Write)(void *Context, uint32_t Offset, const uint8_t *Block);
   bool (*Flush)(void *Context);
};

FLASHMEM static int TRBlockImageSectorRead(void *Context, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;
   return Image->Read(Image->Context, (uint32_t)d64_sector_offset(&Image->Disk, Track, Sector), Block) ? 0 : -1;
}

FLASHMEM static int TRBlockImageSectorWrite(void *Context, uint8_t Track, uint8_t Sector, const uint8_t *Block)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;
   return Image->Write(Image->Context, (uint32_t)d64_sector_offset(&Image->Disk, Track, Sector), Block) ? 0 : -1;
}

// An error table (one byte per sector after the data) is left unread: a GEOS driver has no status for a
// sector marked bad, so it gets the sector's bytes, as a drive that read it anyway would give them.
FLASHMEM static bool TRBlockImageOpen(struct TRBlockImage *Image, uint64_t Size)
{
   Image->Open = Size <= SIZE_MAX && !d64_open_exact(&Image->Disk, TRBlockImageSectorRead, Image, (size_t)Size);
   if (Image->Open && Image->Writable) Image->Disk.write = TRBlockImageSectorWrite;
   return Image->Open;
}

FLASHMEM static uint8_t TRBlockImageTransfer(void *Context, bool Write, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;

   if (!Image->Open) return TRBlockNoImage;
   if (d64_sector_offset(&Image->Disk, Track, Sector) < 0) return TRBlockBadAddress;
   if (!Write) return d64_block_read(&Image->Disk, Track, Sector, Block) ? TRBlockReadFailed : TRBlockDone;
   if (!Image->Writable) return TRBlockReadOnly;
   return d64_block_write(&Image->Disk, Track, Sector, Block) ? TRBlockWriteFailed : TRBlockDone;
}

FLASHMEM static uint8_t TRBlockImageInfo(void *Context, uint8_t *Out)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;
   uint32_t Blocks;

   if (!Image->Open) return TRBlockNoImage;
   Blocks = (uint32_t)d64_sectors(&Image->Disk);

   Out[0] = (uint8_t)(Image->Disk.fmt + 1);
   Out[1] = Image->Writable ? TRBLOCK_IMAGE_WRITABLE : 0;
   Out[2] = (uint8_t)Blocks;
   Out[3] = (uint8_t)(Blocks >> 8);
   Out[4] = (uint8_t)(Blocks >> 16);
   return TRBlockDone;
}

FLASHMEM static uint8_t TRBlockImageFlush(void *Context)
{
   struct TRBlockImage *Image = (struct TRBlockImage *)Context;

   if (!Image->Open) return TRBlockNoImage;
   return Image->Flush(Image->Context) ? TRBlockDone : TRBlockWriteFailed;
}

static inline struct TRBlockBackend TRBlockImageBackend(struct TRBlockImage *Image)
{
   return (struct TRBlockBackend){ Image, TRBlockImageTransfer, TRBlockImageInfo, TRBlockImageFlush, NULL, 0 };
}
