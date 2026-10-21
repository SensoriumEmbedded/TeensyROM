// SPDX-License-Identifier: MIT
// The GEOS versions the TR sets up from a boot disk image the user puts on the SD card.
#pragma once
#include "GeosDiskPatch.h"
#include "TRMouse128.h"
#include "TRMouse64.h"

#define GEOS_INPUT_128 15
#define GEOS_INPUT_64  10
#define GEOS64_BOOT_FILE  "GEOBOOT"
#define GEOS64_BOOT_ENTRY 0x2f3c

struct GeosLauncher
{
   const char *Title, *Folder;
   struct GeosTarget Target;
};

static inline bool GeosBoots(const struct GeosLauncher *Launcher) { return Launcher->Target.BootFile != NULL; }

static const struct GeosLauncher Geos128Launcher =
{
   "GEOS 128", "/GEOS128",
   { "128 DESKTOP", TRMouse128Name, TRMouse128Code, TRMouse128Info, sizeof TRMouse128Code, GEOS_INPUT_128, NULL, 0 },
};

static const struct GeosLauncher Geos64Launcher =
{
   "GEOS 64", "/GEOS64",
   { "DESK TOP", TRMouse64Name, TRMouse64Code, TRMouse64Info, sizeof TRMouse64Code, GEOS_INPUT_64,
     GEOS64_BOOT_FILE, GEOS64_BOOT_ENTRY },
};
