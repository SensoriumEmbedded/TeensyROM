// SPDX-License-Identifier: MIT
// Puts a TR input driver on a GEOS boot disk image, block by block.
// Mirrors tools/geos/trmouse.py install(), which the host test holds it to byte for byte.
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

#define GEOS_SECTOR_SIZE     256
#define GEOS_DIR_ENTRY_SIZE  32
#define GEOS_ENTRIES_PER_DIR (GEOS_SECTOR_SIZE / GEOS_DIR_ENTRY_SIZE)
#define GEOS_NAME_SIZE       16
#define GEOS_MAX_DIR_SECTORS 40
#define GEOS_ERROR_SIZE      80
#define GEOS_DATA_PER_BLOCK  (GEOS_SECTOR_SIZE - 2)

#define CBM_NAME_PAD         0xa0
#define CBM_TYPE_MASK        0x07
#define CBM_USR_CLOSED       0x83
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

struct GeosPlace { uint8_t Track, Sector, Byte; };
struct GeosTS    { uint8_t Track, Sector; };

struct GeosFormat
{
   const char *Name;
   uint8_t Tracks, DirTrack, FirstDirSector, LastDirSector;
   uint8_t Reserved[2];
   uint32_t Size;
   bool FixedSectors;   // D81: 40 per track; else the 1541's zones, repeated on a 1571's second side
   bool TwoSided;
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
};

struct GeosBootDisk { bool HasDeskTop, HasDriver; };

// A boot disk image reached through 256-byte block reads and writes at byte offsets.
struct GeosDisk
{
   const struct GeosFormat *Format;
   const struct GeosTarget *Target;
   void *Context;
   bool (*Read)(void *Context, uint32_t Offset, uint8_t *Block);
   bool (*Write)(void *Context, uint32_t Offset, const uint8_t *Block);
   char Error[GEOS_ERROR_SIZE];
};

static const struct GeosFormat GeosFormats[] =
{
   { "D64", 35, 18, 1, 18, { 18,  0 }, 174848, false, false },
   { "D71", 70, 18, 1, 18, { 18, 53 }, 349696, false, true  },
   { "D81", 80, 40, 3, 39, { 40,  0 }, 819200, true,  false },
};

FLASHMEM static bool GeosFail(struct GeosDisk *Disk, const char *Message)
{
   snprintf(Disk->Error, sizeof Disk->Error, PSTR("%s"), Message);
   return false;
}

FLASHMEM static uint8_t GeosSectorsOnTrack(const struct GeosFormat *Format, uint8_t Track)
{
   uint8_t ZoneTrack = (Track - 1) % 35 + 1;

   if (Format->FixedSectors) return 40;
   return ZoneTrack <= 17 ? 21 : ZoneTrack <= 24 ? 19 : ZoneTrack <= 30 ? 18 : 17;
}

FLASHMEM static uint32_t GeosOffset(const struct GeosFormat *Format, uint8_t Track, uint8_t Sector)
{
   uint32_t Blocks = Sector;

   for (uint8_t t = 1; t < Track; t++) Blocks += GeosSectorsOnTrack(Format, t);
   return Blocks * GEOS_SECTOR_SIZE;
}

FLASHMEM static bool GeosReadTS(struct GeosDisk *Disk, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   if (Disk->Read(Disk->Context, GeosOffset(Disk->Format, Track, Sector), Block)) return true;
   return GeosFail(Disk, PSTR("the image could not be read"));
}

FLASHMEM static bool GeosWriteTS(struct GeosDisk *Disk, uint8_t Track, uint8_t Sector, const uint8_t *Block)
{
   if (Disk->Write(Disk->Context, GeosOffset(Disk->Format, Track, Sector), Block)) return true;
   return GeosFail(Disk, PSTR("the image could not be written"));
}

FLASHMEM static bool GeosIsReserved(const struct GeosFormat *Format, uint8_t Track)
{
   return Track == Format->Reserved[0] || Track == Format->Reserved[1];
}

