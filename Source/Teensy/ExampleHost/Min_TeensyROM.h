// SPDX-License-Identifier: MIT
//
// Build profile for the example third-party extension host.
//
// Deliberately close to Source/Teensy/VMBoot/Min_TeensyROM.h, because the
// difference is the point: what a host owes the build is this file and one
// .ino, and everything else it inherits from the minimal sketch it is overlaid
// onto. The one line that differs is FeatVMHost, which this host does not
// define -- it runs no modules, so it compiles none of the module runtime.
//
// It must be built by tools/build-firmware.mjs, which generates the linker
// script that puts the image in the extension slot and reserves the module's
// memory windows:
//
//   node tools/build-firmware.mjs --target tr-plus --host-sketch Source/Teensy/ExampleHost
//
// The stock IDE linker profile links for 0x60000000 and would produce an image
// the loader refuses (vm_host_slot_valid checks the boot base), so the guard
// below fails the build early rather than at install time.
#if !defined(VM_HOST_PROFILE) || !defined(USB_DISABLED)
#error "Build this image with tools/build-firmware.mjs --host-sketch"
#endif

#define MinimumBuild         //Must be defined for minimal build to identify in common files

// No module runtime, no Ethernet, no bank swapping. RAM_ImageSize still has to
// be something: the shared minimal sources declare a buffer of that size, and
// this host never fills it.
#define Num8kSwapBuffers  2
#define EthernetDeduction 0
#define MaxRAM_ImageSize  16
