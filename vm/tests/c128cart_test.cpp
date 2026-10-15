// SPDX-License-Identifier: MIT
// Which chip layouts a "C128 CARTRIDGE" .crt may have, and where each chip lands.
#include <cassert>
#include "../../Source/Teensy/MinimalBoot/Common/CrtChips.h"

static uint8_t RomA[1], RomB[1];

static StructCrtChip chip(uint8_t *Rom, uint16_t Load, uint16_t Size, uint16_t Bank = 0)
{
   return StructCrtChip{Rom, Load, Size, Bank};
}

static bool map(const StructCrtChip *Chips, uint8_t Num, C128CartMap *Out)
{
   *Out = C128CartMap{RomA, RomA, 0, 0};
   return MapC128Cart(Chips, Num, Out);
}

int main() {
   C128CartMap Map;

   const StructCrtChip Lo8k[] = {chip(RomA, 0x8000, 0x2000)};
   assert(map(Lo8k, 1, &Map));
   assert(Map.ROML == RomA && Map.ROMLMask == 0x1fff && Map.ROMH == NULL);

   const StructCrtChip Lo16k[] = {chip(RomA, 0x8000, 0x4000)};
   assert(map(Lo16k, 1, &Map));
   assert(Map.ROML == RomA && Map.ROMLMask == 0x3fff && Map.ROMH == NULL);

   const StructCrtChip Both[] = {chip(RomB, 0xC000, 0x4000), chip(RomA, 0x8000, 0x2000)};
   assert(map(Both, 2, &Map));
   assert(Map.ROML == RomA && Map.ROMLMask == 0x1fff);
   assert(Map.ROMH == RomB && Map.ROMHMask == 0x3fff);

   const StructCrtChip HiOnly[] = {chip(RomB, 0xC000, 0x2000)};
   assert(map(HiOnly, 1, &Map));
   assert(Map.ROML == NULL && Map.ROMH == RomB && Map.ROMHMask == 0x1fff);

   // The load address TeensyROM once required, and nowhere else in the spec.
   const StructCrtChip Old0000[] = {chip(RomA, 0x0000, 0x2000)};
   assert(!map(Old0000, 1, &Map));

   const StructCrtChip TwoLo[] = {chip(RomA, 0x8000, 0x2000), chip(RomB, 0x8000, 0x2000)};
   assert(!map(TwoLo, 2, &Map));
   const StructCrtChip TwoHi[] = {chip(RomA, 0xC000, 0x2000), chip(RomB, 0xC000, 0x2000)};
   assert(!map(TwoHi, 2, &Map));

   const StructCrtChip Banked[] = {chip(RomA, 0x8000, 0x4000, 1)};
   assert(!map(Banked, 1, &Map));

   const StructCrtChip Small[] = {chip(RomA, 0x8000, 0x1000)};
   assert(!map(Small, 1, &Map));
   const StructCrtChip Big[] = {chip(RomA, 0x8000, 0x8000)};
   assert(!map(Big, 1, &Map));

   const StructCrtChip A000[] = {chip(RomA, 0xA000, 0x2000)};
   assert(!map(A000, 1, &Map));
   const StructCrtChip E000[] = {chip(RomA, 0xE000, 0x2000)};
   assert(!map(E000, 1, &Map));

   assert(!map(Lo8k, 0, &Map));
   return 0;
}
