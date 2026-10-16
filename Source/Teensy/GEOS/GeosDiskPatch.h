// SPDX-License-Identifier: MIT
// Puts the TR USB mouse driver on a GEOS 128 boot disk image, block by block.
// Mirrors tools/geos/trmouse.py install(), which the host test holds it to byte for byte.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "TRMouse128.h"

#define GEOS_SECTOR_SIZE     256
#define GEOS_DIR_ENTRY_SIZE  32
#define GEOS_ENTRIES_PER_DIR (GEOS_SECTOR_SIZE / GEOS_DIR_ENTRY_SIZE)
#define GEOS_NAME_SIZE       16
#define GEOS_MAX_DIR_SECTORS 40
#define GEOS_ERROR_SIZE      80

#define CBM_NAME_PAD         0xa0
#define CBM_TYPE_MASK        0x07
#define CBM_USR_CLOSED       0x83
#define GEOS_SEQUENTIAL      0
#define GEOS_INPUT_128       15
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

struct GeosPlace { uint8_t Track, Sector, Byte; };
struct GeosTS    { uint8_t Track, Sector; };

struct GeosFormat
{
   const char *Name;
   uint8_t Tracks, DirTrack, FirstDirSector, LastDirSector;
   uint8_t Reserved[2];
   uint32_t Size;
   bool FixedSectors;   // D81: 40 per track; else the 1541's zones, repeated on a 1571's second side
};

struct GeosDate { uint8_t Year, Month, Day, Hour, Minute; };

// A boot disk image reached through 256-byte block reads and writes at byte offsets.
struct GeosDisk
{
   const struct GeosFormat *Format;
   void *Context;
   bool (*Read)(void *Context, uint32_t Offset, uint8_t *Block);
   bool (*Write)(void *Context, uint32_t Offset, const uint8_t *Block);
   char Error[GEOS_ERROR_SIZE];
};

static const struct GeosFormat GeosFormats[] =
{
   { "D64", 35, 18, 1, 18, { 18,  0 }, 174848, false },
   { "D71", 70, 18, 1, 18, { 18, 53 }, 349696, false },
   { "D81", 80, 40, 3, 39, { 40,  0 }, 819200, true  },
};

static bool GeosFail(struct GeosDisk *Disk, const char *Message)
{
   snprintf(Disk->Error, sizeof Disk->Error, "%s", Message);
   return false;
}

static uint8_t GeosSectorsOnTrack(const struct GeosFormat *Format, uint8_t Track)
{
   uint8_t ZoneTrack = (Track - 1) % 35 + 1;

   if (Format->FixedSectors) return 40;
   return ZoneTrack <= 17 ? 21 : ZoneTrack <= 24 ? 19 : ZoneTrack <= 30 ? 18 : 17;
}

static uint32_t GeosOffset(const struct GeosFormat *Format, uint8_t Track, uint8_t Sector)
{
   uint32_t Blocks = Sector;

   for (uint8_t t = 1; t < Track; t++) Blocks += GeosSectorsOnTrack(Format, t);
   return Blocks * GEOS_SECTOR_SIZE;
}

static bool GeosReadTS(struct GeosDisk *Disk, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   if (Disk->Read(Disk->Context, GeosOffset(Disk->Format, Track, Sector), Block)) return true;
   return GeosFail(Disk, "the image could not be read");
}

static bool GeosWriteTS(struct GeosDisk *Disk, uint8_t Track, uint8_t Sector, const uint8_t *Block)
{
   if (Disk->Write(Disk->Context, GeosOffset(Disk->Format, Track, Sector), Block)) return true;
   return GeosFail(Disk, "the image could not be written");
}

static bool GeosIsReserved(const struct GeosFormat *Format, uint8_t Track)
{
   return Track == Format->Reserved[0] || Track == Format->Reserved[1];
}

static bool GeosIsDirSector(const struct GeosFormat *Format, uint8_t Track, uint8_t Sector)
{
   return Track == Format->DirTrack && Sector >= Format->FirstDirSector && Sector <= Format->LastDirSector;
}

