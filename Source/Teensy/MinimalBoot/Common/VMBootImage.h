// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <string.h>
#include "VMABI.h"

// base/limit must match VM_BASE/VM_LIMIT in tools/lib/hex.mjs, which partitions
// the combined hex -- tools/verify-extensions.mjs compares the two. The ordinary
// images keep their upstream addresses. Only the extension image uses the
// module-compatible RAM map.
namespace VmBootImage {
static constexpr uint32_t base = 0x60280000u;
static constexpr uint32_t limit = 0x602e0000u;

// In the 0xFF fill between the FlexSPI config block and the image vector
// table. extensionLinkerScript() in tools/lib/extension-image.mjs places
// .vmhostid here; tools/verify-extensions.mjs compares the two.
static constexpr uint32_t idOffset = 0x800u;

#if defined(__arm__)
static inline const uint8_t *window() { return reinterpret_cast<const uint8_t *>(base); }
#else
// Host conformance builds have no flash behind `base`, so the slot they inspect
// is an ordinary buffer, which install() below fills.
alignas(uint32_t) inline uint8_t hostWindow[0x1028];
static inline const uint8_t *window() { return hostWindow; }
#endif

static inline bool valid(uint32_t flashMagic, uint32_t vectorMagic,
                         uint32_t entry, uint32_t bootBase, uint32_t imageBytes) {
    const uint32_t address = entry & ~1u;
    return flashMagic == 0x42464346u && vectorMagic == 0x432000d1u &&
           (entry & 1u) && address >= base + 0x1000u && address <= base + 0x3000u &&
           bootBase == base && imageBytes > 0x1000u && imageBytes <= limit - base;
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

inline void install(uint32_t services, uint32_t abi = VM_ABI) {
    installWithoutDescriptor();
    const VmHostId id = { VM_HOSTID_MAGIC, abi, services, sizeof(VmHost), "TeensyROM", 0 };
    memcpy(hostWindow + idOffset, &id, sizeof id);
}
#endif
}
