// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

namespace mpe_video {

enum class RenderMode : uint8_t {
    Native,
    Color,
    Auto8,
    Enhanced25,
    Sharp
};

enum class Selector : uint8_t {
    Default,
    Auto8,
    Enhanced25,
    Sharp
};

using CapabilityMask = uint8_t;

constexpr bool valid_render_mode(RenderMode mode) {
    return static_cast<uint8_t>(mode) <=
           static_cast<uint8_t>(RenderMode::Sharp);
}

constexpr CapabilityMask capability(RenderMode mode) {
    return valid_render_mode(mode)
               ? CapabilityMask(1u << static_cast<uint8_t>(mode))
               : CapabilityMask(0);
}

constexpr CapabilityMask all_capabilities() {
    return CapabilityMask(capability(RenderMode::Native) |
                          capability(RenderMode::Color) |
                          capability(RenderMode::Auto8) |
                          capability(RenderMode::Enhanced25) |
                          capability(RenderMode::Sharp));
}

struct PackagePolicy {
    RenderMode preferred;
    CapabilityMask capabilities;

    constexpr PackagePolicy(RenderMode preferred_mode,
                            CapabilityMask capability_mask)
        : preferred(valid_render_mode(preferred_mode) ? preferred_mode
                                                      : RenderMode::Color),
          capabilities(CapabilityMask(
              (capability_mask & all_capabilities()) | capability(preferred))) {}

    constexpr bool supports(RenderMode mode) const {
        return (capabilities & capability(mode)) != 0;
    }
};

constexpr CapabilityMask indexed_video_capabilities() {
    return CapabilityMask(capability(RenderMode::Color) |
                          capability(RenderMode::Auto8) |
                          capability(RenderMode::Enhanced25) |
                          capability(RenderMode::Sharp));
}

// Shared opt-in policy for neutral indexed-frame producers. Individual
// packages may choose different defaults without adding engine IDs here.
constexpr PackagePolicy indexed_video_policy(
    RenderMode preferred = RenderMode::Color) {
    return PackagePolicy(preferred == RenderMode::Native ? RenderMode::Color
                                                          : preferred,
                         indexed_video_capabilities());
}

// AGI already matches the native C64 presentation well and retains all of its
// function-key bindings. It therefore advertises no alternate presentation.
constexpr PackagePolicy native_only_policy() {
    return PackagePolicy(RenderMode::Native, capability(RenderMode::Native));
}

inline bool selector_for_scan(uint8_t scan, Selector &selector) {
    switch (scan) {
        case 0x3b: selector = Selector::Default; return true;     // F1
        case 0x3d: selector = Selector::Auto8; return true;       // F3
        case 0x3f: selector = Selector::Enhanced25; return true;  // F5
        case 0x41: selector = Selector::Sharp; return true;       // F7
        default: return false;
    }
}

class PresentationPolicy {
public:
    explicit PresentationPolicy(PackagePolicy package)
        : package_(package), current_(package.preferred) {}

    const PackagePolicy &package() const { return package_; }
    RenderMode current() const { return current_; }

    void reset() { current_ = package_.preferred; }

    // Selectors name a mode directly. Reapplying the active selector succeeds
    // without toggling, while an unsupported alternate leaves state untouched.
    bool select(Selector selector) {
        RenderMode requested = package_.preferred;
        if (!mode_for(selector, requested)) return false;
        if (!package_.supports(requested)) return false;
        current_ = requested;
        return true;
    }

private:
    bool mode_for(Selector selector, RenderMode &mode) const {
        switch (selector) {
            case Selector::Default: mode = package_.preferred; return true;
            case Selector::Auto8: mode = RenderMode::Auto8; return true;
            case Selector::Enhanced25: mode = RenderMode::Enhanced25; return true;
            case Selector::Sharp: mode = RenderMode::Sharp; return true;
        }
        return false;
    }

    PackagePolicy package_;
    RenderMode current_;
};

}  // namespace mpe_video
