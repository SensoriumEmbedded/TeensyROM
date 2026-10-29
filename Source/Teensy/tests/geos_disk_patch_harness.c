// SPDX-License-Identifier: MIT
// geos_disk_patch_harness IN OUT: the firmware's GEOS boot-disk patch, run on an image file.
// Exits 0 after writing OUT, 1 with the patcher's refusal on stderr, 3 if a refusal changed the image.
// geos_disk_patch_harness --inspect IN: prints what GeosInspect found, or exits 1 with its refusal.
#include <stdlib.h>

#include "../GEOS/GeosTargets.h"
#include "../GEOS/TRMouse128.h"

#define GEOS_INPUT_128 15

#define MAX_IMAGE 819200

// One byte past the largest image, so an oversized file reads as a size no format has
static uint8_t Image[MAX_IMAGE + 1], Original[MAX_IMAGE + 1];

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

static const struct GeosTarget Geos128Target =
   { "128 DESKTOP", TRMouse128Name, TRMouse128Code, TRMouse128Info, sizeof TRMouse128Code, GEOS_INPUT_128, NULL, 0, NULL, NULL, 0, 0 };

static int Inspect(struct GeosDisk *Disk)
{
   struct GeosBootDisk Found;

   if (!GeosInspect(Disk, &Found))
   {
      fprintf(stderr, "%s\n", Disk->Error);
      return 1;
   }
   printf("desktop=%d driver=%d\n", Found.HasDeskTop, Found.HasDriver);
   return 0;
}

int main(int argc, char **argv)
{
   const struct GeosDate Date = { 26, 10, 16, 12, 0 };
   struct GeosDisk Disk = { 0 };
   FILE *In, *Out;
   size_t Size;
   bool Inspecting;

   if (argc != 3) return 2;
   Inspecting = !strcmp(argv[1], "--inspect");
   In = fopen(Inspecting ? argv[2] : argv[1], "rb");
   if (!In) return 2;
   Size = fread(Image, 1, sizeof Image, In);
   fclose(In);
   memcpy(Original, Image, Size);

   Disk.Format = GeosFormatForSize(Size);
   Disk.Target = &Geos128Target;
   Disk.Context = Image;
   Disk.Read = ReadBlock;
   Disk.Write = WriteBlock;
   if (Inspecting) return Inspect(&Disk);
   if (!GeosInstallDriver(&Disk, &Date))
   {
      fprintf(stderr, "%s\n", Disk.Error);
      return memcmp(Original, Image, Size) ? 3 : 1;
   }

   Out = fopen(argv[2], "wb");
   if (!Out || fwrite(Image, 1, Size, Out) != Size) return 2;
   fclose(Out);
   return 0;
}
