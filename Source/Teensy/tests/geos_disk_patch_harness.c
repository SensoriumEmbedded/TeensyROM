// SPDX-License-Identifier: MIT
// geos_disk_patch_harness IN OUT: the firmware's GEOS boot-disk patch, run on an image file.
// Exits 0 after writing OUT, 1 with the patcher's refusal on stderr.
#include <stdlib.h>

#include "../GEOS/GeosDiskPatch.h"

#define MAX_IMAGE 819200

static uint8_t Image[MAX_IMAGE];

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
   const struct GeosDate Date = { 26, 10, 16, 12, 0 };
   struct GeosDisk Disk = { 0 };
   FILE *In, *Out;
   size_t Size;

   if (argc != 3) return 2;
   In = fopen(argv[1], "rb");
   if (!In) return 2;
   Size = fread(Image, 1, sizeof Image, In);
   fclose(In);

   Disk.Format = GeosFormatForSize(Size);
   Disk.Context = Image;
   Disk.Read = ReadBlock;
   Disk.Write = WriteBlock;
   if (!GeosInstallMouseDriver(&Disk, &Date))
   {
      fprintf(stderr, "%s\n", Disk.Error);
      return 1;
   }

   Out = fopen(argv[2], "wb");
   if (!Out || fwrite(Image, 1, Size, Out) != Size) return 2;
   fclose(Out);
   return 0;
}
