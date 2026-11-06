// SPDX-License-Identifier: MIT
// disk_image_harness new KIND: formats a blank D64, D71 or D81 in memory, saves a 600-byte file on it, reads it
//   back, and prints "free=<before>,<after> blocks=<n> first=<track>/<sector> same=<0|1>".
// disk_image_harness size BYTES: prints the format and track count d64_open_size picks for a file of that size,
//   or "none".
// disk_image_harness wrap: a file whose last sector is past every track with room, appended to after a scratch
//   frees tracks earlier in the allocation order; prints "append=<rc> write=<rc> close=<rc> same=<0|1>".
// disk_image_harness closefail new|append|copy N: the Nth sector write of d64_close_write fails; prints
//   "close=<rc> free=<before>,<after> files=<n> old=<blocks>,<0|1>" (old: the file an append or copy started from).
// disk_image_harness replace fit|full|locked: "@:" over a file, with room for both, without room until the old one's
//   blocks are freed, or over a locked file; prints "create=<rc> write=<rc> close=<rc> free=<before>,<after> files=<n> data=<old|new|none>".
#include <stdio.h>
#include <stdlib.h>

#include "../DiskImage/DiskImage.h"

static uint8_t Image[819200 + 3200];
static d64_mem_t Mem;
static d64_write_fn MemWrite;
static int FailIn;   // the write this many writes from now fails; 0 = none

static int CountedWrite(void *Ctx, uint8_t Track, uint8_t Sector, const uint8_t *Buf)
{
   if (FailIn && --FailIn == 0) return -1;
   return MemWrite(Ctx, Track, Sector, Buf);
}

static int BlankD64(d64_t *Disk)
{
   memset(Image, 0, sizeof Image);
   d64_mem_init(&Mem, Image, 174848);
   if (d64_open_mem(Disk, &Mem) || d64_format_blank(Disk, "TEST", "TR")) return 1;
   MemWrite = Disk->write;
   Disk->write = CountedWrite;
   FailIn = 0;
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

static int Files(d64_t *Disk)
{
   d64_dir_t It;
   d64_entry_t Entry;
   int n = 0;
   if (d64_dir_first(Disk, &It)) return -1;
   while (d64_dir_next(Disk, &It, &Entry) > 0) n += Entry.type != 0;
   return n;
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

static int CloseFail(const char *How, int Nth)
{
   static const char *const Src[] = { "OLD" };
   uint8_t Data[600];
   d64_t Disk;
   d64_wfile_t Out;
   d64_entry_t Entry;
   int Before, Close;

   Fill(Data, sizeof Data, 3);
   if (BlankD64(&Disk) || Save(&Disk, "OLD", Data, 400)) return 1;
   Before = d64_blocks_free(&Disk);
   if (!strcmp(How, "new"))
   {
      if (d64_create(&Disk, "NEW", 2, 0, &Out) || d64_write(&Disk, &Out, Data, 10) != 10) return 1;
      FailIn = Nth;
      Close = d64_close_write(&Disk, &Out);
   }
   else if (!strcmp(How, "append"))
   {
      if (d64_append(&Disk, "OLD", &Out) || d64_write(&Disk, &Out, Data + 400, 200) != 200) return 1;
      FailIn = Nth;
      Close = d64_close_write(&Disk, &Out);
   }
   else if (!strcmp(How, "copy"))
   {
      FailIn = Nth + 4;   // before the close: d64_create's BAM and directory writes, d64_write's BAM write and full sector
      Close = d64_copy(&Disk, "B", Src, 1);
   }
   else return 2;
   FailIn = 0;
   int Old = Same(&Disk, "OLD", Data, 400);
   d64_find(&Disk, "OLD", &Entry);
   printf("close=%d free=%d,%d files=%d old=%u,%d\n", Close, Before, d64_blocks_free(&Disk), Files(&Disk), Entry.blocks,
          Old == 1 && (Entry.type & 0x80));
   return 0;
}

static int Replace(const char *How)
{
   static uint8_t Old[2540], New[5080];
   d64_t Disk;
   d64_wfile_t Out;
   uint8_t Nt, Ns;
   int Before, Create, Write = 0, Close = 0;

   Fill(Old, sizeof Old, 5);
   Fill(New, sizeof New, 9);
   if (BlankD64(&Disk) || Save(&Disk, "A", Old, sizeof Old)) return 1;
   if (!strcmp(How, "full"))
   {
      // 15 blocks free: room for the 20-block new file only once the old file's 10 are freed
      for (uint8_t t = 1; t <= 35; t++)
         for (uint8_t s = 0; t != 18 && s < d64_sectors_in_track(&Disk, t) && d64_blocks_free(&Disk) > 15; s++)
            d64_block_alloc(&Disk, t, s, &Nt, &Ns);
   }
   else if (!strcmp(How, "locked"))
   {
      if (d64_lock(&Disk, "A", 1) != 1) return 1;
   }
   else if (strcmp(How, "fit")) return 2;
   Before = d64_blocks_free(&Disk);
   Create = d64_create(&Disk, "A", 2, 1, &Out);
   if (!Create) Write = d64_write(&Disk, &Out, New, sizeof New);
   if (!Create && Write == (int)sizeof New) Close = d64_close_write(&Disk, &Out);
   int IsOld = Same(&Disk, "A", Old, sizeof Old), IsNew = Same(&Disk, "A", New, sizeof New);
   printf("create=%d write=%d close=%d free=%d,%d files=%d data=%s\n", Create, Write, Close, Before, d64_blocks_free(&Disk),
          Files(&Disk), IsNew == 1 ? "new" : IsOld == 1 ? "old" : "none");
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
   if (argc == 4 && !strcmp(argv[1], "closefail")) return CloseFail(argv[2], atoi(argv[3]));
   if (argc == 3 && !strcmp(argv[1], "replace")) return Replace(argv[2]);
   return 2;
}
