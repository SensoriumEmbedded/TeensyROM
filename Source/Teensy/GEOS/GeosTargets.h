// SPDX-License-Identifier: MIT
// The GEOS versions the TR sets up from a boot disk image the user puts on the SD card.
#pragma once
#include "GeosDiskPatch.h"
#include "TRMouse128.h"

#define GEOS_INPUT_128 15

struct GeosLauncher
{
   const char *Title, *Folder;
   struct GeosTarget Target;
};

static const struct GeosLauncher Geos128Launcher =
{
   "GEOS 128", "/GEOS128",
   { "128 DESKTOP", TRMouse128Name, TRMouse128Code, TRMouse128Info, sizeof TRMouse128Code, GEOS_INPUT_128 },
};
