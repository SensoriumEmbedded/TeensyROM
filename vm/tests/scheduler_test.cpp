// SPDX-License-Identifier: MIT
// The real VMHostPoll, with only the bus registers and the clock replaced.
// The ordering rules it enforces are the subtle part of the loader, so they are
// asserted here rather than discovered on hardware.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../abi/vm_abi.h"

static uint8_t EZFlashRAM[256];
static uint32_t now;
static uint32_t micros() { return now; }

namespace VmRuntime {
static const VmModule *module;
static VmPacket packet;
static bool active = true, started, startRequested, inputPending, pending, quietRequested;
static uint8_t failure, sequence;
static uint32_t sliceStarted;
static VmInput input;
static unsigned pumps, acks, offers, inputs;
static VmInput lastInput;
static bool offer;
static VmPacket offered;
static void fail(uint8_t error) { failure = error; EZFlashRAM[0xfb] = error; EZFlashRAM[0xf5] = 0xe0; }
static uint16_t crc16(const uint8_t *p, unsigned n) {
    uint16_t c = 0xffff;
    while (n--) { c ^= uint16_t(*p++) << 8; for (unsigned b = 0; b < 8; b++) c = (c << 1) ^ ((c & 0x8000) ? 0x1021 : 0); }
    return c;
}
static unsigned encodePacket(uint8_t *bytes) {
    bytes[0] = 'M'; bytes[1] = '3'; bytes[2] = 1; bytes[3] = packet.type; bytes[4] = sequence;
    bytes[5] = packet.flags; bytes[6] = packet.length; bytes[7] = 0;
    memcpy(bytes + 8, packet.payload, packet.length);
    const auto crc = crc16(bytes, 8 + packet.length);
    bytes[8 + packet.length] = crc; bytes[9 + packet.length] = crc >> 8;
    return 10u + packet.length;
}
#include "../../Source/Teensy/MinimalBoot/VMHostYield.h"
}
#include "../../Source/Teensy/MinimalBoot/VMHostPoll.h"

using namespace VmRuntime;

static const VmModule spy{ VM_ABI, sizeof(VmModule),
    [](const VmInput *in) { ++inputs; lastInput = *in; },
    []() { ++pumps; },
    [](VmPacket *p) { if (!offer) return false; offer = false; ++offers; *p = offered; return true; },
    []() { ++acks; } };

static void reset() {
    module = &spy; active = true; started = startRequested = inputPending = pending = quietRequested = false;
    failure = sequence = 0; pumps = acks = offers = inputs = 0; offer = false;
    memset(EZFlashRAM, 0, sizeof EZFlashRAM);
    offered = {}; offered.type = 2; offered.length = 26;
    for (unsigned i = 0; i < 26; i++) offered.payload[i] = i * 7;
}

int main() {
    // Nothing runs before the client says it is ready.
    reset();
    offer = true;
    VMHostPoll();
    assert(!pumps && !offers && !pending);
    startRequested = true;
    VMHostPoll();
    assert(started && EZFlashRAM[0xf5] == 2 && pending && sequence == 1 && offers == 1);

    // The published frame is exactly what the client is told to expect.
    uint8_t expected[240];
    const unsigned size = encodePacket(expected);
    assert(!memcmp(EZFlashRAM, expected, size));
    assert(EZFlashRAM[0] == 'M' && EZFlashRAM[1] == '3' && EZFlashRAM[3] == 2 && EZFlashRAM[4] == 1);
    assert(EZFlashRAM[0xf7] == 1);

    // Frozen until ACK: no new packet, and pump must not disturb the buffer.
    offer = true;
    const auto before = packet;
    for (int i = 0; i < 3; i++) VMHostPoll();
    assert(pending && offers == 1 && sequence == 1 && !memcmp(&packet, &before, sizeof packet));
    assert(!memcmp(EZFlashRAM, expected, size));

    // The ACK is consumed before the next pump, and does not itself consume a
    // slice: one poll delivers the ACK and one pump, then republishes.
    const auto pumpsBefore = pumps;
    EZFlashRAM[0xf6] = 1;
    VMHostPoll();
    assert(acks == 1 && pumps == pumpsBefore + 1 && offers == 2 && sequence == 2);

    // Input reaches the module ahead of the pump that follows it.
    reset();
    started = true; offer = false;
    input = VmInput{ 0x21, 3, 0, 0x81 };
    inputPending = true;
    VMHostPoll();
    assert(inputs == 1 && !inputPending && pumps == 1);
    assert(lastInput.buttons == 0x21 && lastInput.display == 3 && lastInput.protocol == 0x81);

    // "Nothing to say" publishes nothing and is not a failure.
    reset();
    started = true; offer = false;
    for (int i = 0; i < 5; i++) VMHostPoll();
    assert(!pending && !failure && pumps == 5 && !EZFlashRAM[0xf7]);

    // A quiet request stops the module entirely until it is lifted by an ACK.
    reset();
    started = true; offer = true; quietRequested = true;
    VMHostPoll();
    assert(!pumps && !offers && !pending && EZFlashRAM[0xf5] == 0x12);
    quietRequested = false;
    VMHostPoll();
    assert(pumps == 1 && pending);

    // Sequence numbers skip zero, so the client can use zero as "none".
    reset();
    started = true; sequence = 255; offer = true;
    VMHostPoll();
    assert(sequence == 1 && EZFlashRAM[0xf7] == 1);

    // A malformed packet is a module fault, reported and latched.
    for (const auto &broken : { [] { VmPacket p{}; p.type = 1; p.length = 229; return p; }(),
                                [] { VmPacket p{}; p.type = 1; p.reserved = 1; return p; }(),
                                [] { VmPacket p{}; p.type = 0; p.length = 4; return p; }() }) {
        reset();
        started = true; offered = broken; offer = true;
        VMHostPoll();
        assert(failure == 0x15 && !pending && EZFlashRAM[0xf5] == 0xe0);
        // Latched: once failed, the module is not called again.
        const auto pumpsAfter = pumps;
        VMHostPoll();
        assert(pumps == pumpsAfter);
    }

    // A module that failed to load is reported at start, not silently idle.
    reset();
    failure = 0x11; startRequested = true;
    VMHostPoll();
    assert(EZFlashRAM[0xf5] == 0xe0 && EZFlashRAM[0xfb] == 0x11 && !pumps);

    // A slice ends when the client needs attention or 1500us elapse.
    reset();
    started = true; pending = true; sequence = 4;
    now = 0; sliceStarted = 0;
    assert(!shouldYield());
    now = 1500;
    assert(shouldYield());
    now = 0; inputPending = true;
    assert(shouldYield());
    inputPending = false; EZFlashRAM[0xf6] = 4;
    assert(shouldYield());

    puts("PASS: real scheduler; start handshake, wire framing, frozen-until-ACK, ACK before pump, "
         "input ordering, silence, quiet, sequence wrap, three malformed packets latched, load failure and yield conditions");
}
