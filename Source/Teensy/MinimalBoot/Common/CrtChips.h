// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

// One CHIP packet of a loaded .crt.  Pure, so the native tests can hold the layout rules below.
struct StructCrtChip
{
   uint8_t *ChipROM;
   uint16_t LoadAddress;
   uint16_t ROMSize;
   uint16_t BankNum;
};

struct C128CartMap
{
   const uint8_t *ROML, *ROMH;
   uint16_t ROMLMask, ROMHMask;
};

// Generic C128 cart (VICE CRT spec, C128 type 0): ROML is $8000-$BFFF and ROMH $C000-$FFFF, each
//  one 8k (mirrored) or 16k chip.
static inline bool MapC128Cart(const StructCrtChip *Chips, uint8_t NumChips, C128CartMap *Map)
{
   const StructCrtChip *ROML = NULL, *ROMH = NULL;
   for (uint8_t ChipNum = 0; ChipNum < NumChips; ChipNum++)
   {
      const StructCrtChip *Chip = &Chips[ChipNum];
      if (Chip->BankNum != 0 || (Chip->ROMSize != 0x2000 && Chip->ROMSize != 0x4000)) return false;

      const StructCrtChip **Slot;
      if (Chip->LoadAddress == 0x8000) Slot = &ROML;
      else if (Chip->LoadAddress == 0xC000) Slot = &ROMH;
      else return false;

      if (*Slot != NULL) return false;
      *Slot = Chip;
   }
   if (ROML == NULL && ROMH == NULL) return false;

   Map->ROML = ROML ? ROML->ChipROM : NULL;
   Map->ROMH = ROMH ? ROMH->ChipROM : NULL;
   Map->ROMLMask = ROML ? ROML->ROMSize - 1 : 0x1fff;
   Map->ROMHMask = ROMH ? ROMH->ROMSize - 1 : 0x1fff;
   return true;
}
