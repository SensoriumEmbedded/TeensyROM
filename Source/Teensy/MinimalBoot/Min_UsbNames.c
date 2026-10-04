// SPDX-License-Identifier: MIT
//
// MinimalBoot's USB name strings and its startup hook: the same strings as the main
// image (Common/UsbNames.c), filled in at the same point, so both images present one
// identical USB device.  Main's equivalent is StartupHooks.c.
//
// Every file in this folder is also built into the extension image, which has USB
// compiled out and may define its own startup hooks, so all of it is left out there.
// Must stay a .c file (see UsbNames.c).

#if !defined(USB_DISABLED)

#include "Common/UsbNames.c"

// Runs once, after RAM globals and peripherals are up and before the core starts
// USB (see Source/Teensy/StartupHooks.c for the core's three hooks).
FLASHMEM void startup_middle_hook(void)
{
   UsbNames_AppendUniqueID();  //USB name strings must be final before usb_init()
}

#endif
