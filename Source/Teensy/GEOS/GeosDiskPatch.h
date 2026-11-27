// SPDX-License-Identifier: MIT
// Puts a TR input driver on a GEOS boot disk image, through the shared disk image engine
// (docs/Architecture/Disk-Images.md). Mirrors tools/geos/trmouse.py install(), which the host test
// holds it to byte for byte.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef ARDUINO
#include <avr/pgmspace.h>
#else
#define FLASHMEM
#define PSTR(s) (s)
#endif
#include "../DiskImage/DiskImage.h"

#define GEOS_SECTOR_SIZE     D64_SECTOR
#define GEOS_DIR_ENTRY_SIZE  32
#define GEOS_NAME_SIZE       16
#define GEOS_ERROR_SIZE      80
#define GEOS_DATA_PER_BLOCK  (GEOS_SECTOR_SIZE - 2)

#define CBM_TYPE_MASK        0x07
#define CBM_USR              3
#define GEOS_SEQUENTIAL      0
#define D71_DOUBLE_SIDED     0x80

// Byte offsets inside a 32-byte directory slot (the first two hold the sector link)
enum GeosSlot
{
   SlotType      = 2,
   SlotFirst     = 3,
   SlotName      = 5,
   SlotInfo      = 21,
   SlotStructure = 23,
   SlotGeosType  = 24,
   SlotDate      = 25,
   SlotBlocks    = 30,
};

struct GeosDate { uint8_t Year, Month, Day, Hour, Minute; };

// A GEOS version the TR sets up: its boot disk's DeskTop and the input driver the TR adds
struct GeosTarget
{
   const char *DeskTopName, *DriverName;
   const uint8_t *DriverCode, *DriverInfo;
   uint16_t DriverSize;
   uint8_t InputType;
   const char *BootFile;   // what the TR starts the disk from
   uint16_t BootEntry;
   const struct GeosDiskDriver *DiskDriver;
   const struct GeosClockRun *ClockRuns;
   uint8_t ClockRunCount;
   uint16_t ResetKey;      // where the boot file holds the C128's reset key, or 0
   const char *ConfigureName;
};

struct GeosBootDisk { bool HasDeskTop, HasDriver; };

// A boot disk image reached through 256-byte block reads and writes at byte offsets. Set Target, Context
// and the callbacks (Write NULL for a read-only image), then GeosDiskOpen. The engine keeps a pointer to
// the struct, so it stays where it was opened.
struct GeosDisk
{
   d64_t Image;
   bool Open;
   const struct GeosTarget *Target;
   void *Context;
   bool (*Read)(void *Context, uint32_t Offset, uint8_t *Block);
   bool (*Write)(void *Context, uint32_t Offset, const uint8_t *Block);
   char Error[GEOS_ERROR_SIZE];
};

FLASHMEM static bool GeosFail(struct GeosDisk *Disk, const char *Message)
{
   snprintf(Disk->Error, sizeof Disk->Error, PSTR("%s"), Message);
   return false;
}

// For an engine call that failed: a card that failed has said so already, which is the better reason
FLASHMEM static bool GeosFailed(struct GeosDisk *Disk, const char *Message)
{
   if (!Disk->Error[0]) GeosFail(Disk, Message);
   return false;
}

FLASHMEM static int GeosSectorRead(void *Context, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   struct GeosDisk *Disk = (struct GeosDisk *)Context;

   if (Disk->Read(Disk->Context, (uint32_t)d64_sector_offset(&Disk->Image, Track, Sector), Block)) return 0;
   GeosFail(Disk, PSTR("the image could not be read"));
   return -1;
}

FLASHMEM static int GeosSectorWrite(void *Context, uint8_t Track, uint8_t Sector, const uint8_t *Block)
{
   struct GeosDisk *Disk = (struct GeosDisk *)Context;

   if (Disk->Write(Disk->Context, (uint32_t)d64_sector_offset(&Disk->Image, Track, Sector), Block)) return 0;
   GeosFail(Disk, PSTR("the image could not be written"));
   return -1;
}

FLASHMEM static bool GeosDiskOpen(struct GeosDisk *Disk, uint64_t Size)
{
   Disk->Error[0] = 0;
   Disk->Open = Size <= SIZE_MAX && !d64_open_exact(&Disk->Image, GeosSectorRead, Disk, (size_t)Size);
   if (Disk->Open && Disk->Write) Disk->Image.write = GeosSectorWrite;
   return Disk->Open;
}

FLASHMEM static uint8_t GeosDirTrack(const struct GeosDisk *Disk)
{
   return Disk->Image.fmt == D64_FMT_D81 ? 40 : 18;
}

