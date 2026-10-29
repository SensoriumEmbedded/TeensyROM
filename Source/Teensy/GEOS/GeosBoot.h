// SPDX-License-Identifier: MIT
// What launching a GEOS boot disk from the TR needs of its image: the boot file with the disk driver swapped for
// the TR's, and the disk's own auto-configuration kept from probing a drive that is not there.
#pragma once
#include "Geos128Boot.h"
#include "GeosDiskPatch.h"

#define GEOS_DRIVER_AREA     0x9000
#define GEOS_DRIVER_SIZE     0xd80
#define GEOS_VECTOR_WORDS    24
#define GEOS_JUMP_SLOTS      0x30
#define GEOS_JUMP_COUNT      10
#define GEOS_TYPE_BYTE       0x4e
#define GEOS_JUMP_OPCODE     0x4c
#define GEOS_AUTO_EXEC       14
#define GEOS_SYSTEM_FILE     4
#define GEOS_CONFIG_NAME     "CONFIG"
#define GEOS_BASIC_START     0x0801
#define GEOS_MEMORY_END      0x10000
#define GEOS_MAX_FILE_BLOCKS 3200
#define GEOS_DEVICE_8        8
#define GEOS128_SHARED_RAM_END 0x2000   // the C128 boot ROM's loader runs in the RAM below, which both banks share
#define GEOS128_IO_START       0xd000   // and writes bank 1 with I/O mapped in from here up

enum GeosDriverVector { VecInitForIO, VecDoneWithIO, VecExitTurbo, VecPurgeTurbo, VecEnterTurbo, VecChangeDiskDevice,
                        VecNewDisk, VecReadBlock, VecWriteBlock, VecVerWriteBlock, VecCount = 10 };
enum GeosDriverJump { JumpSendTSBytes = 6, JumpGetDOSError = 7, JumpReadLink = 9 };
enum TRDriverEntry { EntrySendTSBytes = 10, EntryGetDOSError, EntryReadLink };

// A TR disk driver as tools/geos/trdrive.py emits it: code assembled at Base, the offsets of the words that hold
// addresses inside it, and the offsets of its routines in TRDriverEntry order
struct GeosDiskDriver
{
   const uint8_t *Code;
   uint16_t Size, Base;
   const uint16_t *Relocations;
   uint16_t RelocationCount;
   const uint16_t *Entries;
};

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

// The TR's driver goes over the stock driver's code from InitForIO up to ReadBlock, provided no vector or jump slot
// it leaves alone points there, and every vector it implements is pointed at its own routines. Replaced routines
// outside that run stay where they are, unused: GEOS 128 keeps NewDisk after ReadBlock.
static bool GeosReplaceTransport(uint8_t *Driver, const struct GeosDiskDriver *With)
{
   uint16_t Start = GeosWord(Driver + 2 * VecInitForIO), End = GeosWord(Driver + 2 * VecReadBlock);
   uint8_t *Code = Driver + (Start - GEOS_DRIVER_AREA);

   if (End <= Start || (size_t)(End - Start) < With->Size) return false;
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
   memcpy(Code, With->Code, With->Size);
   for (uint16_t i = 0; i < With->RelocationCount; i++)
   {
      uint8_t *Word = Code + With->Relocations[i];
      GeosPutWord(Word, GeosWord(Word) + Start - With->Base);
   }
   for (int i = 0; i < VecCount; i++) GeosPutWord(Driver + 2 * i, Start + With->Entries[i]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpSendTSBytes + 1, Start + With->Entries[EntrySendTSBytes]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpGetDOSError + 1, Start + With->Entries[EntryGetDOSError]);
   GeosPutWord(Driver + GEOS_JUMP_SLOTS + 3 * JumpReadLink + 1, Start + With->Entries[EntryReadLink]);
   return true;
}

// A file's bytes, found by name in the directory and followed along its sector chain
FLASHMEM static bool GeosReadFile(struct GeosDisk *Disk, const char *Name, uint8_t *Out, uint32_t Capacity, uint32_t *Length)
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

