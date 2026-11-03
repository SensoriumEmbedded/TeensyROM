// SPDX-License-Identifier: MIT
// The TR menu's view of a D64, D71 or D81 (D64.ino): browsing an image and loading a file from it, read only,
// through the shared engine. The menu picks the kind from the file's extension.
#pragma once
#include <stdbool.h>

#include "DiskImage.h"

enum MenuDiskKind { MenuDiskD64, MenuDiskD71, MenuDiskD81 };

#define MENUDISK_BROKEN   (-1)   // a link off the image, a read that failed, or a chain that loops
#define MENUDISK_TOO_BIG  (-2)

// A file of the kind's size, with or without error bytes, or a short image some copiers write (Realm.d64 is
// 171520 bytes): a sector past its end fails to read rather than the image being refused
FLASHMEM static bool MenuDiskOpen(d64_t *Disk, enum MenuDiskKind Kind, uint64_t Size, d64_read_fn Read, void *Context)
{
   static const uint32_t Smallest[] D64_FLASH = { 171520, 349696, 819200 };
   static const uint32_t Largest[] D64_FLASH = { 197376, 351062, 822400 };
   static const uint8_t Tracks[] D64_FLASH = { 35, 70, 80 };

   if (Size < Smallest[Kind] || Size > Largest[Kind]) return false;
   d64_open(Disk, Read, Context, Kind == MenuDiskD64 && Size >= 196608 ? 40 : Tracks[Kind]);
   return true;
}

// The bytes of the file whose first sector is Track/Sector: its length, MENUDISK_BROKEN or MENUDISK_TOO_BIG
FLASHMEM static int32_t MenuDiskReadFile(d64_t *Disk, uint8_t Track, uint8_t Sector, uint8_t *Out, uint32_t Capacity)
{
   d64_entry_t Entry;
   d64_file_t File;
   int Read;

   memset(&Entry, 0, sizeof Entry);
   Entry.track = Track;
   Entry.sector = Sector;
   d64_file_open(Disk, &Entry, &File);
   Read = d64_file_read(Disk, &File, Out, Capacity);
   if (Read < 0) return MENUDISK_BROKEN;
   if (!d64_file_eof(&File)) return MENUDISK_TOO_BIG;
   return Read;
}