FLASHMEM static bool GeosIsDirSector(const struct GeosFormat *Format, uint8_t Track, uint8_t Sector)
{
   return Track == Format->DirTrack && Sector >= Format->FirstDirSector && Sector <= Format->LastDirSector;
}

// Where a track's free count and free-sector bitmap live
FLASHMEM static void GeosBamPlaces(const struct GeosFormat *Format, uint8_t Track, struct GeosPlace *Count, struct GeosPlace *Bitmap)
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

FLASHMEM static bool GeosSectorIsFree(const uint8_t *Bitmap, uint8_t Sector)
{
   return Bitmap[Sector / 8] & (1 << (Sector % 8));
}

FLASHMEM static uint8_t GeosCountFree(const struct GeosFormat *Format, uint8_t Track, const uint8_t *Bitmap, int *FirstFree)
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

// A track's BAM blocks as read; Count and Bits point into them.
struct GeosTrackBam
{
   struct GeosPlace CountAt, BitmapAt;
   bool Shared;
   uint8_t CountBlock[GEOS_SECTOR_SIZE], BitmapBlock[GEOS_SECTOR_SIZE];
   uint8_t *Count, *Bits, Free;
   int FirstFree;
};

FLASHMEM static bool GeosReadTrackBam(struct GeosDisk *Disk, uint8_t Track, struct GeosTrackBam *Bam)
{
   GeosBamPlaces(Disk->Format, Track, &Bam->CountAt, &Bam->BitmapAt);
   Bam->Shared = Bam->CountAt.Track == Bam->BitmapAt.Track && Bam->CountAt.Sector == Bam->BitmapAt.Sector;
   if (!GeosReadTS(Disk, Bam->CountAt.Track, Bam->CountAt.Sector, Bam->CountBlock)) return false;
   if (!Bam->Shared && !GeosReadTS(Disk, Bam->BitmapAt.Track, Bam->BitmapAt.Sector, Bam->BitmapBlock)) return false;
   Bam->Count = Bam->CountBlock + Bam->CountAt.Byte;
   Bam->Bits = (Bam->Shared ? Bam->CountBlock : Bam->BitmapBlock) + Bam->BitmapAt.Byte;

   Bam->Free = GeosCountFree(Disk->Format, Track, Bam->Bits, &Bam->FirstFree);
   return true;
}

FLASHMEM static bool GeosCheckCount(struct GeosDisk *Disk, uint8_t Track, const struct GeosTrackBam *Bam)
{
   if (*Bam->Count >= Bam->Free) return true;
   snprintf(Disk->Error, sizeof Disk->Error, PSTR("the BAM count for track %u disagrees with its bitmap"), Track);
   return false;
}

// Marks the first free sector of a track in use; *Sector is -1 when the track has none.
FLASHMEM static bool GeosTake(struct GeosDisk *Disk, uint8_t Track, int *Sector)
{
   struct GeosTrackBam Bam;

   if (!GeosReadTrackBam(Disk, Track, &Bam) || !GeosCheckCount(Disk, Track, &Bam)) return false;
   *Sector = Bam.FirstFree;
   if (*Sector < 0) return true;

   Bam.Bits[*Sector / 8] &= ~(1 << (*Sector % 8));
   (*Bam.Count)--;
   if (!Bam.Shared && !GeosWriteTS(Disk, Bam.BitmapAt.Track, Bam.BitmapAt.Sector, Bam.BitmapBlock)) return false;
   return GeosWriteTS(Disk, Bam.CountAt.Track, Bam.CountAt.Sector, Bam.CountBlock);
}

// Refuses, before anything is written, a disk that GeosAllocate would run out on partway.
// Reads the tracks GeosAllocate would visit, in its order, so the refusals match trmouse.py's.
FLASHMEM static bool GeosCheckRoom(struct GeosDisk *Disk, unsigned Needed)
{
   struct GeosTrackBam Bam;
   unsigned Found = 0;

   for (uint8_t Track = 1; Track <= Disk->Format->Tracks; Track++)
   {
      if (GeosIsReserved(Disk->Format, Track)) continue;
      if (!GeosReadTrackBam(Disk, Track, &Bam) || !GeosCheckCount(Disk, Track, &Bam)) return false;
      Found += Bam.Free;
      if (Found >= Needed) return true;
   }
   return GeosFail(Disk, PSTR("the disk is full"));
}