static bool GeosNameContains(const uint8_t *Name, const char *Part)
{
   size_t Length = strlen(Part);

   for (size_t i = 0; i + Length <= GEOS_NAME_SIZE; i++)
   {
      if (!memcmp(Name + i, Part, Length)) return true;
   }
   return false;
}

// Gives AUTO_EXEC configuration files the type of a file GEOS leaves alone, the way the stock disk's own
// configuration probes the IEC bus and empties drive A; returns whether anything was retyped
static inline bool GeosDisableConfigure(struct GeosDisk *Disk, bool *Changed)
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
         if (!GeosNameContains(Slot + SlotName, GEOS_CONFIG_NAME)) continue;
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

// One run of a boot file's start-up code that sets the date or the time. It is found by its bytes, since the stock
// file fixes the values it sets, and Set makes it set Now instead without changing its length.
struct GeosClockRun
{
   const uint8_t *Pattern;
   uint8_t Size;
   void (*Set)(uint8_t *Run, const struct GeosDate *Now);
};

// GEOS 64 sets the date with three immediate stores and the clock in CIA 1 with another run. The clock's run
// follows a loop that leaves X zero, which the patched run uses for the seconds and tenths.
static const uint8_t Geos64StartDate[] = { 0xa9, 0x17, 0x8d, 0x18, 0x85, 0xa9, 0x03, 0x8d, 0x17, 0x85, 0xa9, 0x5c, 0x8d, 0x16, 0x85 };
static const uint8_t Geos64StartClock[] =
{
   0xca, 0xd0, 0xe4,                               // dex : bne, the loop before it
   0xad, 0x0f, 0xdc, 0x29, 0x7f, 0x8d, 0x0f, 0xdc, // clear the alarm-set bit
   0xa9, 0x81, 0x8d, 0x0b, 0xdc,                   // hour 1 PM
   0xa9, 0x00, 0x8d, 0x0a, 0xdc,                   // minute 0
   0x8d, 0x09, 0xdc, 0x8d, 0x08, 0xdc,             // second and tenths 0; the tenths start the clock
};
#define GEOS64_CLOCK_TIME 11

FLASHMEM static void Geos64SetDate(uint8_t *Run, const struct GeosDate *Now)
{
   Run[1] = Now->Day;
   Run[6] = Now->Month;
   Run[11] = Now->Year;
}

FLASHMEM static void Geos64SetTime(uint8_t *Run, const struct GeosDate *Now)
{
   const uint8_t Time[] =
   {
      0xa9, GeosTodHour(Now->Hour), 0x8d, 0x0b, 0xdc,
      0xa9, GeosBcd(Now->Minute), 0x8d, 0x0a, 0xdc,
      0x8e, 0x09, 0xdc, 0x8e, 0x08, 0xdc,
   };

   memcpy(Run + GEOS64_CLOCK_TIME, Time, sizeof Time);
}

static const struct GeosClockRun Geos64ClockRuns[] =
{
   { Geos64StartDate, sizeof Geos64StartDate, Geos64SetDate },
   { Geos64StartClock, sizeof Geos64StartClock, Geos64SetTime },
};

// GEOS 128 sets the clock and then the date in one run, behind a store of zero. No register is known to be zero
// there and the run has no byte to spare for clearing one, so the patched run makes that store load X instead.
static const uint8_t Geos128StartUp[] =
{
   0xa9, 0x00, 0x8d, 0xc5, 0x88,                   // lda #0 : sta $88c5
   0xad, 0x0f, 0xdc, 0x29, 0x7f, 0x8d, 0x0f, 0xdc, // clear the alarm-set bit
   0xa9, 0x81, 0x8d, 0x0b, 0xdc,                   // hour 1 PM
   0xa9, 0x00, 0x8d, 0x0a, 0xdc,                   // minute 0
   0x8d, 0x09, 0xdc, 0x8d, 0x08, 0xdc,             // second and tenths 0; the tenths start the clock
   0xa9, 0x5c, 0x8d, 0x16, 0x85,                   // year 92
   0xa9, 0x03, 0x8d, 0x17, 0x85,                   // month 3
   0xa9, 0x17, 0x8d, 0x18, 0x85,                   // day 23
};

