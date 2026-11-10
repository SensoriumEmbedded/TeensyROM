// SPDX-License-Identifier: MIT

#include "TRMenuFiles/ROMs/C128PrgBoot.bin.h"

#define C128_BASIC_START   0x1c01
#define C128_BASIC_TOP     0xff00   // the MMU's registers start here in every bank configuration
#define C128PRG_ROML_SIZE  0x400    // ROML_SIZE in C128PrgBoot.asm
#define C128PRG_ROMH_SIZE  0x100
#define C128PRG_END_PARAM  0x21     // prgEnd, the first word of loaderRom in C128PrgBoot.asm

static_assert(sizeof C128PrgBoot_bin == C128PRG_ROML_SIZE + C128PRG_ROMH_SIZE, "C128PrgBoot.asm's layout changed");

static uint16_t PrgWord(const uint8_t *Bytes)
{
   return Bytes[0] | Bytes[1] << 8;
}

FLASHMEM bool IsC128BasicPrg(const uint8_t *Prg, uint32_t Size)
{
   if (Size < 4 || PrgWord(Prg) != C128_BASIC_START) return false;
   const uint16_t Link = PrgWord(Prg + 2);
   if (Link == 0) return true;
   const uint32_t LinkOffset = Link - C128_BASIC_START + 2;
   return Link > C128_BASIC_START + 4 && LinkOffset < Size && Prg[LinkOffset - 1] == 0;
}

static bool IsEmptyBasicPrg(const uint8_t *Prg)
{
   return PrgWord(Prg + 2) == 0;
}

static void ExitToC128Basic()
{
   SetGameDeassert;
   SetExROMDeassert;
   LOROM_Image = NULL;
   HIROM_Image = NULL;
}

// Prg is in RAM_Image. The boot ROM goes at the end of RAM_Image, where a program that fits under $FF00 never reaches.
static void StartC128Prg(uint8_t *Prg, uint32_t Size)
{
   uint8_t *Rom = RAM_Image + RAM_ImageSize - sizeof C128PrgBoot_bin;
   const uint16_t End = C128_BASIC_START + Size - 2;
   const uint8_t *BootRom = C128PrgBoot_bin;

   // GCC turns a memcpy of a whole const array it can see into a copy of the array in .rodata, which is DTCM
   asm("" : "+r"(BootRom));
   memcpy(Rom, BootRom, sizeof C128PrgBoot_bin);
   Rom[C128PRG_END_PARAM] = End & 0xff;
   Rom[C128PRG_END_PARAM + 1] = End >> 8;

   XferImage = Prg;
   XferSize = Size;
   StreamOffsetAddr = 0;

   SetGameDeassert;
   SetExROMDeassert;
   LOROM_Image = Rom;
   LOROM_Mask = C128PRG_ROML_SIZE - 1;
   HIROM_Image = Rom + C128PRG_ROML_SIZE;
   HIROM_Mask = C128PRG_ROMH_SIZE - 1;
}

// On a C64 an empty $1C01 program says it needs a C128, and any other runs as it always has, in C64 mode
FLASHMEM bool RunsInC128Mode(const uint8_t *Prg, uint32_t Size)
{
   return IsC128BasicPrg(Prg, Size) && (HostIsC128() || IsEmptyBasicPrg(Prg));
}

// An empty program returns to C128 BASIC, with the IO handler selected now as after a cartridge. Otherwise
// IOH_TeensyROM stays current to stream the program, and the boot ROM's rCtlRunningPRG selects the handler.
FLASHMEM void LaunchC128BasicPrg(uint8_t *Prg, uint32_t Size)
{
   if (!HostIsC128())
   {
      SendMsgPrintfln(PSTR("C128 BASIC needs a C128"));
      return;
   }
   if (C128_BASIC_START + Size - 2 > C128_BASIC_TOP)
   {
      SendMsgPrintfln(PSTR("Too big for C128 BASIC"));
      return;
   }
   SendMsgPrintfln(PSTR("Starting C128 mode"));
   BootComplete = true;
   doReset = true;
   if (IsEmptyBasicPrg(Prg))
   {
      ExitToC128Basic();
      IOHandlerSelectInit();
   }
   else StartC128Prg(Prg, Size);
}
