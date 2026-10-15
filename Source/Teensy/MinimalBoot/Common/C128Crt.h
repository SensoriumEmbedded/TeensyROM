// SPDX-License-Identifier: MIT
#pragma once
#include <EEPROM.h>
#include <TimeLib.h>
#include "CrtChips.h"

// Included after IOHandlers.h and Common_Defs.h, ahead of the .ino code that calls it: it sets the
//  cart lines and ROM pointers those define.

extern StructCrtChip CrtChips[];
extern uint8_t NumCrtChips;

bool CrtIsC128 = false;
static C128CartMap C128Map;

// Record layout: tools/c128/README.md, "The boot clock".
static const uint8_t C128ClockTag[8] = {'T', 'R', 'C', 'L', 'O', 'C', 'K', 1};
static const uint8_t C128ClockLen = 7;
static uint8_t *C128Clock = NULL;
static const int32_t TimezoneStepSecs = 30 * 60;
static int32_t C128ClockLocalOffset;

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
   if (!MapC128Cart(CrtChips, NumCrtChips, &C128Map))
   {
      SendMsgPrintfln("C128 chip layout unsupported");
      return false;
   }
   MyMenuItem->ItemType = rtBinC128;
   SendMsgPrintfln(" C128 config");
   return true;
}

static uint8_t *FindC128Clock(const uint8_t *Image, uint16_t Mask)
{
   if (Image == NULL || ((uint32_t)Image & SwapSeekAddrMask) == SwapSeekAddrMask) return NULL;
   for (uint32_t Offset = 0; Offset + sizeof(C128ClockTag) + C128ClockLen <= (uint32_t)Mask + 1; Offset++)
      if (memcmp(Image + Offset, C128ClockTag, sizeof(C128ClockTag)) == 0)
         return (uint8_t *)Image + Offset + sizeof(C128ClockTag);
   return NULL;
}

static void WriteC128Clock()
{
   tmElements_t tm;
   breakTime(Teensy3Clock.get() + C128ClockLocalOffset, tm);
   const uint16_t Year = tmYearToCalendar(tm.Year);
   const uint8_t Clock[C128ClockLen] = {(uint8_t)(Year / 100), (uint8_t)(Year % 100),
      tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second};
   memcpy(C128Clock, Clock, C128ClockLen);
}

void UpdateC128Clock()
{
   static uint32_t LastWrite;
   if (C128Clock == NULL || millis() - LastWrite < 100) return;
   LastWrite = millis();
   WriteC128Clock();
}

static void LaunchC128Cart()
{
   SetGameDeassert;
   SetExROMDeassert;
   LOROM_Image = C128Map.ROML;
   HIROM_Image = C128Map.ROMH;
   LOROM_Mask = C128Map.ROMLMask;
   HIROM_Mask = C128Map.ROMHMask;

   C128Clock = FindC128Clock(C128Map.ROML, C128Map.ROMLMask);
   if (C128Clock == NULL) C128Clock = FindC128Clock(C128Map.ROMH, C128Map.ROMHMask);
   if (C128Clock == NULL) return;
   C128ClockLocalOffset = (int8_t)EEPROM.read(eepAdTimezone) * TimezoneStepSecs;
   WriteC128Clock();
}