FLASHMEM static void Geos128SetClock(uint8_t *Run, const struct GeosDate *Now)
{
   const uint8_t StartUp[sizeof Geos128StartUp] =
   {
      0xa2, 0x00, 0x8e, 0xc5, 0x88,
      0xad, 0x0f, 0xdc, 0x29, 0x7f, 0x8d, 0x0f, 0xdc,
      0xa9, GeosTodHour(Now->Hour), 0x8d, 0x0b, 0xdc,
      0xa9, GeosBcd(Now->Minute), 0x8d, 0x0a, 0xdc,
      0x8e, 0x09, 0xdc, 0x8e, 0x08, 0xdc,
      0xa9, Now->Year, 0x8d, 0x16, 0x85,
      0xa9, Now->Month, 0x8d, 0x17, 0x85,
      0xa9, Now->Day, 0x8d, 0x18, 0x85,
   };

   memcpy(Run, StartUp, sizeof StartUp);
}

static const struct GeosClockRun Geos128ClockRuns[] = { { Geos128StartUp, sizeof Geos128StartUp, Geos128SetClock } };

// Finds Pattern once in Body; NULL when it is absent or ambiguous
static uint8_t *GeosFindOnce(uint8_t *Body, uint32_t Length, const uint8_t *Pattern, uint32_t Size)
{
   uint8_t *Found = NULL;

   for (uint32_t i = 0; i + Size <= Length; i++)
   {
      if (memcmp(Body + i, Pattern, Size)) continue;
      if (Found) return NULL;
      Found = Body + i;
   }
   return Found;
}

// Sets the date and time GEOS starts with. Leaves the body as it is, and returns false, for a boot file whose
// start-up code is not the one the target knows
static bool GeosSetClock(uint8_t *Body, uint32_t Length, const struct GeosTarget *Target, const struct GeosDate *Now)
{
   for (uint8_t i = 0; i < Target->ClockRunCount; i++)
   {
      const struct GeosClockRun *Run = &Target->ClockRuns[i];
      if (!GeosFindOnce(Body, Length, Run->Pattern, Run->Size)) return false;
   }
   for (uint8_t i = 0; i < Target->ClockRunCount; i++)
   {
      const struct GeosClockRun *Run = &Target->ClockRuns[i];
      Run->Set(GeosFindOnce(Body, Length, Run->Pattern, Run->Size), Now);
   }
   return true;
}

// GEOS 128 leaves "CBM" and $03E4 in bank 1 at $FFF5, and the C128 kernal resumes there on every reset until the
// power goes off. The TR menu runs in C64 mode and overwrites $03E4, so the next C128 program would crash.
static const uint8_t GeosResetKey[] = { 'C', 'B', 'M', 0xe4, 0x03 };

static bool GeosBreakResetKey(struct GeosDisk *Disk, uint8_t *Image, uint32_t Length)
{
   uint16_t Key = Disk->Target->ResetKey, Load = GeosWord(Image);
   uint32_t At = 2 + (uint32_t)(Key - Load);

   if (!Key) return true;
   if (Key < Load || At + sizeof GeosResetKey > Length || memcmp(Image + At, GeosResetKey, sizeof GeosResetKey))
   {
      return GeosFail(Disk, PSTR("the boot file's reset key is not where this version keeps it"));
   }
   Image[At] = 0;
   return true;
}

// The boot file as GEOS starts from it, load address first: the disk driver's transport replaced, any reset key
// broken, and the clock set to Now where the start-up code is one the target knows
FLASHMEM static inline bool GeosBuildBootImage(struct GeosDisk *Disk, const struct GeosDate *Now, uint8_t *Out, uint32_t Capacity,
                                      uint32_t *Length, bool *ClockSet)
{
   const struct GeosTarget *Target = Disk->Target;
   int32_t Driver;

   if (!Target->BootFile) return GeosFail(Disk, PSTR("this version has no boot file to start from"));
   if (!GeosReadFile(Disk, Target->BootFile, Out, Capacity, Length)) return false;
   if (*Length < 2 + GEOS_DRIVER_SIZE) return GeosFail(Disk, PSTR("the boot file is too short"));
   Driver = GeosFindDriver(Out + 2, *Length - 2);
   if (Driver < 0 || !GeosReplaceTransport(Out + 2 + Driver, Target->DiskDriver))
   {
      return GeosFail(Disk, PSTR("the boot file's disk driver is not one the TR can replace"));
   }
   if (!GeosBreakResetKey(Disk, Out, *Length)) return false;
   *ClockSet = GeosSetClock(Out + 2, *Length - 2, Target, Now);
   return true;
}