// Where a track's free count and free-sector bitmap live
static void GeosBamPlaces(const struct GeosFormat *Format, uint8_t Track, struct GeosPlace *Count, struct GeosPlace *Bitmap)
{
   if (Format->FixedSectors)
   {
      uint8_t Sector = 1 + (Track - 1) / 40, Entry = 0x10 + (Track - 1) % 40 * 6;
      *Count  = (struct GeosPlace){ 40, Sector, Entry };
      *Bitmap = (struct GeosPlace){ 40, Sector, (uint8_t)(Entry + 1) };
   }
   else if (Track <= 35)
   {
      *Count  = (struct GeosPlace){ 18, 0, (uint8_t)(4 + (Track - 1) * 4) };
      *Bitmap = (struct GeosPlace){ 18, 0, (uint8_t)(5 + (Track - 1) * 4) };
   }
   else
   {
      *Count  = (struct GeosPlace){ 18, 0, (uint8_t)(0xdd + Track - 36) };
      *Bitmap = (struct GeosPlace){ 53, 0, (uint8_t)((Track - 36) * 3) };
   }
}

static bool GeosSectorIsFree(const uint8_t *Bitmap, uint8_t Sector)
{
   return Bitmap[Sector / 8] & (1 << (Sector % 8));
}

static uint8_t GeosCountFree(const struct GeosFormat *Format, uint8_t Track, const uint8_t *Bitmap, int *FirstFree)
{
   uint8_t Free = 0;

   *FirstFree = -1;
   for (uint8_t s = 0; s < GeosSectorsOnTrack(Format, Track); s++)
   {
      if (!GeosSectorIsFree(Bitmap, s)) continue;
      if (*FirstFree < 0) *FirstFree = s;
      Free++;
   }
   return Free;
}

// Marks the first free sector of a track in use; *Sector is -1 when the track has none.
static bool GeosTake(struct GeosDisk *Disk, uint8_t Track, int *Sector)
{
   struct GeosPlace Count, Bitmap;
   uint8_t CountBlock[GEOS_SECTOR_SIZE], BitmapBlock[GEOS_SECTOR_SIZE];
   bool Shared;

   GeosBamPlaces(Disk->Format, Track, &Count, &Bitmap);
   Shared = Count.Track == Bitmap.Track && Count.Sector == Bitmap.Sector;
   if (!GeosReadTS(Disk, Count.Track, Count.Sector, CountBlock)) return false;
   if (!Shared && !GeosReadTS(Disk, Bitmap.Track, Bitmap.Sector, BitmapBlock)) return false;
   uint8_t *Bits = (Shared ? CountBlock : BitmapBlock) + Bitmap.Byte;

   uint8_t Free = GeosCountFree(Disk->Format, Track, Bits, Sector);
   if (CountBlock[Count.Byte] < Free)
   {
      snprintf(Disk->Error, sizeof Disk->Error, "the BAM count for track %u disagrees with its bitmap", Track);
      return false;
   }
   if (*Sector < 0) return true;

   Bits[*Sector / 8] &= ~(1 << (*Sector % 8));
   CountBlock[Count.Byte]--;
   if (!Shared && !GeosWriteTS(Disk, Bitmap.Track, Bitmap.Sector, BitmapBlock)) return false;
   return GeosWriteTS(Disk, Count.Track, Count.Sector, CountBlock);
}

static bool GeosAllocate(struct GeosDisk *Disk, struct GeosTS *Where)
{
   for (uint8_t Track = 1; Track <= Disk->Format->Tracks; Track++)
   {
      int Sector;
      if (GeosIsReserved(Disk->Format, Track)) continue;
      if (!GeosTake(Disk, Track, &Sector)) return false;
      if (Sector < 0) continue;
      *Where = (struct GeosTS){ Track, (uint8_t)Sector };
      return true;
   }
   return GeosFail(Disk, "the disk is full");
}

static bool GeosChainHas(const struct GeosTS *Chain, int Length, uint8_t Track, uint8_t Sector)
{
   for (int i = 0; i < Length; i++)
   {
      if (Chain[i].Track == Track && Chain[i].Sector == Sector) return true;
   }
   return false;
}

