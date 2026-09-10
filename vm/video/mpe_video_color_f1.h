// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mean Hamster Software.
// Independent indexed-palette F1 converter. No external conversion tables.
#pragma once
namespace mpe_video {
class LiveConverter;
// Packed palette mixtures replace the preview's 16x256 lookup. This cache
// fits the spare bytes in both existing 24 KiB and 36 KiB workspace loans.
struct ColorF1Cache {
    uint8_t palette[768];
    uint8_t pair[256],fraction[256];
    uint16_t colors;
    bool ready;
    const LiveConverter *owner;
};
}
