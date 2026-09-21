// SPDX-License-Identifier: MIT
#pragma once
#include "VMHostABI.h"

// Why the last extension launch ended the way it did.
//
// The extension image is built USB_DISABLED, because the USB stack's buffers
// live in DMAMEM and collide with the guest RAM2 arena (see the note in
// tools/lib/extension-image.mjs). So it has no serial, and its only exit is a
// reset back into the stock menu: a failure inside it is otherwise completely
// silent, and looks from the couch like "junk on screen, then the menu came
// back". It leaves one record here instead, in the cache line directly below
// Teensy's own CrashReport at the top of RAM2, which survives the soft reset.
// The main image collects it on the way back up and shows it on the C64 menu.
//
// Registry-level failures never get this far: VmRegistry::tryLaunch runs in the
// main image, where SendMsgPrintfln already works, and only reboots on success.
#ifndef MinimumBuild
// Defined in FileParsers.ino, and declared the same way IOH_TeensyROM.c does.
extern void SendMsgPrintfln(const char *Fmt, ...);
#endif
namespace VmFail {
// One code per exit point, in the order they can be reached. Ok is written on
// the single path that hands the machine to the C64 client; anything the module
// itself has to say after that travels over IO2 to the client, not through here.
enum : uint8_t {
    // Stamped before the entry point is called, because after that the arena
    // it sits in belongs to the module. A fault inside vm_entry therefore
    // reads back as Ok rather than as Entered.
    Ok            = 0x00,  // handed to the module; nothing refused it after
    Entered       = 0x01,  // minimal is about to jump to the extension image
    NoImage       = 0x02,  // ...but the top flash slot holds no valid image
    Faulted       = 0x03,  // Ok, but the core recorded a fault (promoteFault)
    SdInit        = 0x10,  // SD card would not initialise (detail = attempts)
    LaunchRecord  = 0x11,  // /VMS/launch.vml missing, short or corrupt
    Manifest      = 0x12,  // manifest.vmi unreadable or malformed
    ManifestCrc   = 0x13,  // manifest changed since the main image preflighted it
    ClientOpen    = 0x14,  // client cartridge would not open
    ClientHeader  = 0x15,  // not a 16 KiB C64 EasyFlash cartridge
    ClientBank    = 0x16,  // CHIP header or bank payload bad (detail = bank)
    Descriptor    = 0x17,  // third CHIP is not a valid VMH1 descriptor
    ClientCrc     = 0x18,  // client banks do not match the descriptor CRC
    ModuleLoad    = 0x20,  // module image refused (detail = VMHost failure code)
};
// Layout, magic and address are the published host contract (VMHostABI.h), so
// that a third-party host writes a record this image can read.
using Record = VmFailRecord;
enum : uint32_t { Magic = VM_FAIL_MAGIC };
static constexpr uint32_t base = VM_FAIL_BASE;

#if defined(__arm__)
static inline Record *slot() { return reinterpret_cast<Record *>(base); }
#else
// Host conformance builds have no RAM2 behind that address, so set() and take()
// run against ordinary memory.
inline Record hostSlot;
static inline Record *slot() { return &hostSlot; }
#endif

// RAM2 is cached and the reset that follows does not write the cache back, so
// the record has to be pushed out to physical RAM by hand -- exactly what the
// core does for its own crash report in startup.c.
static inline void set(uint8_t code, uint32_t detail = 0) {
    Record *r = slot();
    vm_fail_fill(*r, code, detail);
    arm_dcache_flush_delete(r, sizeof *r);
}

static inline bool intact(const Record *r) { return vm_fail_intact(r); }

// Reads the record and clears it, so a stale reason cannot be reported twice.
static inline bool take(Record &out) {
    Record *r = slot();
    arm_dcache_delete(r, sizeof *r);
    const bool valid = intact(r);
    if (valid) out = *r;
    r->magic = 0;
    arm_dcache_flush_delete(r, sizeof *r);
    return valid;
}

// Runs in the minimal image, where the reset after a fault lands. Validity
// only: printing is what clears the report, and the main image prints it at
// the end of its setup(), after USB has had time to enumerate.
static inline void promoteFault(bool faulted) {
    if (!faulted) return;

    Record *r = slot();
    arm_dcache_delete(r, sizeof *r);
    if (r->code == Ok && intact(r)) set(Faulted);
}

// Short enough for the C64's message window.
static inline const char *describe(uint8_t code) {
    switch (code) {
        case Ok:           return "handed off to client";
        case Entered:      return "image did not start";
        case NoImage:      return "no extension image installed";
        case Faulted:      return "extension faulted";
        case SdInit:       return "SD card init failed";
        case LaunchRecord: return "launch record unreadable";
        case Manifest:     return "manifest unreadable";
        case ManifestCrc:  return "manifest changed";
        case ClientOpen:   return "client CRT missing";
        case ClientHeader: return "client CRT header bad";
        case ClientBank:   return "client CRT bank bad";
        case Descriptor:   return "client descriptor bad";
        case ClientCrc:    return "client CRC mismatch";
        case ModuleLoad:   return "module refused";
        default:           return "unknown";
    }
}

#ifndef MinimumBuild
// Main image only. The record has to be collected before anything can allocate
// over it -- the stock heap runs to the top of RAM2 -- but it cannot be shown
// until the C64 is inside a WaitForTR* loop, which is the only time the menu
// reads messages. So capture() runs first thing in setup() and report() runs
// from the TeensyROM polling handler, where the C64 is by definition waiting.
static Record captured;
static bool captureHeld, captureValid;
static uint32_t captureFirstWord;

static FLASHMEM void capture() {
    captureFirstWord = slot()->magic;  // whatever is there, before take() clears it
    captureValid = take(captured);
    // Ok means the client came up; that is not worth interrupting the menu for.
    captureHeld = captureValid && captured.code != Ok;
}

// Called at the end of setup(), not from capture(): at capture() time the USB
// host has not enumerated yet, so anything printed there is lost.
static FLASHMEM void printBoot() {
    if (!captureValid) {
        Serial.printf("Extension boot: no record ($%08lx at $%08lx)\n",
                      (unsigned long)captureFirstWord, (unsigned long)base);
        return;
    }
    Serial.printf("Extension boot: %s (code $%02x, detail $%lx)\n",
                  describe((uint8_t)captured.code), (unsigned)captured.code,
                  (unsigned long)captured.detail);
}

static inline bool pending() { return captureHeld; }
// Cleared by the caller once the C64 has read the message, and by report()
// below when the attempts run out.
static inline void clear() { captureHeld = false; }

// An attempt the C64 does not read costs the menu SendMsgSerialStringBuf's
// three-second wait, and pending() is still true afterwards, so the next poll
// with queued work tries again. Bound the attempts here: SendMsgSerialStringBuf
// carries every other message in the firmware and is not ours to change.
static constexpr uint8_t reportAttemptLimit = 3;
static uint8_t reportAttempts;

static FLASHMEM void report() {
    if (reportAttempts == reportAttemptLimit) {
        Serial.printf("Extension boot: %s not read by the C64 in %u attempts, dropping it\n",
                      describe((uint8_t)captured.code), (unsigned)reportAttemptLimit);
        clear();
        return;
    }

    ++reportAttempts;
    if (captured.detail) SendMsgPrintfln("Extension: %s ($%02x/$%lx)",
        describe((uint8_t)captured.code), (unsigned)captured.code, (unsigned long)captured.detail);
    else SendMsgPrintfln("Extension: %s ($%02x)",
        describe((uint8_t)captured.code), (unsigned)captured.code);
}
#endif
}
