// SPDX-License-Identifier: MIT
// The remote file commands and the menu both decide, from this one predicate,
// whether a write to storage has invalidated the listing the C64 is showing.
// Answering "yes" too often costs a refused launch; answering it when the menu
// cannot be rebuilt, or too rarely, is what the firmware must not do.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#define FLASHMEM
#include "../../Source/Teensy/MinimalBoot/Common/Menu_Regs.h"
#include "../../Source/Teensy/LoadedListing.h"

using namespace LoadedListing;

int main(){
    // The protocol's storage byte, in the menu's own device numbering.
    assert(storageDevice(0)==rmtUSBDrive);
    assert(storageDevice(1)==rmtSD);
    assert(storageDevice(255)==rmtSD);

    // Only a drive menu showing a real directory can be rebuilt.
    assert(reloadable(rmtSD,"/"));
    assert(reloadable(rmtSD,"/Games"));
    assert(reloadable(rmtUSBDrive,"/Games"));
    assert(!reloadable(rmtTeensy,"/"));            // built-in menu, not a drive
    assert(!reloadable(rmtSD,"/Games/disk.d64*")); // mounted image, not a directory
    assert(!reloadable(rmtSD,""));
    assert(!reloadable(rmtSD,"Games"));
    assert(!reloadable(rmtSD,nullptr));

    // A write into the loaded directory invalidates it; one beside it does not.
    assert(invalidatedBy("/Games/New.prg",rmtSD,rmtSD,"/Games"));
    assert(invalidatedBy("/games/NEW.PRG",rmtSD,rmtSD,"/Games"));   // FAT is case-insensitive
    assert(!invalidatedBy("/Other/New.prg",rmtSD,rmtSD,"/Games"));
    assert(!invalidatedBy("/GamesOther/New.prg",rmtSD,rmtSD,"/Games"));
    assert(!invalidatedBy("/Games",rmtSD,rmtSD,"/Games"));

    // A deeper write creates entries in the loaded directory as it goes, so it
    // counts: PostFileCommand makes every missing directory on the path.
    assert(invalidatedBy("/Games/Sub/New.prg",rmtSD,rmtSD,"/Games"));

    // At the root, every absolute path is inside the listing except the root.
    assert(invalidatedBy("/New.prg",rmtSD,rmtSD,"/"));
    assert(invalidatedBy("/Games/New.prg",rmtSD,rmtSD,"/"));
    assert(!invalidatedBy("/",rmtSD,rmtSD,"/"));

    // Writing to the other drive leaves this listing alone.
    assert(!invalidatedBy("/Games/New.prg",rmtUSBDrive,rmtSD,"/Games"));
    assert(!invalidatedBy("/Games/New.prg",rmtSD,rmtUSBDrive,"/Games"));

    // Nothing reloadable() refuses may be reported as invalidated, or the menu
    // would arm a rebuild it cannot perform.
    assert(!invalidatedBy("/Games/New.prg",rmtSD,rmtTeensy,"/"));
    assert(!invalidatedBy("/Games/disk.d64",rmtSD,rmtSD,"/Games/disk.d64*"));
    assert(!invalidatedBy("/New.prg",rmtSD,rmtSD,""));
    assert(!invalidatedBy("/New.prg",rmtSD,rmtSD,nullptr));
    assert(!invalidatedBy("New.prg",rmtSD,rmtSD,"/"));
    assert(!invalidatedBy(nullptr,rmtSD,rmtSD,"/"));

    puts("PASS: loaded-listing invalidation; device and root cases, prefix look-alikes, deeper writes, "
         "mounted images, the built-in menu and malformed paths");
}
