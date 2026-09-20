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
#include "../../Source/Teensy/MinimalBoot/VMHostWire.h"
#include "../../Source/Teensy/MinimalBoot/VMHostCommand.h"
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
    commandWrite(1);                  // before the start, 1 starts
    assert(startRequested && !started);
    VMHostPoll();
    assert(started && EZFlashRAM[0xf5] == 2 && pending && sequence == 1 && offers == 1);

    // The published frame, pinned to literal bytes. Comparing against the
    // encoder that produced it would assert nothing about the wire format.
    // The trailer is CRC-16/CCITT-FALSE over the 34 header and payload bytes.
    uint8_t expected[36] = { 'M', '3', 1, 2, 1, 0, 26, 0 };
    for (unsigned i = 0; i < 26; i++) expected[8 + i] = uint8_t(i * 7);
    expected[34] = 0x61; expected[35] = 0x2e;
    const unsigned size = sizeof expected;
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

    // Quiet asked for with nothing outstanding has no ACK left to lift it, so
    // $DFF4 = 1 has to, and the status has to leave $12 when it does.
    reset();
    started = true; offer = false;
    commandWrite(4);
    VMHostPoll();
    assert(!pumps && EZFlashRAM[0xf5] == 0x12);
    commandWrite(1);
    assert(!quietRequested && !startRequested);
    VMHostPoll();
    assert(pumps == 1 && EZFlashRAM[0xf5] == 2);

    // 1 is idempotent, so a client can write it without tracking the state:
    // asked for again while the module is already running, it changes nothing.
    commandWrite(1);
    assert(started && !startRequested && !quietRequested);
    VMHostPoll();
    assert(pumps == 2 && EZFlashRAM[0xf5] == 2 && !failure);

    // An input record is taken once, and only when its token is new and its
    // checksum agrees.
    reset();
    started = true;
    EZFlashRAM[0xf8] = 0x21; EZFlashRAM[0xf9] = 3; EZFlashRAM[0xfa] = 0; EZFlashRAM[0xfd] = 0x81;
    EZFlashRAM[0xfe] = 7; EZFlashRAM[0xff] = 0xa5 ^ 0x21 ^ 3 ^ 0 ^ 0x81 ^ 7;
    commandWrite(3);
    assert(inputPending && EZFlashRAM[0xfc] == 7 && input.buttons == 0x21 && input.protocol == 0x81);
    inputPending = false;
    commandWrite(3);
    assert(!inputPending);            // same token, already consumed
    EZFlashRAM[0xfe] = 8;
    commandWrite(3);
    assert(!inputPending);            // new token, stale checksum

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
         "input ordering, silence, idempotent run over all three states, sequence wrap, three malformed packets "
         "latched, load failure and yield conditions");
}