FLASHMEM static bool GeosAllocate(struct GeosDisk *Disk, struct GeosTS *Where)
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
   return GeosFail(Disk, PSTR("the disk is full"));
}

FLASHMEM static bool GeosChainHas(const struct GeosTS *Chain, int Length, uint8_t Track, uint8_t Sector)
{
   for (int i = 0; i < Length; i++)
   {
      if (Chain[i].Track == Track && Chain[i].Sector == Sector) return true;
   }
   return false;
}

FLASHMEM static bool GeosDirChain(struct GeosDisk *Disk, struct GeosTS *Chain, int *Length)
{
   uint8_t Block[GEOS_SECTOR_SIZE];
   uint8_t Track, Sector;

   *Length = 0;
   if (!GeosReadTS(Disk, Disk->Format->DirTrack, 0, Block)) return false;
   Track = Block[0];
   Sector = Block[1];
   if (!Track) return GeosFail(Disk, PSTR("the disk header links to no directory"));

   while (Track)
   {
      if (GeosChainHas(Chain, *Length, Track, Sector) || !GeosIsDirSector(Disk->Format, Track, Sector) ||
          *Length == GEOS_MAX_DIR_SECTORS)
      {
         snprintf(Disk->Error, sizeof Disk->Error, PSTR("the directory chain is broken at %u/%u"), Track, Sector);
         return false;
      }
      Chain[(*Length)++] = (struct GeosTS){ Track, Sector };
      if (!GeosReadTS(Disk, Track, Sector, Block)) return false;
      Track = Block[0];
      Sector = Block[1];
   }
   return true;
}

// Refuses, before anything is written, a directory that GeosGrowDirectory could not grow.
FLASHMEM static bool GeosCheckDirectoryRoom(struct GeosDisk *Disk, const struct GeosTS *Chain, int Length)
{
   const struct GeosFormat *Format = Disk->Format;
   struct GeosTrackBam Bam;

   if (!GeosReadTrackBam(Disk, Format->DirTrack, &Bam)) return false;
   for (int i = 0; i < Length; i++)
   {
      if (!GeosSectorIsFree(Bam.Bits, Chain[i].Sector)) continue;
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("the BAM shows directory sector %u/%u free"), Chain[i].Track, Chain[i].Sector);
      return false;
   }
   if (!GeosCheckCount(Disk, Format->DirTrack, &Bam)) return false;
   if (Bam.FirstFree >= 0 && GeosIsDirSector(Format, Format->DirTrack, Bam.FirstFree)) return true;
   return GeosFail(Disk, PSTR("the directory is full"));
}

