// SPDX-License-Identifier: MIT
// geos_boot_harness 64|128 IN.d81 OUT OUT.d81 [HOUR MINUTE]: builds the GEOS 64 boot PRG, or the GEOS 128 boot ROM
// and the boot image behind it, from the disk, sets up its CONFIGURE files, sets the clock to 2026-10-21 at
// HOUR:MINUTE (15:07 if not given), and writes both. Prints "changed=<0|1> clock=<0|1> mouse=<0|1>", the last whether
// the TR mouse driver on the disk was brought up to date, and for GEOS 128
// " boot=<offset>,<size>" where BOOT's image lies in OUT; exits 1 with the refusal on stderr. REGS_PAGE (hex) is the
// block device's page the GEOS 128 boot ROM is built for, beside GeoRAM if not set.
#include <stdlib.h>

#include "../GEOS/GeosTargets.h"

#define MAX_IMAGE 819200
#define MAX_PRG   (128 * 1024)

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
   uint32_t Length, BootSize = 0;
   const uint8_t *Boot = NULL;
   const char *RegsPage = getenv("REGS_PAGE");
   bool Is128, Changed, Refreshed, ClockSet, Built;
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
   Disk.Target = Is128 ? &Geos128Launcher.Target : &Geos64Launcher.Target;
   Disk.Context = Image;
   Disk.Read = ReadBlock;
   Disk.Write = WriteBlock;
   Built = GeosRefreshDriver(&Disk, &Refreshed) && GeosPrepareConfigure(&Disk, Prg, sizeof Prg, &Changed) && (Is128 ? GeosBuildC128Boot(&Disk, &Now, RegsPage ? (uint8_t)strtol(RegsPage, NULL, 16) : GEOS_REGS_BESIDE_GEORAM,
                                     Prg, sizeof Prg, &Boot, &BootSize, &ClockSet)
                 : GeosBuildBootPrg(&Disk, &Now, Prg, sizeof Prg, &Length, &ClockSet));
   if (!Built)
   {
      fprintf(stderr, "%s\n", Disk.Error);
      return 1;
   }
   printf("changed=%d clock=%d mouse=%d", Changed, ClockSet, Refreshed);
   if (Is128)
   {
      Length = (uint32_t)(Boot - Prg) + BootSize;
      printf(" boot=%u,%u", (unsigned)(Boot - Prg), (unsigned)BootSize);
   }
   printf("\n");
   File = fopen(argv[3], "wb");
   fwrite(Prg, 1, Length, File);
   fclose(File);
   File = fopen(argv[4], "wb");
   fwrite(Image, 1, Size, File);
   fclose(File);
   return 0;
}
