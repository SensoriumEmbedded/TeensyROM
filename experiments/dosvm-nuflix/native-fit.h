#pragma once
#include <stdint.h>
#include <string.h>
#if defined(__arm__)
#define NUFLIX_CODE __attribute__((section(".flashmem.nuflix.inline")))
#define NUFLIX_IMPL __attribute__((section(".flashmem.nuflix.external")))
#else
#define NUFLIX_CODE
#define NUFLIX_IMPL
#endif

namespace dosvm_nuflix {
struct NativeFit {
    uint8_t attributes[4000];
    uint8_t underlay[1200];
    uint8_t bug[400];
};
struct NativeSource {
    const void *context;
    uint8_t (*read)(const void *, unsigned, unsigned);
};
struct FitStats {
    unsigned regions = 0;
    unsigned samples = 0;
    unsigned candidates = 0;
};
class NativeConverter {
    uint32_t distance[16][16];
    static uint32_t minimum(uint32_t left, uint32_t right) { return left < right ? left : right; }
    static void topColors(const uint8_t *histogram, uint8_t *colors, bool excludeGrey = false) {
        uint16_t selected = excludeGrey ? uint16_t(1u << 15) : 0;
        for (unsigned slot = 0; slot < 4; ++slot) {
            unsigned best = 16;
            for (unsigned color = 0; color < 16; ++color)
                if (!(selected & (1u << color)) && (best == 16 || histogram[color] > histogram[best])) best = color;
            colors[slot] = uint8_t(best);
            selected |= uint16_t(1u << best);
        }
    }
    NUFLIX_CODE uint32_t fitCell(const uint8_t *pixels, const uint8_t *colors, int sprite,
                     uint8_t &attributes, FitStats &stats) const {
        uint32_t best = UINT32_MAX;
        for (unsigned paperIndex = 0; paperIndex < 4; ++paperIndex) {
            const unsigned paper = colors[paperIndex];
            for (unsigned inkIndex = 0; inkIndex < 4; ++inkIndex) {
                const unsigned ink = colors[inkIndex];
                uint32_t error = 0;
                ++stats.candidates;
                for (unsigned offset = 0; offset < 16; offset += 2) {
                    const auto left = pixels[offset], right = pixels[offset + 1];
                    const auto leftInk = distance[left][ink], rightInk = distance[right][ink];
                    auto pair = minimum(distance[left][paper], leftInk) + minimum(distance[right][paper], rightInk);
                    if (sprite >= 0) pair = minimum(pair, minimum(distance[left][sprite], leftInk) + minimum(distance[right][sprite], rightInk));
                    error += pair;
                    if (error >= best) break;
                }
                if (error < best) { best = error; attributes = uint8_t(paper | (ink << 4)); }
                if (!best) return best;
            }
        }
        return best;
    }
public:
    uint32_t colorDistance(unsigned first, unsigned second) const { return distance[first & 15][second & 15]; }
    NativeConverter() { initialize(); }
    NUFLIX_CODE void initialize() {
        static constexpr uint8_t rgb[16][3] = {
            {0,0,0},{255,255,255},{136,57,50},{103,182,189},{139,63,150},{85,160,73},{64,49,141},{191,206,114},
            {139,84,41},{87,66,0},{184,105,98},{80,80,80},{120,120,120},{148,224,137},{120,105,196},{159,159,159}
        };
        for (unsigned first = 0; first < 16; ++first) for (unsigned second = 0; second < 16; ++second) {
            uint32_t error = 0;
            for (unsigned channel = 0; channel < 3; ++channel) {
                const int delta = int(rgb[first][channel]) - rgb[second][channel];
                error += unsigned(delta * delta);
            }
            distance[first][second] = error;
        }
    }
    static bool regionDirty(const uint8_t *dirty, unsigned row, unsigned first, unsigned count) {
        if (!dirty) return true;
        const unsigned base = (row / 8) * 40 + first;
        for (unsigned offset = 0; offset < count; ++offset)
            if (dirty[(base + offset) / 8] & (1u << ((base + offset) & 7))) return true;
        return false;
    }
    NUFLIX_CODE FitStats convert(const NativeSource &source, NativeFit &out, const uint8_t *dirty = nullptr) const {
        FitStats stats;
        for (unsigned row = 0; row < 200; row += 2) {
            if (dirty) {
                uint8_t any = 0;
                for (unsigned byte = 0; byte < 5; ++byte) any |= dirty[(row / 8) * 5 + byte];
                if (!any) { row += 6; continue; }
            }
            if (regionDirty(dirty, row, 0, 3)) {
                uint8_t histogram[16]{};
                for (unsigned vertical = 0; vertical < 2; ++vertical) for (unsigned column = 0; column < 24; ++column)
                    ++histogram[source.read(source.context, column, row + vertical) & 15];
                topColors(histogram, out.bug + row * 2, true);
                memset(out.attributes + (row / 2) * 40, 255, 3);
                ++stats.regions; stats.samples += 48;
            }
            for (unsigned block = 0; block < 6; ++block) {
                if (!regionDirty(dirty, row, 3 + block * 6, 6)) continue;
                uint8_t pixels[6][16], colors[6][4], histogram[16]{};
                for (unsigned cell = 0; cell < 6; ++cell) {
                    uint8_t local[16]{};
                    for (unsigned vertical = 0; vertical < 2; ++vertical) for (unsigned column = 0; column < 8; ++column) {
                        const uint8_t color = source.read(source.context, 24 + block * 48 + cell * 8 + column, row + vertical) & 15;
                        pixels[cell][vertical * 8 + column] = color;
                        ++local[color]; ++histogram[color];
                    }
                    topColors(local, colors[cell]);
                }
                uint8_t sprites[4], selected = 0, attributes[6]{};
                topColors(histogram, sprites);
                uint32_t best = UINT32_MAX;
                for (unsigned candidate = 0; candidate < 4; ++candidate) {
                    uint8_t trial[6];
                    uint32_t error = 0;
                    for (unsigned cell = 0; cell < 6; ++cell) {
                        error += fitCell(pixels[cell], colors[cell], sprites[candidate], trial[cell], stats);
                        if (error >= best) break;
                    }
                    if (error < best) { best = error; selected = sprites[candidate]; memcpy(attributes, trial, 6); }
                    if (!best) break;
                }
                out.underlay[row * 6 + block] = out.underlay[(row + 1) * 6 + block] = selected;
                memcpy(out.attributes + (row / 2) * 40 + 3 + block * 6, attributes, 6);
                ++stats.regions; stats.samples += 96;
            }
            if (regionDirty(dirty, row, 39, 1)) {
                uint8_t pixels[16], histogram[16]{}, colors[4];
                for (unsigned vertical = 0; vertical < 2; ++vertical) for (unsigned column = 0; column < 8; ++column) {
                    const auto color = source.read(source.context, 312 + column, row + vertical) & 15;
                    pixels[vertical * 8 + column] = color; ++histogram[color];
                }
                topColors(histogram, colors);
                fitCell(pixels, colors, -1, out.attributes[(row / 2) * 40 + 39], stats);
                ++stats.regions; stats.samples += 16;
            }
        }
        return stats;
    }
};
static_assert(sizeof(NativeFit) == 5600, "Native NUFLIX fit storage");
}