// Chains one more directory sector, as DOS does when a directory fills.
FLASHMEM static bool GeosGrowDirectory(struct GeosDisk *Disk, struct GeosTS *Chain, int *Length)
{
   const struct GeosFormat *Format = Disk->Format;
   uint8_t Block[GEOS_SECTOR_SIZE];
   int New;

   if (*Length == GEOS_MAX_DIR_SECTORS) return GeosFail(Disk, PSTR("the directory is full"));
   if (!GeosCheckDirectoryRoom(Disk, Chain, *Length) || !GeosTake(Disk, Format->DirTrack, &New)) return false;

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

FLASHMEM static uint16_t GeosDriverDataBlocks(const struct GeosTarget *Target)
{
   return (Target->DriverSize + GEOS_DATA_PER_BLOCK - 1) / GEOS_DATA_PER_BLOCK;
}

FLASHMEM static uint16_t GeosDriverBlocks(const struct GeosTarget *Target)
{
   return GeosDriverDataBlocks(Target) + 1;
}

// Writes the driver's data chain and info block; returns the first data sector, the info sector and the block count.
FLASHMEM static bool GeosWriteDriver(struct GeosDisk *Disk, struct GeosTS *First, struct GeosTS *Info, uint16_t *Blocks)
{
   const struct GeosTarget *Target = Disk->Target;
   const uint16_t DataBlocks = GeosDriverDataBlocks(Target);
   struct GeosTS Here, Next = { 0, 0 };
   uint8_t Block[GEOS_SECTOR_SIZE];

   if (!GeosAllocate(Disk, &Here)) return false;
   *First = Here;
   for (uint16_t i = 0; i < DataBlocks; i++)
   {
      uint16_t Start = i * GEOS_DATA_PER_BLOCK, Length = Target->DriverSize - Start;
      if (Length > GEOS_DATA_PER_BLOCK) Length = GEOS_DATA_PER_BLOCK;
      bool Last = i + 1 == DataBlocks;

      if (!Last && !GeosAllocate(Disk, &Next)) return false;
      memset(Block, 0, sizeof Block);
      Block[0] = Last ? 0 : Next.Track;
      Block[1] = Last ? Length + 1 : Next.Sector;
      memcpy(Block + 2, Target->DriverCode + Start, Length);
      if (!GeosWriteTS(Disk, Here.Track, Here.Sector, Block)) return false;
      Here = Next;
   }

   if (!GeosAllocate(Disk, Info)) return false;
   *Blocks = GeosDriverBlocks(Target);
   return GeosWriteTS(Disk, Info->Track, Info->Sector, Target->DriverInfo);
}

FLASHMEM static void GeosBuildEntry(const struct GeosTarget *Target, uint8_t *Slot, struct GeosTS First, struct GeosTS Info, uint16_t Blocks, const struct GeosDate *Date)
{
   memset(Slot + SlotType, 0, GEOS_DIR_ENTRY_SIZE - SlotType);
   Slot[SlotType] = CBM_USR_CLOSED;
   Slot[SlotFirst] = First.Track;
   Slot[SlotFirst + 1] = First.Sector;
   memset(Slot + SlotName, CBM_NAME_PAD, GEOS_NAME_SIZE);
   memcpy(Slot + SlotName, Target->DriverName, strlen(Target->DriverName));
   Slot[SlotInfo] = Info.Track;
   Slot[SlotInfo + 1] = Info.Sector;
   Slot[SlotStructure] = GEOS_SEQUENTIAL;
   Slot[SlotGeosType] = Target->InputType;
   Slot[SlotDate] = Date->Year % 100;
   Slot[SlotDate + 1] = Date->Month;
   Slot[SlotDate + 2] = Date->Day;
   Slot[SlotDate + 3] = Date->Hour;
   Slot[SlotDate + 4] = Date->Minute;
   Slot[SlotBlocks] = Blocks & 0xff;
   Slot[SlotBlocks + 1] = Blocks >> 8;
}

FLASHMEM static bool GeosSlotNamed(const uint8_t *Slot, const char *Name)
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
   struct GeosSlotRef Free, FirstInputDriver;
   bool HasFree, HasInputDriver, HasDeskTop, HasOurs;
};

FLASHMEM static bool GeosScanDirectory(struct GeosDisk *Disk, const struct GeosTS *Chain, int Length, struct GeosDirScan *Scan)
{
   const struct GeosTarget *Target = Disk->Target;
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
         if (GeosSlotNamed(Slot, Target->DriverName)) Scan->HasOurs = true;
         if (GeosSlotNamed(Slot, Target->DeskTopName)) Scan->HasDeskTop = true;
         if (Slot[SlotGeosType] == Target->InputType && !Scan->HasInputDriver) { Scan->FirstInputDriver = Here; Scan->HasInputDriver = true; }
      }
   }
   return true;
}

FLASHMEM static bool GeosSlotBefore(struct GeosSlotRef a, struct GeosSlotRef b)
{
   return a.Sector < b.Sector || (a.Sector == b.Sector && a.Slot < b.Slot);
}

FLASHMEM static bool GeosSwapEntries(struct GeosDisk *Disk, const struct GeosTS *Chain, struct GeosSlotRef a, struct GeosSlotRef b)
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

