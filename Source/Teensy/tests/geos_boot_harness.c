// SPDX-License-Identifier: MIT
// geos_boot_harness 64|128 IN.d81 OUT OUT.d81 [HOUR MINUTE]: builds the GEOS 64 boot PRG, or the GEOS 128 boot
// image, from the disk, retypes its CONFIGURE files, sets the clock to 2026-10-21 at HOUR:MINUTE (15:07 if not
// given), and writes both. Prints "changed=<0|1> clock=<0|1>"; exits 1 with the refusal on stderr.
#include <stdlib.h>

#include "../GEOS/GeosTargets.h"
#include "../GEOS/TRMouse128.h"

#define MAX_IMAGE 819200
#define MAX_PRG   (128 * 1024)
#define GEOS_INPUT_128 15

// GEOS 128's boot facts, on GEOS 64's disk driver until there is a 128 one: the transport swap is the same
static const struct GeosTarget Geos128Boot =
{
   "128 DESKTOP", TRMouse128Name, TRMouse128Code, TRMouse128Info, sizeof TRMouse128Code, GEOS_INPUT_128,
   GEOS128_BOOT_FILE, GEOS128_BOOT_ENTRY, &TRDiskDriver64,
   Geos128ClockRuns, sizeof Geos128ClockRuns / sizeof Geos128ClockRuns[0], GEOS128_RESET_KEY,
};

static uint8_t Image[MAX_IMAGE + 1], Prg[MAX_PRG];

static bool ReadBlock(void *Context, uint32_t Offset, uint8_t *Block)
{
   memcpy(Block, (uint8_t *)Context + Offset, GEOS_SECTOR_SIZE);
   return true;
}

static bool WriteBlock(void *Context, uint32_t Offset, const uint8_t *Block)
{
   memcpy((uint8_t *)Context + Offset, Block, GEOS_SECTOR_SIZE);
   return true;
}

int main(int argc, char **argv)
{
   struct GeosDate Now = { 26, 10, 21, 15, 7 };
   struct GeosDisk Disk = { 0 };
   uint32_t Length;
   bool Is128, Changed, ClockSet, Built;
   FILE *File;
   size_t Size;

   if ((argc != 5 && argc != 7) || !(File = fopen(argv[2], "rb"))) return 2;
   Is128 = !strcmp(argv[1], "128");
   if (argc == 7)
   {
      Now.Hour = atoi(argv[5]);
      Now.Minute = atoi(argv[6]);
   }
   Size = fread(Image, 1, sizeof Image, File);
   fclose(File);

   Disk.Format = GeosFormatForSize(Size);
   Disk.Target = Is128 ? &Geos128Boot : &Geos64Launcher.Target;
   Disk.Context = Image;
   Disk.Read = ReadBlock;
   Disk.Write = WriteBlock;
   Built = Is128 ? GeosBuildBootImage(&Disk, &Now, Prg, sizeof Prg, &Length, &ClockSet)
                 : GeosBuildBootPrg(&Disk, &Now, Prg, sizeof Prg, &Length, &ClockSet);
   if (!Built || !GeosDisableConfigure(&Disk, &Changed))
   {
      fprintf(stderr, "%s\n", Disk.Error);
      return 1;
   }
   printf("changed=%d clock=%d\n", Changed, ClockSet);
   File = fopen(argv[3], "wb");
   fwrite(Prg, 1, Length, File);
   fclose(File);
   File = fopen(argv[4], "wb");
   fwrite(Image, 1, Size, File);
   fclose(File);
   return 0;
}
