# USBHost_t36 (vendored, pinned)

A copy of Paul Stoffregen's [USBHost_t36](https://github.com/PaulStoffregen/USBHost_t36) library, pinned to upstream commit [`f943c4b`](https://github.com/PaulStoffregen/USBHost_t36/commit/f943c4b1d4ffdea53b4fd007931453e22b970732) (2025-09-29). The main firmware builds against this copy instead of the one bundled with Teensyduino. MinimalBoot doesn't use USB host.

## Why

Teensyduino 1.60 shipped a rewrite of USBHost_t36's enumeration error handling (upstream [`fde482e`](https://github.com/PaulStoffregen/USBHost_t36/commit/fde482e) "Improve error handling" and [`1e2c91e`](https://github.com/PaulStoffregen/USBHost_t36/commit/1e2c91e) "Restructure enumeration to recover from errors"). Since then some USB devices are no longer recognized on the TeensyROM host port; going back to Teensyduino 1.59 fixes it. `f943c4b` is the last upstream commit before those two. Teensyduino 1.61 (the core this repo builds against) bundles the rewritten version, and 1.62 has no USBHost_t36 changes.

Compared with the copy bundled in 1.61, this one lacks CP2105 dual-serial support, the fix for a Raspberry Pi Pico-through-hub enumeration hang ([PJRC forum](https://forum.pjrc.com/index.php?threads/connecting-pico-to-teensy-4-1-usb-host-through-usb-hub-crashes-except-for-one-specific-hub-and-certain-ports.77469/)), and one new Serial example.

## How it's wired in

- `Teensy.ino` includes `"src/USBHost_t36/USBHost_t36.h"`, and `src/PN532/PN532_UHSU.h` includes `"../USBHost_t36/USBHost_t36.h"`.
- Don't write `#include <USBHost_t36.h>` (or a bare `"USBHost_t36.h"`) anywhere in the sketch. arduino-cli would then also compile Teensyduino's copy, and the link fails with multiple-definition errors.
- To confirm this copy is the one in use:
  - **`npm run build:*`:** build with `--keep-work` and check that the build root's `main/libraries/` has no `USBHost_t36` folder. The vendored sources compile under `main/sketch/src/USBHost_t36/` instead.
  - **Arduino IDE:** with verbose compile output on, there should be no `Using library USBHost_t36` line.
- Only the top-level `.cpp`/`.h` files and `utility/` are here, identical to upstream at `f943c4b` except for the one local modification listed below. `examples/` is left out on purpose: Arduino compiles every `.cpp` under a sketch's `src/`, so the example sources would be built into the firmware.

## Updating or removing it

The copy tracks tag `tr-pin-f943c4b` in the fork, [SensoriumEmbedded/USBHost_t36](https://github.com/SensoriumEmbedded/USBHost_t36).

- **To move to a fixed version:** replace this folder's contents from that commit (same file set, still no `examples/`) and update this README.
- **Once a Teensyduino release TeensyROM builds against has the fix upstream:** delete this folder and change the two includes back to `<USBHost_t36.h>`.

**Local modifications** (reapply these when replacing the contents):

- `utility/USBFilesystemFormatter.h`: `#include "USBHost_t36.h"` changed to `#include "../USBHost_t36.h"`. A quoted include is looked up next to the including file first, and inside `utility/` that finds nothing. As a normal library it then falls back to the library's own folder on the include path, but under a sketch's `src/` there is no such path. arduino-cli would then add Teensyduino's copy to find the header, and the link fails with multiple definitions.

## License

MIT, as stated in each file's header (Copyright Paul Stoffregen).