static bool GeosDirChain(struct GeosDisk *Disk, struct GeosTS *Chain, int *Length)
{
   uint8_t Block[GEOS_SECTOR_SIZE];
   uint8_t Track, Sector;

   *Length = 0;
   if (!GeosReadTS(Disk, Disk->Format->DirTrack, 0, Block)) return false;
   Track = Block[0];
   Sector = Block[1];
   if (!Track) return GeosFail(Disk, "the disk header links to no directory");

   while (Track)
   {
      if (GeosChainHas(Chain, *Length, Track, Sector) || !GeosIsDirSector(Disk->Format, Track, Sector) ||
          *Length == GEOS_MAX_DIR_SECTORS)
      {
         snprintf(Disk->Error, sizeof Disk->Error, "the directory chain is broken at %u/%u", Track, Sector);
         return false;
      }
      Chain[(*Length)++] = (struct GeosTS){ Track, Sector };
      if (!GeosReadTS(Disk, Track, Sector, Block)) return false;
      Track = Block[0];
      Sector = Block[1];
   }
   return true;
}

// Chains one more directory sector, as DOS does when a directory fills.
static bool GeosGrowDirectory(struct GeosDisk *Disk, struct GeosTS *Chain, int *Length)
{
   const struct GeosFormat *Format = Disk->Format;
   struct GeosPlace Count, Bitmap;
   uint8_t Block[GEOS_SECTOR_SIZE];
   int New;

   GeosBamPlaces(Format, Format->DirTrack, &Count, &Bitmap);
   if (!GeosReadTS(Disk, Bitmap.Track, Bitmap.Sector, Block)) return false;
   for (int i = 0; i < *Length; i++)
   {
      if (!GeosSectorIsFree(Block + Bitmap.Byte, Chain[i].Sector)) continue;
      snprintf(Disk->Error, sizeof Disk->Error, "the BAM shows directory sector %u/%u free", Chain[i].Track, Chain[i].Sector);
      return false;
   }

   if (!GeosTake(Disk, Format->DirTrack, &New)) return false;
   if (New < 0 || !GeosIsDirSector(Format, Format->DirTrack, New) || *Length == GEOS_MAX_DIR_SECTORS)
   {
      return GeosFail(Disk, "the directory is full");
   }

   struct GeosTS Last = Chain[*Length - 1];
   if (!GeosReadTS(Disk, Last.Track, Last.Sector, Block)) return false;
   Block[0] = Format->DirTrack;
   Block[1] = New;
   if (!GeosWriteTS(Disk, Last.Track, Last.Sector, Block)) return false;

   memset(Block, 0, sizeof Block);
   Block[1] = 0xff;
   Chain[(*Length)++] = (struct GeosTS){ Format->DirTrack, (uint8_t)New };
   return GeosWriteTS(Disk, Format->DirTrack, New, Block);
}

// Writes the driver's data chain and info block; returns the first data sector, the info sector and the block count.
static bool GeosWriteDriver(struct GeosDisk *Disk, struct GeosTS *First, struct GeosTS *Info, uint16_t *Blocks)
{
   const uint16_t DataPerBlock = GEOS_SECTOR_SIZE - 2;
   uint16_t DataBlocks = (sizeof TRMouse128Code + DataPerBlock - 1) / DataPerBlock;
   struct GeosTS Here, Next;
   uint8_t Block[GEOS_SECTOR_SIZE];

   if (!GeosAllocate(Disk, &Here)) return false;
   *First = Here;
   for (uint16_t i = 0; i < DataBlocks; i++)
   {
      uint16_t Start = i * DataPerBlock, Length = sizeof TRMouse128Code - Start;
      if (Length > DataPerBlock) Length = DataPerBlock;
      bool Last = i + 1 == DataBlocks;

      if (!Last && !GeosAllocate(Disk, &Next)) return false;
      memset(Block, 0, sizeof Block);
      Block[0] = Last ? 0 : Next.Track;
      Block[1] = Last ? Length + 1 : Next.Sector;
      memcpy(Block + 2, TRMouse128Code + Start, Length);
      if (!GeosWriteTS(Disk, Here.Track, Here.Sector, Block)) return false;
      Here = Next;
   }

   if (!GeosAllocate(Disk, Info)) return false;
   *Blocks = DataBlocks + 1;
   return GeosWriteTS(Disk, Info->Track, Info->Sector, TRMouse128Info);
}

