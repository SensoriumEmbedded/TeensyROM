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
#define GEOS_SYSTEM_FILE     4
#define GEOS_CONFIG_PREFIX   "CONFIG"
#define GEOS_BASIC_START     0x0801
#define GEOS_MEMORY_END      0x10000
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

   if (End <= Start || (size_t)(End - Start) < sizeof Code) return false;
   for (int i = VecDoneWithIO; i <= VecNewDisk; i++)
   {
      uint16_t Routine = GeosWord(Driver + 2 * i);
      if (Routine < Start || Routine >= End) return false;
   }
   for (int i = VecCount; i < GEOS_VECTOR_WORDS; i++)
   {
      uint16_t Other = GeosWord(Driver + 2 * i);
      if (Other >= Start && Other < End) return false;
   }
   for (int i = 0; i < GEOS_JUMP_COUNT; i++)
   {
      uint16_t Target = GeosWord(Driver + GEOS_JUMP_SLOTS + 3 * i + 1);
      bool Repointed = i == JumpSendTSBytes || i == JumpGetDOSError || i == JumpReadLink;
      if (!Repointed && Target >= Start && Target < End) return false;
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
   if (!At.Track) return GeosFail(Disk, PSTR("the boot file is not on the disk"));

   *Length = 0;
   for (int Blocks = 0; At.Track; Blocks++)
   {
      uint32_t Used;
      if (Blocks > GEOS_MAX_FILE_BLOCKS || At.Track > Disk->Format->Tracks) return GeosFail(Disk, PSTR("the boot file's chain is broken"));
      if (!GeosReadTS(Disk, At.Track, At.Sector, Block)) return false;
      Used = Block[0] ? GEOS_DATA_PER_BLOCK : (Block[1] > 1 ? Block[1] - 1 : 0);
      if (*Length + Used > Capacity) return GeosFail(Disk, PSTR("the boot file is too large"));
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

// The kernal's start-up code sets the date with three immediate stores and the clock in CIA 1 with another run;
// both are found in the boot file by their bytes, since the stock file fixes the date and time they set. The
// clock's run follows a loop that leaves X zero, which the patched run uses for the seconds and tenths.
static const uint8_t GeosStartDate[] = { 0xa9, 0x17, 0x8d, 0x18, 0x85, 0xa9, 0x03, 0x8d, 0x17, 0x85, 0xa9, 0x5c, 0x8d, 0x16, 0x85 };
static const uint8_t GeosStartClock[] =
{
   0xca, 0xd0, 0xe4,                               // dex : bne, the loop before it
   0xad, 0x0f, 0xdc, 0x29, 0x7f, 0x8d, 0x0f, 0xdc, // clear the alarm-set bit
   0xa9, 0x81, 0x8d, 0x0b, 0xdc,                   // hour 1 PM
   0xa9, 0x00, 0x8d, 0x0a, 0xdc,                   // minute 0
   0x8d, 0x09, 0xdc, 0x8d, 0x08, 0xdc,             // second and tenths 0; the tenths start the clock
};
#define GEOS_CLOCK_TIME 11
#define GEOS_PM_BIT     0x80

static uint8_t GeosBcd(uint8_t Value)
{
   return (Value / 10) << 4 | Value % 10;
}

// The hour byte that leaves CIA 1's TOD holding Hour: a 6526 flips AM/PM when hour 12 is written to it
static uint8_t GeosTodHour(uint8_t Hour)
{
   uint8_t Hour12 = Hour % 12 ? Hour % 12 : 12;
   bool Pm = (Hour >= 12) != (Hour12 == 12);

   return GeosBcd(Hour12) | (Pm ? GEOS_PM_BIT : 0);
}

// Finds Pattern once in Prg; NULL when it is absent or ambiguous
static uint8_t *GeosFindOnce(uint8_t *Prg, uint32_t Length, const uint8_t *Pattern, uint32_t Size)
{
   uint8_t *Found = NULL;

   for (uint32_t i = 0; i + Size <= Length; i++)
   {
      if (memcmp(Prg + i, Pattern, Size)) continue;
      if (Found) return NULL;
      Found = Prg + i;
   }
   return Found;
}

// Sets the date and time GEOS starts with. Leaves the PRG as it is, and returns false, for a boot file whose
// start-up code is not the one this knows
static bool GeosSetClock(uint8_t *Prg, uint32_t Length, const struct GeosDate *Now)
{
   uint8_t *Date = GeosFindOnce(Prg, Length, GeosStartDate, sizeof GeosStartDate);
   uint8_t *Clock = GeosFindOnce(Prg, Length, GeosStartClock, sizeof GeosStartClock);
   uint8_t Time[] =
   {
      0xa9, GeosTodHour(Now->Hour), 0x8d, 0x0b, 0xdc,
      0xa9, GeosBcd(Now->Minute), 0x8d, 0x0a, 0xdc,
      0x8e, 0x09, 0xdc, 0x8e, 0x08, 0xdc,
   };

   if (!Date || !Clock) return false;
   Date[1] = Now->Day;
   Date[6] = Now->Month;
   Date[11] = Now->Year;
   memcpy(Clock + GEOS_CLOCK_TIME, Time, sizeof Time);
   return true;
}

// One PRG that starts GEOS: the boot file with the disk driver's transport replaced, placed where its load
// address puts it, behind a BASIC line that jumps to the boot entry
static bool GeosBuildBootPrg(struct GeosDisk *Disk, uint8_t *Out, uint32_t Capacity, uint32_t *Length)
{
   const uint32_t Prologue = 2 + sizeof GeosBasicLine + sizeof GeosBootStub;
   uint32_t FileSize, BodySize, Load, Place;
   int32_t Driver;
   uint8_t *Body = Out + 2;

   if (!Disk->Target->BootFile) return GeosFail(Disk, PSTR("this version has no boot file to start from"));
   if (!GeosReadFile(Disk, Disk->Target->BootFile, Out, Capacity, &FileSize)) return false;
   if (FileSize < 2 + GEOS_DRIVER_SIZE) return GeosFail(Disk, PSTR("the boot file is too short"));
   BodySize = FileSize - 2;
   Load = GeosWord(Out);
   Driver = GeosFindDriver(Body, BodySize);
   if (Driver < 0 || !GeosReplaceTransport(Body + Driver)) return GeosFail(Disk, PSTR("the boot file's disk driver is not one the TR can replace"));
   Place = 2 + Load - GEOS_BASIC_START;
   if (Load < GEOS_BASIC_START + Prologue - 2 || Place + BodySize > Capacity || Load + BodySize > GEOS_MEMORY_END) return GeosFail(Disk, PSTR("the boot file does not fit in memory"));

   memmove(Out + Place, Body, BodySize);
   memset(Out, 0, Place);
   GeosPutWord(Out, GEOS_BASIC_START);
   memcpy(Out + 2, GeosBasicLine, sizeof GeosBasicLine);
   memcpy(Out + 2 + sizeof GeosBasicLine, GeosBootStub, sizeof GeosBootStub);
   GeosPutWord(Out + Prologue - 2, Disk->Target->BootEntry);
   *Length = Place + BodySize;
   return true;
}
