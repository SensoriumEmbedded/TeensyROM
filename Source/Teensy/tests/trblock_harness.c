// SPDX-License-Identifier: MIT
// trblock_harness IMAGE OUT [ro|nobackend]: the block device's register side and disk image backend,
// driven from stdin one command a line, as the IO ISR and the main loop would drive them:
//   w OFFSET HEX   an IO write          r OFFSET   an IO read, printing the byte or "open"
//   svc            the main loop's service, printing 1 if a command ran
//   flushes        how many times the backend was asked to flush
// OUT receives the image when stdin ends.
#include <stdio.h>
#include <stdlib.h>

#include "../BlockDevice/TRBlockImage.h"

#define MAX_IMAGE 819200

static uint8_t Bytes[MAX_IMAGE];
static uint32_t Size, Flushes;

static bool ReadAt(void *Context, uint32_t Offset, uint8_t *Block)
{
   (void)Context;
   if (Offset + GEOS_SECTOR_SIZE > Size) return false;
   memcpy(Block, Bytes + Offset, GEOS_SECTOR_SIZE);
   return true;
}

static bool WriteAt(void *Context, uint32_t Offset, const uint8_t *Block)
{
   (void)Context;
   if (Offset + GEOS_SECTOR_SIZE > Size) return false;
   memcpy(Bytes + Offset, Block, GEOS_SECTOR_SIZE);
   return true;
}

static bool FlushAll(void *Context)
{
   (void)Context;
   Flushes++;
   return true;
}

int main(int argc, char **argv)
{
   FILE *In = argc >= 3 ? fopen(argv[1], "rb") : NULL;
   struct TRBlock Dev;
   struct TRBlockImage Image = { NULL, argc < 4 || strcmp(argv[3], "ro"), NULL, ReadAt, WriteAt, FlushAll };
   struct TRBlockBackend Backend;
   char Line[64];

   if (!In)
   {
      fprintf(stderr, "usage: trblock_harness IMAGE OUT [ro|nobackend]\n");
      return 2;
   }
   Size = fread(Bytes, 1, MAX_IMAGE, In);
   fclose(In);
   Image.Format = GeosFormatForSize(Size);
   Backend = TRBlockImageBackend(&Image);
   TRBlockReset(&Dev);
   const struct TRBlockBackend *Use = argc >= 4 && !strcmp(argv[3], "nobackend") ? NULL : &Backend;

   while (fgets(Line, sizeof Line, stdin))
   {
      unsigned Offset, Data;

      if (sscanf(Line, "w %x %x", &Offset, &Data) == 2)
      {
         if (TRBlockTakes(Offset)) TRBlockWriteAt(&Dev, Offset, Data);
      }
      else if (sscanf(Line, "r %x", &Offset) == 1)
      {
         int Read = TRBlockReadAt(&Dev, Offset);
         if (Read != TRBLOCK_UNSERVED) printf("%02x\n", Read);
         else printf("open\n");
      }
      else if (!strncmp(Line, "svc", 3)) printf("%d\n", TRBlockService(&Dev, Use));
      else if (!strncmp(Line, "flushes", 7)) printf("%u\n", Flushes);
   }
   FILE *Out = fopen(argv[2], "wb");
   fwrite(Bytes, 1, Size, Out);
   fclose(Out);
   return 0;
}