// Everything that reads the directory checks the image first, so nothing follows a chain off the directory track
FLASHMEM static bool GeosCheckDisk(struct GeosDisk *Disk)
{
   const struct GeosTarget *Target = Disk->Target;
   uint8_t Header[GEOS_SECTOR_SIZE], Track, Sector;
   int Result;

   if (strlen(Target->DeskTopName) > GEOS_NAME_SIZE || strlen(Target->DriverName) > GEOS_NAME_SIZE || !Target->DriverSize)
   {
      return GeosFail(Disk, PSTR("the GEOS target's names or driver do not fit a directory entry"));
   }
   if (!Disk->Open) return GeosFail(Disk, PSTR("not a D64, D71 or D81 image"));
   if (d64_block_read(&Disk->Image, GeosDirTrack(Disk), 0, Header)) return GeosFailed(Disk, PSTR("the image could not be read"));
   if (Disk->Image.fmt == D64_FMT_D71 && !(Header[3] & D71_DOUBLE_SIDED))
   {
      return GeosFail(Disk, PSTR("this D71 is formatted single-sided"));
   }
   Result = d64_dir_check(&Disk->Image, &Track, &Sector);
   if (Result == D64_E_NOTFOUND) return GeosFail(Disk, PSTR("the disk header links to no directory"));
   if (Result == D64_E_LOOP)
   {
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("the directory chain is broken at %u/%u"), Track, Sector);
      return false;
   }
   if (Result) return GeosFailed(Disk, PSTR("the image could not be read"));
   return true;
}

// Whether a file is on the disk, as DOS finds it by name
FLASHMEM static bool GeosHasFile(struct GeosDisk *Disk, const char *Name, bool *Found)
{
   d64_entry_t Entry;
   int Result = d64_find(&Disk->Image, Name, &Entry);

   *Found = Result == 1;
   return Result >= 0 || GeosFailed(Disk, PSTR("the image could not be read"));
}

// Checks the image and says whether it is the target's boot disk and already holds the TR driver.
FLASHMEM static inline bool GeosInspect(struct GeosDisk *Disk, struct GeosBootDisk *Found)
{
   return GeosCheckDisk(Disk) && GeosHasFile(Disk, Disk->Target->DeskTopName, &Found->HasDeskTop) &&
          GeosHasFile(Disk, Disk->Target->DriverName, &Found->HasDriver);
}

FLASHMEM static uint16_t GeosDriverDataBlocks(const struct GeosTarget *Target)
{
   return (Target->DriverSize + GEOS_DATA_PER_BLOCK - 1) / GEOS_DATA_PER_BLOCK;
}

// The data chain and the info block
FLASHMEM static uint16_t GeosDriverBlocks(const struct GeosTarget *Target)
{
   return GeosDriverDataBlocks(Target) + 1;
}

// Block i of the driver's data chain, linked to the next unless it is the last; returns how many of its bytes are used
FLASHMEM static uint16_t GeosDriverBlock(const struct GeosTarget *Target, uint16_t i, uint8_t NextTrack, uint8_t NextSector,
                                         uint8_t *Block)
{
   uint16_t Start = i * GEOS_DATA_PER_BLOCK, Length = Target->DriverSize - Start;
   bool Last = i + 1 == GeosDriverDataBlocks(Target);

   if (Length > GEOS_DATA_PER_BLOCK) Length = GEOS_DATA_PER_BLOCK;
   memset(Block, 0, GEOS_SECTOR_SIZE);
   Block[0] = Last ? 0 : NextTrack;
   Block[1] = Last ? Length + 1 : NextSector;
   memcpy(Block + 2, Target->DriverCode + Start, Length);
   return Length + 2;
}

// Refuses, before anything is written, a disk whose directory or free sectors would run out partway
FLASHMEM static bool GeosCheckRoom(struct GeosDisk *Disk)
{
   uint8_t Track, Sector;
   int Result = d64_dir_room(&Disk->Image, &Track, &Sector);

   if (Result == D64_E_FULL) return GeosFail(Disk, PSTR("the directory is full"));
   if (Result == D64_E_BAM)
   {
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("the BAM shows directory sector %u/%u free"), Track, Sector);
      return false;
   }
   if (Result < 0) return GeosFailed(Disk, PSTR("the image could not be read"));

   Result = d64_blocks_allocatable(&Disk->Image);
   if (Result < 0) return GeosFailed(Disk, PSTR("the image could not be read"));
   if (Result < GeosDriverBlocks(Disk->Target)) return GeosFail(Disk, PSTR("the disk is full"));
   return true;
}

