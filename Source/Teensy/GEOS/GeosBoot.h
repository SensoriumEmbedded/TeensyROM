// SPDX-License-Identifier: MIT
// What launching a GEOS boot disk from the TR needs of its image: the boot file with the disk driver swapped for
// the TR's, and the disk's own auto-configuration kept from probing a drive that is not there.
#pragma once
#include "GeosDiskPatch.h"
#include "TRDriver.h"

#define GEOS_DRIVER_AREA     0x9000
#define GEOS_DRIVER_SIZE     0xd80
#define GEOS_VECTOR_WORDS    24
#define GEOS_JUMP_SLOTS      0x30
#define GEOS_JUMP_COUNT      10
#define GEOS_TYPE_BYTE       0x4e
#define GEOS_JUMP_OPCODE     0x4c
#define GEOS_AUTO_EXEC       14
#define GEOS_SYSTEM_FILE     5
#define GEOS_CONFIG_PREFIX   "CONFIG"
#define GEOS_BASIC_START     0x0801
#define GEOS_MAX_FILE_BLOCKS 3200
#define GEOS_DEVICE_8        8

enum GeosDriverVector { VecInitForIO, VecDoneWithIO, VecExitTurbo, VecPurgeTurbo, VecEnterTurbo, VecChangeDiskDevice,
                        VecNewDisk, VecReadBlock, VecWriteBlock, VecVerWriteBlock, VecCount = 10 };
enum GeosDriverJump { JumpSendTSBytes = 6, JumpGetDOSError = 7, JumpReadLink = 9 };
enum TRDriverEntry { EntrySendTSBytes = 10, EntryGetDOSError, EntryReadLink };

static uint16_t GeosWord(const uint8_t *At) { return At[0] | At[1] << 8; }

static void GeosPutWord(uint8_t *At, uint16_t Value)
{
   At[0] = Value & 0xff;
   At[1] = Value >> 8;
}

static bool GeosInDriverArea(uint16_t Address) { return Address >= GEOS_DRIVER_AREA && Address < GEOS_DRIVER_AREA + GEOS_DRIVER_SIZE; }

// Whether a stock disk driver's tables start here: the vectors and the jump slots all point into the driver, and
// the type byte names a 1541, 1571 or 1581
static bool GeosIsDriverAt(const uint8_t *At)
{
   uint8_t Type = At[GEOS_TYPE_BYTE];

   for (int i = 0; i < GEOS_VECTOR_WORDS; i++)
   {
      if (!GeosInDriverArea(GeosWord(At + 2 * i))) return false;
   }
   for (int i = 0; i < GEOS_JUMP_COUNT; i++)
   {
      const uint8_t *Slot = At + GEOS_JUMP_SLOTS + 3 * i;
      if (Slot[0] != GEOS_JUMP_OPCODE || !GeosInDriverArea(GeosWord(Slot + 1))) return false;
   }
   return Type >= 1 && Type <= 3;
}

// Offset of the stock driver's image in a boot file, or -1
static int32_t GeosFindDriver(const uint8_t *Data, uint32_t Size)
{
   for (uint32_t At = 0; At + GEOS_DRIVER_SIZE <= Size; At++)
   {
      if (GeosIsDriverAt(Data + At)) return At;
   }
   return -1;
}

// The stock driver's turbo routines occupy one run of its code, from InitForIO up to ReadBlock; the TR's
// driver replaces them and points every vector it implements at its own routines
static bool GeosReplaceTransport(uint8_t *Driver)
{
   uint16_t Start = GeosWord(Driver + 2 * VecInitForIO), End = GeosWord(Driver + 2 * VecReadBlock);
   uint8_t Code[sizeof TRDriverCode];

   if (End <= Start || End - Start < sizeof Code) return false;
   for (int i = VecDoneWithIO; i <= VecNewDisk; i++)
   {
      uint16_t Routine = GeosWord(Driver + 2 * i);
      if (Routine < Start || Routine >= End) return false;
   }
   memcpy(Code, TRDriverCode, sizeof Code);
   for (size_t i = 0; i < sizeof TRDriverRelocations / sizeof TRDriverRelocations[0]; i++)
   {
      uint8_t *Word = Code + TRDriverRelocations[i];
      GeosPutWord(Word, GeosWord(Word) + Start - TR_DRIVER_BASE);
   }
   memcpy(Driver + (Start - GEOS_DRIVER_AREA), Code, sizeof Code);
   for (int i = 0; i < VecCount; i++) GeosPutWord(Driver + 2 * i, Start + TRDriverEntries[i]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpSendTSBytes + 1, Start + TRDriverEntries[EntrySendTSBytes]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpGetDOSError + 1, Start + TRDriverEntries[EntryGetDOSError]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpReadLink + 1, Start + TRDriverEntries[EntryReadLink]);
   return true;
}

