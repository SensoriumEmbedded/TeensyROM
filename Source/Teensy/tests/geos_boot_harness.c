// SPDX-License-Identifier: MIT
// geos_boot_harness IN.d81 OUT.prg OUT.d81: retypes the disk's CONFIGURE files, builds the GEOS 64 boot PRG from
// it, sets the clock to 2026-10-21 15:07, and writes both. Prints "changed=<0|1> clock=<0|1>"; exits 1 with the refusal on stderr.
#include <stdlib.h>

#include "../GEOS/GeosBoot.h"
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
   const struct GeosDate Now = { 26, 10, 21, 15, 7 };
   struct GeosDisk Disk = { 0 };
   uint32_t Length;
   bool Changed;
   FILE *File;
   size_t Size;

   if (argc != 4 || !(File = fopen(argv[1], "rb"))) return 2;
   Size = fread(Image, 1, sizeof Image, File);
   fclose(File);

   Disk.Format = GeosFormatForSize(Size);
   Disk.Target = &Geos64Launcher.Target;
   Disk.Context = Image;
   Disk.Read = ReadBlock;
   Disk.Write = WriteBlock;
   if (!GeosBuildBootPrg(&Disk, Prg, sizeof Prg, &Length) || !GeosDisableConfigure(&Disk, &Changed))
   {
      fprintf(stderr, "%s\n", Disk.Error);
      return 1;
   }
   printf("changed=%d clock=%d\n", Changed, GeosSetClock(Prg, Length, &Now));
   File = fopen(argv[2], "wb");
   fwrite(Prg, 1, Length, File);
   fclose(File);
   File = fopen(argv[3], "wb");
   fwrite(Image, 1, Size, File);
   fclose(File);
   return 0;
}
