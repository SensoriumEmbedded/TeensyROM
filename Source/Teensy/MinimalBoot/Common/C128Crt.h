// SPDX-License-Identifier: MIT
#pragma once
#include "CrtChips.h"

// Included after IOHandlers.h and Common_Defs.h, ahead of the .ino code that calls it: it sets the
//  cart lines and ROM pointers those define.

bool CrtIsC128 = false;
static C128CartMap C128Map;

static bool DetectC128Crt(const uint8_t *CrtHeader)
{
   CrtIsC128 = (memcmp(CrtHeader, "C128 CARTRIDGE", 14) == 0);
   return CrtIsC128;
}

static bool C128HWTypeSupported(uint16_t HWType)
{  //C128 crts number their HW types separately: 1 is Warpspeed128, not Action Replay
   if (!CrtIsC128 || HWType == Cart_Generic) return true;
   SendMsgPrintfln("Unsupported C128 HW Type");
   return false;
}

// The C128 half of SetTypeFromCRT; keeps the map for LaunchC128Cart.
static bool SetC128Type(StructMenuItem *MyMenuItem)
{
#if defined(MinimumBuild) && !defined(VM_HOST_PROFILE)
   // C128 keeps the existing non-EasyFlash limit even in the wider loader.
   if (NumCrtChips > MAX_LEGACY_CRT_CHIPS ||
       !MapC128Cart(CrtChips, (uint8_t)NumCrtChips, &C128Map))
#else
   if (!MapC128Cart(CrtChips, NumCrtChips, &C128Map))
#endif
   {
      SendMsgPrintfln("C128 chip layout unsupported");
      return false;
   }
   MyMenuItem->ItemType = rtBinC128;
   SendMsgPrintfln(" C128 config");
   return true;
}

static void LaunchC128Cart()
{
   SetGameDeassert;
   SetExROMDeassert;
   LOROM_Image = C128Map.ROML;
   HIROM_Image = C128Map.ROMH;
   LOROM_Mask = C128Map.ROMLMask;
   HIROM_Mask = C128Map.ROMHMask;
}

// A C128 cart the TR built in memory rather than read from a crt, launched as LaunchC128Cart launches one
static inline void SetC128Image(const uint8_t *ROML, const uint8_t *ROMH, uint16_t ChipSize)
{
   C128Map.ROML = ROML;
   C128Map.ROMH = ROMH;
   C128Map.ROMLMask = ChipSize - 1;
   C128Map.ROMHMask = ChipSize - 1;
}