FLASHMEM static bool GeosWriteEntry(struct GeosDisk *Disk, struct GeosTS Where, int Slot, const struct GeosDate *Date)
{
   struct GeosTS First, Info;
   uint16_t Blocks;
   uint8_t Block[GEOS_SECTOR_SIZE];

   if (!GeosWriteDriver(Disk, &First, &Info, &Blocks)) return false;
   if (!GeosReadTS(Disk, Where.Track, Where.Sector, Block)) return false;
   GeosBuildEntry(Disk->Target, Block + Slot * GEOS_DIR_ENTRY_SIZE, First, Info, Blocks, Date);
   return GeosWriteTS(Disk, Where.Track, Where.Sector, Block);
}

FLASHMEM static inline const struct GeosFormat *GeosFormatForSize(uint32_t Size)
{
   for (size_t i = 0; i < sizeof GeosFormats / sizeof GeosFormats[0]; i++)
   {
      if (GeosFormats[i].Size == Size) return &GeosFormats[i];
   }
   return NULL;
}

FLASHMEM static bool GeosReadDirectory(struct GeosDisk *Disk, struct GeosTS *Chain, int *Length, struct GeosDirScan *Scan)
{
   uint8_t Header[GEOS_SECTOR_SIZE];

   const struct GeosTarget *Target = Disk->Target;

   if (strlen(Target->DeskTopName) > GEOS_NAME_SIZE || strlen(Target->DriverName) > GEOS_NAME_SIZE || !Target->DriverSize)
   {
      return GeosFail(Disk, PSTR("the GEOS target's names or driver do not fit a directory entry"));
   }
   if (!Disk->Format) return GeosFail(Disk, PSTR("not a D64, D71 or D81 without error bytes"));
   if (!GeosReadTS(Disk, Disk->Format->DirTrack, 0, Header)) return false;
   if (Disk->Format->TwoSided && !(Header[3] & D71_DOUBLE_SIDED))
   {
      return GeosFail(Disk, PSTR("this D71 is formatted single-sided"));
   }
   if (!GeosDirChain(Disk, Chain, Length)) return false;
   return GeosScanDirectory(Disk, Chain, *Length, Scan);
}

// Checks the image and says whether it is the target's boot disk and already holds the TR driver.
FLASHMEM static inline bool GeosInspect(struct GeosDisk *Disk, struct GeosBootDisk *Found)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS];
   struct GeosDirScan Scan;
   int Length;

   if (!GeosReadDirectory(Disk, Chain, &Length, &Scan)) return false;
   Found->HasDeskTop = Scan.HasDeskTop;
   Found->HasDriver = Scan.HasOurs;
   return true;
}

// Writes the driver ahead of every other input driver of its type: GEOS 128 boots with the first one in the directory.
// Every refusal but a failed block read or write comes before the first write, leaving the image as it was.
FLASHMEM static inline bool GeosInstallDriver(struct GeosDisk *Disk, const struct GeosDate *Date)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS];
   struct GeosDirScan Scan;
   int Length;

   if (!GeosReadDirectory(Disk, Chain, &Length, &Scan)) return false;
   if (Scan.HasOurs)
   {
      snprintf(Disk->Error, sizeof Disk->Error, PSTR("%s is already on this disk"), Disk->Target->DriverName);
      return false;
   }
   if (!Scan.HasFree && !GeosCheckDirectoryRoom(Disk, Chain, Length)) return false;
   if (!GeosCheckRoom(Disk, GeosDriverBlocks(Disk->Target))) return false;
   if (!Scan.HasFree)
   {
      if (!GeosGrowDirectory(Disk, Chain, &Length)) return false;
      Scan.Free = (struct GeosSlotRef){ Length - 1, 0 };
   }

   if (!GeosWriteEntry(Disk, Chain[Scan.Free.Sector], Scan.Free.Slot, Date)) return false;
   if (!Scan.HasInputDriver || !GeosSlotBefore(Scan.FirstInputDriver, Scan.Free)) return true;
   return GeosSwapEntries(Disk, Chain, Scan.FirstInputDriver, Scan.Free);
}