// The driver as DOS saves a file, then its info block in the sector a longer file would have taken next
FLASHMEM static bool GeosWriteDriver(struct GeosDisk *Disk, d64_wfile_t *File, uint8_t *InfoTrack, uint8_t *InfoSector)
{
   const struct GeosTarget *Target = Disk->Target;

   if (d64_create(&Disk->Image, Target->DriverName, CBM_USR, 0, File) ||
       d64_write(&Disk->Image, File, Target->DriverCode, Target->DriverSize) != Target->DriverSize ||
       d64_close_write(&Disk->Image, File))
   {
      return GeosFailed(Disk, PSTR("the image could not be written"));
   }
   *InfoTrack = File->cur_t;
   *InfoSector = File->cur_s;
   if (d64_alloc_next(&Disk->Image, InfoTrack, InfoSector) ||
       d64_block_write(&Disk->Image, *InfoTrack, *InfoSector, Target->DriverInfo))
   {
      return GeosFailed(Disk, PSTR("the image could not be written"));
   }
   return true;
}

// What makes the driver's entry a GEOS file: its info block, structure, type and date, and the info block in the count
FLASHMEM static bool GeosMakeGeosEntry(struct GeosDisk *Disk, const d64_wfile_t *File, uint8_t InfoTrack, uint8_t InfoSector,
                                       const struct GeosDate *Date)
{
   uint8_t Block[GEOS_SECTOR_SIZE], *Slot = Block + File->dir_slot * GEOS_DIR_ENTRY_SIZE;
   uint16_t Blocks = File->blocks + 1;

   if (d64_block_read(&Disk->Image, File->dir_t, File->dir_s, Block)) return GeosFailed(Disk, PSTR("the image could not be read"));
   Slot[SlotInfo] = InfoTrack;
   Slot[SlotInfo + 1] = InfoSector;
   Slot[SlotStructure] = GEOS_SEQUENTIAL;
   Slot[SlotGeosType] = Disk->Target->InputType;
   Slot[SlotDate] = Date->Year % 100;
   Slot[SlotDate + 1] = Date->Month;
   Slot[SlotDate + 2] = Date->Day;
   Slot[SlotDate + 3] = Date->Hour;
   Slot[SlotDate + 4] = Date->Minute;
   Slot[SlotBlocks] = Blocks & 0xff;
   Slot[SlotBlocks + 1] = Blocks >> 8;
   if (d64_block_write(&Disk->Image, File->dir_t, File->dir_s, Block)) return GeosFailed(Disk, PSTR("the image could not be written"));
   return true;
}

FLASHMEM static bool GeosSwapEntries(struct GeosDisk *Disk, const d64_entry_t *A, const d64_wfile_t *B)
{
   uint8_t BlockA[GEOS_SECTOR_SIZE], BlockB[GEOS_SECTOR_SIZE], Saved[GEOS_DIR_ENTRY_SIZE];
   bool Shared = A->dir_t == B->dir_t && A->dir_s == B->dir_s;
   uint8_t *SlotA = BlockA + A->dir_slot * GEOS_DIR_ENTRY_SIZE + SlotType;
   uint8_t *SlotB = (Shared ? BlockA : BlockB) + B->dir_slot * GEOS_DIR_ENTRY_SIZE + SlotType;
   const size_t EntryBytes = GEOS_DIR_ENTRY_SIZE - SlotType;

   if (d64_block_read(&Disk->Image, A->dir_t, A->dir_s, BlockA)) return GeosFailed(Disk, PSTR("the image could not be read"));
   if (!Shared && d64_block_read(&Disk->Image, B->dir_t, B->dir_s, BlockB)) return GeosFailed(Disk, PSTR("the image could not be read"));
   memcpy(Saved, SlotA, EntryBytes);
   memcpy(SlotA, SlotB, EntryBytes);
   memcpy(SlotB, Saved, EntryBytes);
   if (!Shared && d64_block_write(&Disk->Image, B->dir_t, B->dir_s, BlockB)) return GeosFailed(Disk, PSTR("the image could not be written"));
   if (d64_block_write(&Disk->Image, A->dir_t, A->dir_s, BlockA)) return GeosFailed(Disk, PSTR("the image could not be written"));
   return true;
}

