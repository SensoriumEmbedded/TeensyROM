// SPDX-License-Identifier: MIT
// The GEOS versions the TR sets up from a boot disk image the user puts on the SD card.
#pragma once
#include "GeosBoot.h"
#include "GeosDiskPatch.h"
#include "TRDriver.h"
#include "TRDriver128.h"
#include "TRMouse64.h"
#include "TRMouse128.h"

#define GEOS_INPUT_64  10
#define GEOS64_BOOT_FILE  "GEOBOOT"
#define GEOS64_BOOT_ENTRY 0x2f3c

#define GEOS_INPUT_128 15
#define GEOS128_BOOT_FILE  "GEOBOOT128"
#define GEOS128_BOOT_ENTRY 0x2f3c
#define GEOS128_RESET_KEY  0x6ff5

// What the TR starts the boot image on: a PRG on the C64, or the C128 boot ROM, which a C64 cannot run
enum GeosMachine { GeosC64, GeosC128 };

struct GeosLauncher
{
   const char *Title, *Folder;
   enum GeosMachine Machine;
   struct GeosTarget Target;
};

static const struct GeosDiskDriver TRDiskDriver64 =
{
   TRDriverCode, sizeof TRDriverCode, TR_DRIVER_BASE,
   TRDriverRelocations, sizeof TRDriverRelocations / sizeof TRDriverRelocations[0], TRDriverEntries,
};

static const struct GeosDiskDriver TRDiskDriver128 =
{
   TRDriver128Code, sizeof TRDriver128Code, TR_DRIVER128_BASE,
   TRDriver128Relocations, sizeof TRDriver128Relocations / sizeof TRDriver128Relocations[0], TRDriver128Entries,
};

static const struct GeosLauncher Geos64Launcher =
{
   "GEOS 64", "/GEOS64", GeosC64,
   { "DESK TOP", TRMouse64Name, TRMouse64Code, TRMouse64Info, sizeof TRMouse64Code, GEOS_INPUT_64,
     GEOS64_BOOT_FILE, GEOS64_BOOT_ENTRY, &TRDiskDriver64,
     Geos64ClockRuns, sizeof Geos64ClockRuns / sizeof Geos64ClockRuns[0], 0, "CONFIGURE" },
};

static const struct GeosLauncher Geos128Launcher =
{
   "GEOS 128", "/GEOS128", GeosC128,
   { "128 DESKTOP", TRMouse128Name, TRMouse128Code, TRMouse128Info, sizeof TRMouse128Code, GEOS_INPUT_128,
     GEOS128_BOOT_FILE, GEOS128_BOOT_ENTRY, &TRDiskDriver128,
     Geos128ClockRuns, sizeof Geos128ClockRuns / sizeof Geos128ClockRuns[0], GEOS128_RESET_KEY,
     "128 CONFIGURE" },
};
