// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// Keep synchronized with mpe/tools/build.mjs. The ordinary images keep their
// upstream addresses. Only the MPE image uses the module-compatible RAM map.
namespace MPEBootImage {
static constexpr uint32_t base = 0x60280000u;
static constexpr uint32_t limit = 0x602e0000u;
static inline bool valid(uint32_t flashMagic, uint32_t vectorMagic,
                         uint32_t entry, uint32_t bootBase, uint32_t imageBytes) {
    const uint32_t address = entry & ~1u;
    return flashMagic == 0x42464346u && vectorMagic == 0x432000d1u &&
           (entry & 1u) && address >= base + 0x1000u && address <= base + 0x3000u &&
           bootBase == base && imageBytes > 0x1000u && imageBytes <= limit - base;
}
static inline bool installed() {
    const auto flash = reinterpret_cast<const volatile uint32_t *>(base);
    // BootData follows the eight-word image vector table.
    return valid(flash[0], flash[0x1000 / 4], flash[0x1004 / 4],
                 flash[0x1020 / 4], flash[0x1024 / 4]);
}
}
