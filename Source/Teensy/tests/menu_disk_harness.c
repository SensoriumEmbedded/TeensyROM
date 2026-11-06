// SPDX-License-Identifier: MIT
// menu_disk_harness read KIND IMAGE TRACK SECTOR CAPACITY OUT: what the TR menu loads from a D64, D71 or D81 (KIND 0, 1
//   or 2) for the file starting at TRACK/SECTOR. Writes the bytes to OUT and prints "length=<n>", or prints "refused"
//   (not an image of that kind), "broken" or "too big".
// menu_disk_harness list KIND IMAGE LIMIT: the files the menu lists, one a line as "<type> <track>/<sector> <name>",
//   at most LIMIT of them, then "end" or "broken".
#include <stdio.h>
#include <stdlib.h>

#include "../DiskImage/MenuDisk.h"

static uint8_t Image[822400 + 1], Out[1 << 20];
static size_t Size;

static int ReadSector(void *Context, uint8_t Track, uint8_t Sector, uint8_t *Block)
{
   long Offset = d64_sector_offset((d64_t *)Context, Track, Sector);
   if (Offset < 0 || (size_t)Offset + D64_SECTOR > Size) return -1;
   memcpy(Block, Image + Offset, D64_SECTOR);
   return 0;
}

static int Left;

static bool Add(void *Context, const uint8_t *Name, uint8_t Type, uint8_t Track, uint8_t Sector)
{
   int Length = 16;

   (void)Context;
   if (!Left) return false;
   Left--;
   while (Length && Name[Length - 1] == 0xa0) Length--;
   printf("%02x %u/%u %.*s\n", Type, Track, Sector, Length, (const char *)Name);
   return true;
}

int main(int argc, char **argv)
{
   d64_t Disk;
   FILE *File;
   int32_t Length;
   bool Listing = argc == 5 && !strcmp(argv[1], "list");

   if ((!Listing && (argc != 8 || strcmp(argv[1], "read"))) || !(File = fopen(argv[3], "rb"))) return 2;
   Size = fread(Image, 1, sizeof Image, File);
   fclose(File);
   if (!MenuDiskOpen(&Disk, (enum MenuDiskKind)atoi(argv[2]), Size, ReadSector, &Disk))
   {
      printf("refused\n");
      return 0;
   }
   if (Listing)
   {
      Left = atoi(argv[4]);
      printf("%s\n", MenuDiskList(&Disk, Add, NULL) ? "broken" : "end");
      return 0;
   }
   Length = MenuDiskReadFile(&Disk, (uint8_t)atoi(argv[4]), (uint8_t)atoi(argv[5]), Out, (uint32_t)atol(argv[6]));
   if (Length == MENUDISK_BROKEN) printf("broken\n");
   else if (Length == MENUDISK_TOO_BIG) printf("too big\n");
   else
   {
      if (!(File = fopen(argv[7], "wb")) || fwrite(Out, 1, (size_t)Length, File) != (size_t)Length) return 2;
      fclose(File);
      printf("length=%d\n", (int)Length);
   }
   return 0;
}