// A file's bytes, found by name in the directory and followed along its sector chain
static bool GeosReadFile(struct GeosDisk *Disk, const char *Name, uint8_t *Out, uint32_t Capacity, uint32_t *Length)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS], At = { 0, 0 };
   uint8_t Block[GEOS_SECTOR_SIZE];
   struct GeosDirScan Scan;
   int Count;

   if (!GeosReadDirectory(Disk, Chain, &Count, &Scan)) return false;
   for (int i = 0; i < Count && !At.Track; i++)
   {
      if (!GeosReadTS(Disk, Chain[i].Track, Chain[i].Sector, Block)) return false;
      for (int e = 0; e < GEOS_ENTRIES_PER_DIR; e++)
      {
         const uint8_t *Slot = Block + e * GEOS_DIR_ENTRY_SIZE;
         if (!(Slot[SlotType] & CBM_TYPE_MASK) || !GeosSlotNamed(Slot, Name)) continue;
         At = (struct GeosTS){ Slot[SlotFirst], Slot[SlotFirst + 1] };
         break;
      }
   }
   if (!At.Track) return GeosFail(Disk, "the boot file is not on the disk");

   *Length = 0;
   for (int Blocks = 0; At.Track; Blocks++)
   {
      uint32_t Used;
      if (Blocks > GEOS_MAX_FILE_BLOCKS || At.Track > Disk->Format->Tracks) return GeosFail(Disk, "the boot file's chain is broken");
      if (!GeosReadTS(Disk, At.Track, At.Sector, Block)) return false;
      Used = Block[0] ? GEOS_DATA_PER_BLOCK : (Block[1] > 1 ? Block[1] - 1 : 0);
      if (*Length + Used > Capacity) return GeosFail(Disk, "the boot file is too large");
      memcpy(Out + *Length, Block + 2, Used);
      *Length += Used;
      At = (struct GeosTS){ Block[0], Block[1] };
   }
   return true;
}

// Gives AUTO_EXEC configuration files the type of a file GEOS leaves alone, the way the stock disk's own
// configuration probes the IEC bus and empties drive A; returns whether anything was retyped
static bool GeosDisableConfigure(struct GeosDisk *Disk, bool *Changed)
{
   struct GeosTS Chain[GEOS_MAX_DIR_SECTORS];
   uint8_t Block[GEOS_SECTOR_SIZE];
   struct GeosDirScan Scan;
   int Count;

   *Changed = false;
   if (!GeosReadDirectory(Disk, Chain, &Count, &Scan)) return false;
   for (int i = 0; i < Count; i++)
   {
      bool Retyped = false;
      if (!GeosReadTS(Disk, Chain[i].Track, Chain[i].Sector, Block)) return false;
      for (int e = 0; e < GEOS_ENTRIES_PER_DIR; e++)
      {
         uint8_t *Slot = Block + e * GEOS_DIR_ENTRY_SIZE;
         if (!(Slot[SlotType] & CBM_TYPE_MASK) || Slot[SlotGeosType] != GEOS_AUTO_EXEC) continue;
         if (strncmp((char *)Slot + SlotName, GEOS_CONFIG_PREFIX, strlen(GEOS_CONFIG_PREFIX))) continue;
         Slot[SlotGeosType] = GEOS_SYSTEM_FILE;
         Retyped = true;
      }
      if (!Retyped) continue;
      if (!GeosWriteTS(Disk, Chain[i].Track, Chain[i].Sector, Block)) return false;
      *Changed = true;
   }
   return true;
}

// BASIC "10 SYS 2061" and the stub it runs: the screen colors and drive 8 the stock GEOS start-up file sets, then
// the boot file's entry. The stock file would LOAD the boot file from the drive; here it arrives in the same PRG.
static const uint8_t GeosBasicLine[] = { 0x0b, 0x08, 0x0a, 0x00, 0x9e, '2', '0', '6', '1', 0x00, 0x00, 0x00 };
static const uint8_t GeosBootStub[] =
{
   0xa9, 0x0e, 0x8d, 0x20, 0xd0,    // lda #$0e : sta $d020
   0xa0, 0x06, 0x8c, 0x21, 0xd0,    // ldy #$06 : sty $d021
   0xa9, GEOS_DEVICE_8, 0x85, 0xba, // lda #8 : sta $ba
   0x4c, 0x00, 0x00,                // jmp entry
};

// One PRG that starts GEOS: the boot file with the disk driver's transport replaced, placed where its load
// address puts it, behind a BASIC line that jumps to the boot entry
static bool GeosBuildBootPrg(struct GeosDisk *Disk, uint8_t *Out, uint32_t Capacity, uint32_t *Length)
{
   const uint32_t Prologue = 2 + sizeof GeosBasicLine + sizeof GeosBootStub;
   uint32_t FileSize, BodySize, Load, Place;
   int32_t Driver;
   uint8_t *Body = Out + 2;

   if (!GeosReadFile(Disk, Disk->Target->BootFile, Out, Capacity, &FileSize)) return false;
   if (FileSize < 2 + GEOS_DRIVER_SIZE) return GeosFail(Disk, "the boot file is too short");
   BodySize = FileSize - 2;
   Load = GeosWord(Out);
   Driver = GeosFindDriver(Body, BodySize);
   if (Driver < 0 || !GeosReplaceTransport(Body + Driver)) return GeosFail(Disk, "the boot file's disk driver is not one the TR can replace");
   Place = 2 + Load - GEOS_BASIC_START;
   if (Load < GEOS_BASIC_START + Prologue - 2 || Place + BodySize > Capacity) return GeosFail(Disk, "the boot file does not fit in memory");

   memmove(Out + Place, Body, BodySize);
   memset(Out, 0, Place);
   GeosPutWord(Out, GEOS_BASIC_START);
   memcpy(Out + 2, GeosBasicLine, sizeof GeosBasicLine);
   memcpy(Out + 2 + sizeof GeosBasicLine, GeosBootStub, sizeof GeosBootStub);
   GeosPutWord(Out + Prologue - 2, Disk->Target->BootEntry);
   *Length = Place + BodySize;
   return true;
}
