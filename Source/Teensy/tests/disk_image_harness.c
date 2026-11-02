// SPDX-License-Identifier: MIT
// disk_image_harness new KIND: formats a blank D64, D71 or D81 in memory, saves a 600-byte file on it, reads it
//   back, and prints "free=<before>,<after> blocks=<n> first=<track>/<sector> same=<0|1>".
// disk_image_harness size BYTES: prints the format and track count d64_open_size picks for a file of that size,
//   or "none".
#include <stdio.h>
#include <stdlib.h>

#include "../DiskImage/DiskImage.h"

static uint8_t Image[819200 + 3200];

static int NewDisk(const char *Kind)
{
   static const struct { const char *Kind; size_t Size; } Kinds[] = { { "d64", 174848 }, { "d71", 349696 }, { "d81", 819200 } };
   uint8_t Data[600], Back[sizeof Data + 1];
   d64_mem_t Mem;
   d64_t Disk;
   d64_wfile_t Out;
   d64_file_t In;
   d64_entry_t Entry;
   int Before, After, Got;
   size_t Size = 0;

   for (size_t i = 0; i < sizeof Kinds / sizeof Kinds[0]; i++)
   {
      if (!strcmp(Kind, Kinds[i].Kind)) Size = Kinds[i].Size;
   }
   if (!Size) return 2;
   for (size_t i = 0; i < sizeof Data; i++) Data[i] = (uint8_t)(i * 7 + 1);

   d64_mem_init(&Mem, Image, Size);
   if (d64_open_mem(&Disk, &Mem) || d64_format_blank(&Disk, "TEST", "TR")) return 1;
   Before = d64_blocks_free(&Disk);
   if (d64_create(&Disk, "HELLO", 2, 0, &Out) || d64_write(&Disk, &Out, Data, sizeof Data) != (int)sizeof Data) return 1;
   if (d64_close_write(&Disk, &Out)) return 1;
   After = d64_blocks_free(&Disk);
   if (d64_find(&Disk, "HELLO", &Entry) != 1 || d64_file_open(&Disk, &Entry, &In)) return 1;
   Got = d64_file_read(&Disk, &In, Back, sizeof Back);
   printf("free=%d,%d blocks=%u first=%u/%u same=%d\n", Before, After, Entry.blocks, Entry.track, Entry.sector,
          Got == (int)sizeof Data && !memcmp(Data, Back, sizeof Data));
   return 0;
}

static int SizeOf(const char *Bytes)
{
   static const char *const Names[] = { "d64", "d71", "d81" };
   d64_t Disk;

   if (d64_open_size(&Disk, NULL, NULL, (size_t)strtoul(Bytes, NULL, 10)))
   {
      printf("none\n");
      return 0;
   }
   printf("%s %u\n", Names[Disk.fmt], Disk.tracks);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc == 3 && !strcmp(argv[1], "new")) return NewDisk(argv[2]);
   if (argc == 3 && !strcmp(argv[1], "size")) return SizeOf(argv[2]);
   return 2;
}