static void GeosBuildEntry(uint8_t *Slot, struct GeosTS First, struct GeosTS Info, uint16_t Blocks, const struct GeosDate *Date)
{
   memset(Slot + SlotType, 0, GEOS_DIR_ENTRY_SIZE - SlotType);
   Slot[SlotType] = CBM_USR_CLOSED;
   Slot[SlotFirst] = First.Track;
   Slot[SlotFirst + 1] = First.Sector;
   memset(Slot + SlotName, CBM_NAME_PAD, GEOS_NAME_SIZE);
   memcpy(Slot + SlotName, TRMouse128Name, strlen(TRMouse128Name));
   Slot[SlotInfo] = Info.Track;
   Slot[SlotInfo + 1] = Info.Sector;
   Slot[SlotStructure] = GEOS_SEQUENTIAL;
   Slot[SlotGeosType] = GEOS_INPUT_128;
   Slot[SlotDate] = Date->Year % 100;
   Slot[SlotDate + 1] = Date->Month;
   Slot[SlotDate + 2] = Date->Day;
   Slot[SlotDate + 3] = Date->Hour;
   Slot[SlotDate + 4] = Date->Minute;
   Slot[SlotBlocks] = Blocks & 0xff;
   Slot[SlotBlocks + 1] = Blocks >> 8;
}

static bool GeosSlotNamed(const uint8_t *Slot, const char *Name)
{
   size_t Length = strlen(Name);

   if (memcmp(Slot + SlotName, Name, Length)) return false;
   for (size_t i = Length; i < GEOS_NAME_SIZE; i++)
   {
      if (Slot[SlotName + i] != CBM_NAME_PAD) return false;
   }
   return true;
}

// A slot's place in directory order, as an index over the chain's sectors and their slots
struct GeosSlotRef { int Sector, Slot; };

struct GeosDirScan
{
   struct GeosSlotRef Free, FirstDriver;
   bool HasFree, HasDriver, HasMouse;
};

static bool GeosScanDirectory(struct GeosDisk *Disk, const struct GeosTS *Chain, int Length, struct GeosDirScan *Scan)
{
   uint8_t Block[GEOS_SECTOR_SIZE];

   memset(Scan, 0, sizeof *Scan);
   for (int i = 0; i < Length; i++)
   {
      if (!GeosReadTS(Disk, Chain[i].Track, Chain[i].Sector, Block)) return false;
      for (int e = 0; e < GEOS_ENTRIES_PER_DIR; e++)
      {
         const uint8_t *Slot = Block + e * GEOS_DIR_ENTRY_SIZE;
         struct GeosSlotRef Here = { i, e };
         bool InUse = Slot[SlotType] & CBM_TYPE_MASK;

         if (!Slot[SlotType] && !Scan->HasFree) { Scan->Free = Here; Scan->HasFree = true; }
         if (!InUse) continue;
         if (GeosSlotNamed(Slot, TRMouse128Name)) Scan->HasMouse = true;
         if (Slot[SlotGeosType] == GEOS_INPUT_128 && !Scan->HasDriver) { Scan->FirstDriver = Here; Scan->HasDriver = true; }
      }
   }
   return true;
}

static bool GeosSlotBefore(struct GeosSlotRef a, struct GeosSlotRef b)
{
   return a.Sector < b.Sector || (a.Sector == b.Sector && a.Slot < b.Slot);
}

