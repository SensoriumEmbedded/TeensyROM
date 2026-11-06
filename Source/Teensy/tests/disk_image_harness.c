// SPDX-License-Identifier: MIT
// disk_image_harness new KIND: formats a blank D64, D71 or D81 in memory, saves a 600-byte file on it, reads it
//   back, and prints "free=<before>,<after> blocks=<n> first=<track>/<sector> same=<0|1>".
// disk_image_harness size BYTES: prints the format and track count d64_open_size picks for a file of that size,
//   or "none".
// disk_image_harness wrap: a file whose last sector is past every track with room, appended to after a scratch
//   frees tracks earlier in the allocation order; prints "append=<rc> write=<rc> close=<rc> same=<0|1>".
#include <stdio.h>
#include <stdlib.h>

#include "../DiskImage/DiskImage.h"

static uint8_t Image[819200 + 3200];
static d64_mem_t Mem;

static int BlankD64(d64_t *Disk)
{
   memset(Image, 0, sizeof Image);
   d64_mem_init(&Mem, Image, 174848);
   if (d64_open_mem(Disk, &Mem) || d64_format_blank(Disk, "TEST", "TR")) return 1;
   return 0;
}

static void Fill(uint8_t *Data, size_t Size, uint8_t Seed)
{
   for (size_t i = 0; i < Size; i++) Data[i] = (uint8_t)(i * 7 + Seed);
}

static int Save(d64_t *Disk, const char *Name, const uint8_t *Data, size_t Size)
{
   d64_wfile_t Out;
   if (d64_create(Disk, Name, 2, 0, &Out) || d64_write(Disk, &Out, Data, Size) != (int)Size) return 1;
   return d64_close_write(Disk, &Out);
}

// 1 if the file reads back as Data, 0 if it differs, -1 if it is not there
static int Same(d64_t *Disk, const char *Name, const uint8_t *Data, size_t Size)
{
   static uint8_t Back[8192];
   d64_entry_t Entry;
   d64_file_t In;
   if (d64_find(Disk, Name, &Entry) != 1 || d64_file_open(Disk, &Entry, &In)) return -1;
   int Got = d64_file_read(Disk, &In, Back, sizeof Back);
   return Got == (int)Size && !memcmp(Data, Back, Size);
}

static void UseTrack(d64_t *Disk, uint8_t Track)
{
   uint8_t Nt, Ns;
   for (uint8_t s = 0; s < d64_sectors_in_track(Disk, Track); s++) d64_block_alloc(Disk, Track, s, &Nt, &Ns);
}

static int Wrap(void)
{
   uint8_t Data[1200];
   d64_t Disk;
   d64_wfile_t Out;
   d64_entry_t Entry;

   Fill(Data, sizeof Data, 1);
   if (BlankD64(&Disk)) return 1;
   for (uint8_t t = 1; t <= 35; t++) if (t != 18 && t != 26) UseTrack(&Disk, t);
   if (Save(&Disk, "LOG", Data, 600)) return 1;
   UseTrack(&Disk, 26);
   for (uint8_t t = 1; t <= 25; t++)
      if (t != 18) for (uint8_t s = 0; s < d64_sectors_in_track(&Disk, t); s++) d64_block_free(&Disk, t, s);
   if (d64_find(&Disk, "LOG", &Entry) != 1 || Entry.track != 26) return 1;

   int Append = d64_append(&Disk, "LOG", &Out), Write = 0, Close = 0;
   if (!Append) Write = d64_write(&Disk, &Out, Data + 600, 600);
   if (Write == 600) Close = d64_close_write(&Disk, &Out);
   printf("append=%d write=%d close=%d same=%d\n", Append, Write, Close, Same(&Disk, "LOG", Data, sizeof Data) == 1);
   return 0;
}

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
   if (argc == 2 && !strcmp(argv[1], "wrap")) return Wrap();
   return 2;
}
