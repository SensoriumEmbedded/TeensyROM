// SPDX-License-Identifier: MIT
// The GEOS versions the TR sets up from a boot disk image the user puts on the SD card.
#pragma once
#include "GeosBoot.h"
#include "GeosDiskPatch.h"
#include "TRDriver.h"
#include "TRMouse64.h"

#define GEOS_INPUT_64  10
#define GEOS64_BOOT_FILE  "GEOBOOT"
#define GEOS64_BOOT_ENTRY 0x2f3c

#define GEOS128_BOOT_FILE  "GEOBOOT128"
#define GEOS128_BOOT_ENTRY 0x2f3c
#define GEOS128_RESET_KEY  0x6ff5

struct GeosLauncher
{
   const char *Title, *Folder;
   struct GeosTarget Target;
};

static const struct GeosDiskDriver TRDiskDriver64 =
{
   TRDriverCode, sizeof TRDriverCode, TR_DRIVER_BASE,
   TRDriverRelocations, sizeof TRDriverRelocations / sizeof TRDriverRelocations[0], TRDriverEntries,
};

static const struct GeosLauncher Geos64Launcher =
{
   "GEOS 64", "/GEOS64",
   { "DESK TOP", TRMouse64Name, TRMouse64Code, TRMouse64Info, sizeof TRMouse64Code, GEOS_INPUT_64,
     GEOS64_BOOT_FILE, GEOS64_BOOT_ENTRY, &TRDiskDriver64,
     Geos64ClockRuns, sizeof Geos64ClockRuns / sizeof Geos64ClockRuns[0], 0 },
};