// One PRG that starts GEOS 64: the boot image placed where its load address puts it, behind a BASIC line that
// jumps to the boot entry
static inline bool GeosBuildBootPrg(struct GeosDisk *Disk, const struct GeosDate *Now, uint8_t *Out, uint32_t Capacity,
                                    uint32_t *Length, bool *ClockSet)
{
   const uint32_t Prologue = 2 + sizeof GeosBasicLine + sizeof GeosBootStub;
   uint32_t ImageSize, BodySize, Load, Place;

   if (!GeosBuildBootImage(Disk, Now, Out, Capacity, &ImageSize, ClockSet)) return false;
   BodySize = ImageSize - 2;
   Load = GeosWord(Out);
   Place = 2 + Load - GEOS_BASIC_START;
   if (Load < GEOS_BASIC_START + Prologue - 2 || Place + BodySize > Capacity || Load + BodySize > GEOS_MEMORY_END) return GeosFail(Disk, PSTR("the boot file does not fit in memory"));

   memmove(Out + Place, Out + 2, BodySize);
   memset(Out, 0, Place);
   GeosPutWord(Out, GEOS_BASIC_START);
   memcpy(Out + 2, GeosBasicLine, sizeof GeosBasicLine);
   memcpy(Out + 2 + sizeof GeosBasicLine, GeosBootStub, sizeof GeosBootStub);
   GeosPutWord(Out + Prologue - 2, Disk->Target->BootEntry);
   *Length = Place + BodySize;
   return true;
}

// The C128 boot ROM, its ROML chip then its ROMH chip, followed by the boot image it pulls through BOOT a page at a
// time. Image and ImageSize say where that image starts, without its load address, for TRBlockMount.
static inline bool GeosBuildC128Boot(struct GeosDisk *Disk, const struct GeosDate *Now, uint8_t *Out, uint32_t Capacity,
                                     const uint8_t **Image, uint32_t *ImageSize, bool *ClockSet)
{
   uint8_t *Roml = Out, *Romh = Out + GEOS128_BOOT_CHIP_SIZE, *File = Out + 2 * GEOS128_BOOT_CHIP_SIZE;
   uint32_t Length, Pages, Load;

   if (!GeosBuildBootImage(Disk, Now, File, Capacity - 2 * GEOS128_BOOT_CHIP_SIZE, &Length, ClockSet)) return false;
   Load = GeosWord(File);
   Pages = (Length - 2 + GEOS_SECTOR_SIZE - 1) / GEOS_SECTOR_SIZE;
   if (Load < GEOS128_SHARED_RAM_END || Load + Pages * GEOS_SECTOR_SIZE > GEOS128_IO_START)
   {
      return GeosFail(Disk, PSTR("the boot file does not fit in memory"));
   }

   memset(Out, 0, 2 * GEOS128_BOOT_CHIP_SIZE);
   memcpy(Roml, Geos128BootRoml, sizeof Geos128BootRoml);
   memcpy(Romh + GEOS128_BOOT_VECTORS, Geos128BootVectors, sizeof Geos128BootVectors);
   GeosPutWord(Roml + GEOS128_BOOT_PARAMS, Load);
   GeosPutWord(Roml + GEOS128_BOOT_PARAMS + 2, Pages);
   GeosPutWord(Roml + GEOS128_BOOT_PARAMS + 4, Disk->Target->BootEntry);
   *Image = File + 2;
   *ImageSize = Length - 2;
   return true;
}
