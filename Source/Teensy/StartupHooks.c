// Sketch-side implementations of the Teensyduino core's startup hooks.
//
// The core (cores/teensy4/startup.c) declares three weak, empty hook functions
// and calls them at fixed points while bringing the chip up, before setup()
// runs.  Defining one here replaces the core's empty default.  Anything that
// has to happen during boot rather than in setup() belongs in this file.
//
//   startup_early_hook    Before RAM is initialized.  Must be FLASHMEM; globals
//                         and most peripherals are not yet usable.
//   startup_middle_hook   After memory, clocks, and peripherals are initialized;
//                         before usb_init() and the C++ constructors.
//   startup_late_hook     After usb_init() and the core's USB enumeration delay;
//                         before the C++ constructors and setup().
//
// Only the hooks in use are defined; the core's defaults cover the rest.

#include "avr/pgmspace.h"

void MidiDevName_AppendUniqueID(void);

// Runs once, after RAM globals and peripherals are up and before the core
// starts USB.  Anything that must be in place before the host can see the
// device goes here.  No C++ objects exist yet.
FLASHMEM void startup_middle_hook(void)
{
   MidiDevName_AppendUniqueID();  //USB name strings must be final before usb_init()
}
