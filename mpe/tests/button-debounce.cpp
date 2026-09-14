// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mean Hamster Software.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "ButtonDebounce.h"

static uint32_t nowMs = 0;
static unsigned reads = 0, clocks = 0;
static bool level[256] = {};
static bool scheduled = false;
struct Edge { uint32_t time; bool high; };
static std::vector<Edge> edges;
static size_t nextEdge = 0;
int digitalRead(uint8_t pin) {
    ++reads;
    if (scheduled) {
        assert(nowMs < 200); // A failed production wait must fail, not hang.
        ++nowMs;
        while (nextEdge < edges.size() && edges[nextEdge].time <= nowMs)
            level[pin] = edges[nextEdge++].high;
    }
    return level[pin] ? HIGH : LOW;
}
uint32_t millis() { ++clocks; return nowMs; }

#define Fab04_SpecialButton 1
constexpr uint8_t Special_Btn_In_PIN = 28;
static unsigned desktopResets = 0;
static std::vector<bool> callbacks;
static std::vector<uint32_t> detections;
void DesktopFileReset() { ++desktopResets; }
void recordButton(bool released) { callbacks.push_back(released); }
void (*fSpecialBtnChange)(bool) = recordButton;
void SendMsgPrintfln(const char*) {}
void SendMsgPrintf(const char*) { detections.push_back(nowMs); }
#include "production-button-callers.h"

static bool sample(ButtonDebounce &button, uint32_t when, bool high, uint8_t pin = 28) {
    nowMs = when; level[pin] = high;
    return button.update();
}
static void dispatch(uint32_t when, bool high) {
    nowMs = when; level[28] = high;
    productionLoopButton();
}

int main() {
    // 1. Global/local construction does not sample unconfigured GPIO or clocks.
    assert(reads == 0 && clocks == 0);
    ButtonDebounce idle(28, 35);
    assert(reads == 0 && clocks == 0 && idle.read() == HIGH);
    assert(!sample(idle, 100000, true));
    assert(!sample(idle, 110000, true));

    // 2. A held startup press still needs the full observed interval.
    ButtonDebounce startup(28, 35);
    assert(!sample(startup, 120000, false) && startup.read() == HIGH);
    assert(!sample(startup, 120034, false));
    assert(sample(startup, 120035, false) && startup.read() == LOW);
    assert(!sample(startup, 130000, false));

    // 3. Every observed bounce restarts qualification; one event per press.
    ButtonDebounce button(28, 35);
    for (const Edge edge : {Edge{0,false},{5,true},{10,false},{20,true},{22,false},{56,false}})
        assert(!sample(button, edge.time, edge.high) && button.read() == HIGH);
    assert(sample(button, 57, false) && button.read() == LOW);
    assert(!sample(button, 58, false));

    // 4. Release uses the same stable interval and preserves HIGH polarity.
    for (const Edge edge : {Edge{80,true},{85,false},{90,true},{124,true}})
        assert(!sample(button, edge.time, edge.high) && button.read() == LOW);
    assert(sample(button, 125, true) && button.read() == HIGH);
    assert(!sample(button, 1000, true));

    // 5. A short pulse returning to the settled state emits no event.
    ButtonDebounce pulse(28, 35);
    assert(!sample(pulse, 0, true));
    assert(!sample(pulse, 100, false));
    assert(!sample(pulse, 120, true));
    assert(!sample(pulse, 200, true) && pulse.read() == HIGH);

    // 6. Press qualification crosses the 32-bit millisecond wrap boundary.
    ButtonDebounce wrapPress(28, 35);
    assert(!sample(wrapPress, 0xfffffff0u, false));
    assert(!sample(wrapPress, 0x12, false));
    assert(sample(wrapPress, 0x13, false) && wrapPress.read() == LOW);

    // 7. Release qualification also crosses wrap without an early event.
    ButtonDebounce wrapRelease(28, 35);
    assert(!sample(wrapRelease, 0xffffff90u, false));
    assert(sample(wrapRelease, 0xffffffb3u, false));
    assert(!sample(wrapRelease, 0xfffffff0u, true));
    assert(!sample(wrapRelease, 0x12, true));
    assert(sample(wrapRelease, 0x13, true) && wrapRelease.read() == HIGH);

    // 8. Zero-interval callers and independent pins keep separate state.
    ButtonDebounce immediate(27, 0), delayed(28, 35);
    assert(sample(immediate, 500, false, 27));
    assert(!sample(delayed, 500, false));
    assert(sample(immediate, 500, true, 27));
    assert(!sample(delayed, 534, false));
    assert(sample(delayed, 535, false));
    assert(immediate.read() == HIGH && delayed.read() == LOW);

    // 9. Execute the actual loop branch and declaration: callback order,
    // active-low press, release, no desktop side effect, and a null handler.
    SpecialBtnBounce = ButtonDebounce(Special_Btn_In_PIN, 35);
    callbacks.clear(); desktopResets = 0;
    dispatch(0, true); dispatch(5, false); dispatch(10, true);
    dispatch(15, false); dispatch(49, false); dispatch(50, false);
    dispatch(100, false); dispatch(110, true); dispatch(144, true); dispatch(145, true);
    assert((callbacks == std::vector<bool>{false, true}) && desktopResets == 0);
    fSpecialBtnChange = nullptr;
    dispatch(150, false); dispatch(185, false);
    assert(callbacks.size() == 2 && desktopResets == 0);

    // 10. Execute both actual blocking diagnostic loops with timed bounce.
    SpecialBtnBounce = ButtonDebounce(Special_Btn_In_PIN, 35);
    nowMs = 0; level[28] = true; scheduled = true; nextEdge = 0;
    edges = {{5,false},{10,true},{12,false},{20,true},{30,false},{80,true},{85,false},{90,true}};
    productionStatusButton();
    scheduled = false;
    assert((detections == std::vector<uint32_t>{65,125}));
    assert(SpecialBtnBounce.read() == HIGH);

    std::puts("10 debounce and production integration scenarios passed");
}