// GEOS 128 boots with the first input driver in the directory, so the new one trades places with an earlier one
FLASHMEM static bool GeosMoveFirst(struct GeosDisk *Disk, const d64_wfile_t *File)
{
   d64_dir_t Dir;
   d64_entry_t Entry;
   int Result;

   if (d64_dir_first(&Disk->Image, &Dir)) return GeosFailed(Disk, PSTR("the image could not be read"));
   while ((Result = d64_dir_next(&Disk->Image, &Dir, &Entry)) > 0)
   {
      const uint8_t *Slot = Dir.buf + Entry.dir_slot * GEOS_DIR_ENTRY_SIZE;
      if (!(Slot[SlotType] & CBM_TYPE_MASK) || Slot[SlotGeosType] != Disk->Target->InputType) continue;
      if (Entry.dir_t == File->dir_t && Entry.dir_s == File->dir_s && Entry.dir_slot == File->dir_slot) return true;
      return GeosSwapEntries(Disk, &Entry, File);
   }
   return Result == 0 || GeosFailed(Disk, PSTR("the image could not be read"));
}

// Every refusal but a failed block read or write comes before the first write, leaving the image as it was.
FLASHMEM static inline bool GeosInstallDriver(struct GeosDisk *Disk, const struct GeosDate *Date)
{
   d64_wfile_t File;
   uint8_t InfoTrack, InfoSector;
   bool Present;

   if (!GeosCheckDisk(Disk) || !GeosHasFile(Disk, Disk->Target->DriverName, &Present)) return false;
   if (Present)
   {
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("%s is already on this disk"), Disk->Target->DriverName);
      return false;
   }
   if (!GeosCheckRoom(Disk)) return false;
   return GeosWriteDriver(Disk, &File, &InfoTrack, &InfoSector) && GeosMakeGeosEntry(Disk, &File, InfoTrack, InfoSector, Date) &&
          GeosMoveFirst(Disk, &File);
}

FLASHMEM static bool GeosOnDisk(const struct GeosDisk *Disk, uint8_t Track, uint8_t Sector)
{
   return d64_sector_offset(&Disk->Image, Track, Sector) >= 0;
}

// Makes the copy's TR mouse driver the firmware's, in place: a copy outlives the firmware that set it up, and an older
// driver may not find the registers where this one puts them. One of another length is refused, the image unchanged.
FLASHMEM static inline bool GeosRefreshDriver(struct GeosDisk *Disk, bool *Changed)
{
   const struct GeosTarget *Target = Disk->Target;
   const uint16_t DataBlocks = GeosDriverDataBlocks(Target);
   uint8_t Block[GEOS_SECTOR_SIZE], Wanted[GEOS_SECTOR_SIZE], Track, Sector;
   d64_entry_t Entry;
   uint16_t Blocks = 0;
   int Found;

   *Changed = false;
   if (!GeosCheckDisk(Disk)) return false;
   Found = d64_find(&Disk->Image, Target->DriverName, &Entry);
   if (Found < 0) return GeosFailed(Disk, PSTR("the image could not be read"));
   if (!Found) return true;
   for (Track = Entry.track, Sector = Entry.sector; Track && GeosOnDisk(Disk, Track, Sector) && Blocks <= DataBlocks; Blocks++)
   {
      if (d64_block_read(&Disk->Image, Track, Sector, Block)) return GeosFailed(Disk, PSTR("the image could not be read"));
      Track = Block[0];
      Sector = Block[1];
   }
   if (Track || Blocks != DataBlocks || !GeosOnDisk(Disk, Entry.ss_t, Entry.ss_s))
   {
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("its %s is an old one; delete this copy"), Target->DriverName);
      return false;
   }

   Track = Entry.track;
   Sector = Entry.sector;
   for (uint16_t i = 0; Track; i++)
   {
      uint16_t Used;
      if (d64_block_read(&Disk->Image, Track, Sector, Block)) return GeosFailed(Disk, PSTR("the image could not be read"));
      Used = GeosDriverBlock(Target, i, Block[0], Block[1], Wanted);
      if (memcmp(Block, Wanted, Used))
      {
         if (d64_block_write(&Disk->Image, Track, Sector, Wanted)) return GeosFailed(Disk, PSTR("the image could not be written"));
         *Changed = true;
      }
      Track = Block[0];
      Sector = Block[1];
   }
   if (d64_block_read(&Disk->Image, Entry.ss_t, Entry.ss_s, Block)) return GeosFailed(Disk, PSTR("the image could not be read"));
   if (!memcmp(Block, Target->DriverInfo, GEOS_SECTOR_SIZE)) return true;
   *Changed = true;
   if (d64_block_write(&Disk->Image, Entry.ss_t, Entry.ss_s, Target->DriverInfo)) return GeosFailed(Disk, PSTR("the image could not be written"));
   return true;
}