static bool GeosSwapEntries(struct GeosDisk *Disk, const struct GeosTS *Chain, struct GeosSlotRef a, struct GeosSlotRef b)
{
   uint8_t BlockA[GEOS_SECTOR_SIZE], BlockB[GEOS_SECTOR_SIZE], Saved[GEOS_DIR_ENTRY_SIZE];
   bool Shared = a.Sector == b.Sector;
   uint8_t *SlotA = BlockA + a.Slot * GEOS_DIR_ENTRY_SIZE + SlotType;
   uint8_t *SlotB = (Shared ? BlockA : BlockB) + b.Slot * GEOS_DIR_ENTRY_SIZE + SlotType;
   const size_t EntryBytes = GEOS_DIR_ENTRY_SIZE - SlotType;

   if (!GeosReadTS(Disk, Chain[a.Sector].Track, Chain[a.Sector].Sector, BlockA)) return false;
   if (!Shared && !GeosReadTS(Disk, Chain[b.Sector].Track, Chain[b.Sector].Sector, BlockB)) return false;
   memcpy(Saved, SlotA, EntryBytes);
   memcpy(SlotA, SlotB, EntryBytes);
   memcpy(SlotB, Saved, EntryBytes);
   if (!Shared && !GeosWriteTS(Disk, Chain[b.Sector].Track, Chain[b.Sector].Sector, BlockB)) return false;
   return GeosWriteTS(Disk, Chain[a.Sector].Track, Chain[a.Sector].Sector, BlockA);
}

static bool GeosWriteEntry(struct GeosDisk *Disk, struct GeosTS Where, int Slot, const struct GeosDate *Date)
{
   struct GeosTS First, Info;
   uint16_t Blocks;
   uint8_t Block[GEOS_SECTOR_SIZE];

   if (!GeosWriteDriver(Disk, &First, &Info, &Blocks)) return false;
   if (!GeosReadTS(Disk, Where.Track, Where.Sector, Block)) return false;
   GeosBuildEntry(Block + Slot * GEOS_DIR_ENTRY_SIZE, First, Info, Blocks, Date);
   return GeosWriteTS(Disk, Where.Track, Where.Sector, Block);
}

const struct GeosFormat *GeosFormatForSize(uint32_t Size)
{
   for (size_t i = 0; i < sizeof GeosFormats / sizeof GeosFormats[0]; i++)
   {
      if (GeosFormats[i].Size == Size) return &GeosFormats[i];
   }
   return NULL;
}

// Checks the image and returns whether it already holds the driver.
bool GeosHasMouseDriver(struct GeosDisk *Disk, bool *HasMouse)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS];
   struct GeosDirScan Scan;
   uint8_t Header[GEOS_SECTOR_SIZE];
   int Length;

   if (!Disk->Format) return GeosFail(Disk, "not a D64, D71 or D81 without error bytes");
   if (!GeosReadTS(Disk, Disk->Format->DirTrack, 0, Header)) return false;
   if (Disk->Format->Size == 349696 && !(Header[3] & D71_DOUBLE_SIDED))
   {
      return GeosFail(Disk, "this D71 is formatted single-sided");
   }
   if (!GeosDirChain(Disk, Chain, &Length)) return false;
   if (!GeosScanDirectory(Disk, Chain, Length, &Scan)) return false;
   *HasMouse = Scan.HasMouse;
   return true;
}

// Writes the driver ahead of every other input driver, since GEOS 128 loads the first one in the directory.
bool GeosInstallMouseDriver(struct GeosDisk *Disk, const struct GeosDate *Date)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS];
   struct GeosDirScan Scan;
   bool HasMouse;
   int Length;

   if (!GeosHasMouseDriver(Disk, &HasMouse)) return false;
   if (HasMouse) return GeosFail(Disk, "TR USB MOUSE is already on this disk");
   if (!GeosDirChain(Disk, Chain, &Length)) return false;
   if (!GeosScanDirectory(Disk, Chain, Length, &Scan)) return false;
   if (!Scan.HasFree)
   {
      if (!GeosGrowDirectory(Disk, Chain, &Length)) return false;
      Scan.Free = (struct GeosSlotRef){ Length - 1, 0 };
   }

   if (!GeosWriteEntry(Disk, Chain[Scan.Free.Sector], Scan.Free.Slot, Date)) return false;
   if (!Scan.HasDriver || !GeosSlotBefore(Scan.FirstDriver, Scan.Free)) return true;
   return GeosSwapEntries(Disk, Chain, Scan.FirstDriver, Scan.Free);
}
