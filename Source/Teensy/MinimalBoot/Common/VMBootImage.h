// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "VMHostABI.h"

// The extension host's flash slot, as the ordinary images see it. The geometry
// and the validity predicate are the published host contract (VMHostABI.h);
// this adds the reads against real flash, the one way to render a descriptor's
// name for a message, and the buffer that stands in for the slot on host
// conformance builds.
//
// The ordinary images keep their upstream addresses. Only the extension image
// uses the module-compatible RAM map.
namespace VmBootImage {
static constexpr uint32_t base = VM_HOST_SLOT_BASE;
static constexpr uint32_t limit = VM_HOST_SLOT_LIMIT;

static constexpr uint32_t idOffset = VM_HOST_ID_OFFSET;

#if defined(__arm__)
static inline const uint8_t *window() { return reinterpret_cast<const uint8_t *>(base); }
#else
// Host conformance builds have no flash behind `base`, so the slot they inspect
// is an ordinary buffer, which install() below fills. Sized to the whole slot,
// so it can stand in for the region an installer writes.
alignas(uint32_t) inline uint8_t hostWindow[VM_HOST_SLOT_BYTES];
static inline const uint8_t *window() { return hostWindow; }
#endif

static inline bool valid(uint32_t flashMagic, uint32_t vectorMagic,
                         uint32_t entry, uint32_t bootBase, uint32_t imageBytes) {
    return vm_host_slot_valid(flashMagic, vectorMagic, entry, bootBase, imageBytes);
}
static inline bool installed() {
    const auto flash = reinterpret_cast<const volatile uint32_t *>(window());
    // BootData follows the eight-word image vector table.
    return valid(flash[0], flash[0x1000 / 4], flash[0x1004 / 4],
                 flash[0x1020 / 4], flash[0x1024 / 4]);
}

// False only when the slot holds no descriptor at all: a host built before it
// existed reads as erased flash. That is "cannot say", not "provides nothing"
// -- see tryLaunch, which proceeds on false and judges the rest.
static inline bool identity(VmHostId &out) {
    VmHostId id{};
    memcpy(&id, window() + idOffset, sizeof id);
    if (id.magic != VM_HOSTID_MAGIC) return false;
    out = id;
    return true;
}

// The name a host message shows, for every message that shows one -- the install and
// removal notices and the menu's host line all come through here, because three copies
// of it are how the buffer sizes drifted apart in the first place.
//
// Takes the array rather than a pointer so that rewriting this as a `const char *`
// fails to compile: `sizeof` a pointer is 4, which would quietly size nameBytes below
// from the field alone and restore the char[13] that a strcpy of the placeholder used
// to overrun by three.
static constexpr char noDescriptor[] = "(no descriptor)";
template<unsigned N> static constexpr unsigned literalBytes(const char (&)[N]) { return N; }

// Sized for whichever source is longer, because they are not the same length:
// VmHostId::name is a fixed 12 bytes and need not be terminated, while the placeholder
// is 15. A buffer sized from the field alone -- the obvious `sizeof id.name + 1` -- is
// three bytes short of the placeholder.
static constexpr unsigned nameBytes =
    sizeof(VmHostId::name) + 1 > literalBytes(noDescriptor)
        ? sizeof(VmHostId::name) + 1 : literalBytes(noDescriptor);

// `bytes` is the caller's buffer, which is nameBytes wide if it wants either source
// whole; both writes are bounded by it, so a later edit to either source truncates
// rather than running off the end. `%.*s` is what reads a name that fills all 12 bytes
// with no terminator. A null `id` is identity() saying no: a host built before the
// descriptor existed, whose name cannot be read rather than being blank.
static inline void displayName(char *out, size_t bytes, const VmHostId *id) {
    if (id) snprintf(out, bytes, "%.*s", (int)sizeof id->name, id->name);
    else    snprintf(out, bytes, "%s", noDescriptor);
}

#if !defined(__arm__)
// Writes what installed() and identity() read, so the layout stays beside the
// code that reads it rather than in each test.
inline void installWithoutDescriptor() {
    const uint32_t words[] = { 0x42464346u, 0u, 0u, 0u };
    memset(hostWindow, 0, sizeof hostWindow);
    memcpy(hostWindow, words, sizeof words);
    const uint32_t vector = 0x432000d1u, entry = base + 0x1001u, boot = base, bytes = 0x2000u;
    memcpy(hostWindow + 0x1000, &vector, 4);
    memcpy(hostWindow + 0x1004, &entry, 4);
    memcpy(hostWindow + 0x1020, &boot, 4);
    memcpy(hostWindow + 0x1024, &bytes, 4);
    memset(hostWindow + idOffset, 0xff, sizeof(VmHostId));
}

inline void install(uint32_t services, uint32_t abi = VM_ABI, const char *name = "TeensyROM") {
    installWithoutDescriptor();
    VmHostId id = { VM_HOSTID_MAGIC, abi, services, sizeof(VmHost), {}, 0 };
    snprintf(id.name, sizeof id.name, "%s", name);
    memcpy(hostWindow + idOffset, &id, sizeof id);
}
#endif
}
