// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mean Hamster Software.
// Independently implemented polling debounce for an INPUT_PULLUP button.
#pragma once
#include <Arduino.h>
#include <stdint.h>

class ButtonDebounce {
    uint32_t candidateSince_ = 0;
    uint32_t intervalMs_;
    uint8_t pin_;
    bool candidateHigh_ = true;
    bool settledHigh_ = true;

public:
    // Construction performs no GPIO access: pinMode runs later in setup().
    // A button held at startup must first qualify as a settled LOW input.
    ButtonDebounce(uint8_t pin, uint32_t intervalMs)
        : intervalMs_(intervalMs), pin_(pin) {}

    bool update() {
        const bool high = digitalRead(pin_) != LOW;
        const uint32_t now = static_cast<uint32_t>(millis());
        if (high != candidateHigh_) {
            candidateHigh_ = high;
            candidateSince_ = now;
        }
        // Unsigned elapsed time remains correct across millis() rollover.
        if (candidateHigh_ != settledHigh_ &&
            static_cast<uint32_t>(now - candidateSince_) >= intervalMs_) {
            settledHigh_ = candidateHigh_;
            return true;
        }
        return false;
    }

    // Preserve electrical polarity: HIGH is released, LOW is pressed.
    bool read() const { return settledHigh_; }
};
