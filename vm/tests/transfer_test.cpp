// SPDX-License-Identifier: MIT
// The real service bit 20 job, with the bus replaced by a script: which slices it asks for,
// what a refusal looks like, and how it ends when the client never grants.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../abi/vm_abi.h"
#include "../../Source/Teensy/MinimalBoot/Common/C64Spans.h"

static uint32_t now;
static uint32_t micros() { return now; }

struct Asked { uint16_t address; const uint8_t *source; uint32_t bytes; };
static std::vector<Asked> asked;
static int32_t answer = 1;       // what the bus does with the next slice
static uint32_t costs = 100;     // uS a slice takes, granted or not
static uint32_t grantEvery, nextGrant;   // when set, the client grants once every grantEvery uS

namespace VmRuntime {
static int32_t grantedSlice(uint16_t address, const uint8_t *source, uint32_t bytes, uint32_t until) {
    asked.push_back({ address, source, bytes });
    if (grantEvery) {
        if (int32_t(nextGrant - until) >= 0) { now = until; return 0; }
        if (int32_t(nextGrant - now) > 0) now = nextGrant;
        nextGrant = now + grantEvery; now += costs; return 1;
    }
    now += answer ? costs : until - now;   // a slice that is never granted waits its whole turn
    return answer;
}
#include "../../Source/Teensy/MinimalBoot/VMHostTransfer.h"
}
using namespace VmRuntime;

static uint8_t bytes[0x10000];

static void run() { for (int turn = 0; turn < 1000 && jobStatus == VM_C64_PENDING; turn++) c64Step(); }

int main() {
    // Refused whole, before anything reaches the bus.
    const VmC64Span one[] = { { bytes, 0x4000, 16 } };
    assert(!c64Write(one, 1, 0, 1));                                        // flags are reserved
    assert(!c64Write(one, 0, 0, 0));
    std::vector<VmC64Span> many(VM_C64_SPANS_MAX + 1, one[0]);
    assert(!c64Write(many.data(), VM_C64_SPANS_MAX + 1, 0, 0));
    const VmC64Span nothing[] = { { nullptr, 0x4000, 16 } };
    assert(!c64Write(nothing, 1, 0, 0));
    const VmC64Span io[] = { { bytes, 0xDDF8, 16 } }, wraps[] = { { bytes, 0xFFF8, 16 } };
    assert(!c64Write(io, 1, 0, 0) && !c64Write(wraps, 1, 0, 0));
    const VmC64Span big[] = { { bytes, 0x0000, 0xC000 } };
    assert(!c64Write(big, 1, 32, 0));                                       // 1,536 slices
    assert(asked.empty() && c64Status(0) == VM_C64_UNKNOWN && c64Status(1) == VM_C64_UNKNOWN);

    // Each span sliced from its own source, in span order; 0 is a span per grant.
    const VmC64Span two[] = { { bytes + 100, 0x4000, 10 }, { bytes + 900, 0x5000, 5 } };
    const uint32_t first = c64Write(two, 2, 4, 0);
    assert(first && c64Status(first) == VM_C64_PENDING);
    assert(!c64Write(one, 1, 0, 0));                                        // one at a time
    run();
    assert(c64Status(first) == VM_C64_DONE && asked.size() == 5);
    const Asked want[] = { { 0x4000, bytes + 100, 4 }, { 0x4004, bytes + 104, 4 }, { 0x4008, bytes + 108, 2 },
                           { 0x5000, bytes + 900, 4 }, { 0x5004, bytes + 904, 1 } };
    for (unsigned i = 0; i < 5; i++)
        assert(asked[i].address == want[i].address && asked[i].source == want[i].source && asked[i].bytes == want[i].bytes);
    asked.clear();
    const uint32_t whole = c64Write(two, 2, 0, 0);
    run();
    assert(whole == first + 1 && c64Status(whole) == VM_C64_DONE && c64Status(first) == VM_C64_UNKNOWN);
    assert(asked.size() == 2 && asked[0].bytes == 10 && asked[1].bytes == 5);

    // A turn is 1.5 mS: at 600 uS a slice, the third one lands and the step hands back.
    asked.clear(); costs = 600;
    const VmC64Span wide[] = { { bytes, 0x4000, 40 } };
    const uint32_t paced = c64Write(wide, 1, 4, 0);
    c64Step();
    assert(asked.size() == 3 && c64Status(paced) == VM_C64_PENDING);
    run();
    assert(asked.size() == 10 && c64Status(paced) == VM_C64_DONE);
    costs = 100;

    // A client that never grants: the job waits out VM_C64_GRANT_MS across turns, then fails,
    // and the host takes the next one.
    asked.clear(); answer = 0;
    const uint32_t ignored = c64Write(one, 1, 0, 0);
    const uint32_t began = now;
    run();
    assert(c64Status(ignored) == VM_C64_NO_GRANT && now - began >= VM_C64_GRANT_MS * 1000u);
    assert(now - began < VM_C64_GRANT_MS * 1000u + 1500 && asked.size() > 1);
    for (const auto &a : asked) assert(a.address == 0x4000 && a.bytes == 16);   // always the same slice
    // A grant that comes in time resets the wait: five slices granted 60 mS apart take 240 mS,
    // and are not a failure.
    answer = 1; grantEvery = VM_C64_GRANT_MS * 600u; nextGrant = now + grantEvery;
    const uint32_t slow = c64Write(two, 2, 4, 0), started = now;
    for (int turn = 0; turn < 1000 && c64Status(slow) == VM_C64_PENDING; turn++) c64Step();
    assert(c64Status(slow) == VM_C64_DONE && now - started >= 5 * grantEvery);
    grantEvery = 0;

    // A slice the bus does not complete ends the job there, and nothing more is asked of it.
    asked.clear(); answer = VM_C64_BUS_FAILED;
    const uint32_t broken = c64Write(two, 2, 4, 0);
    run(); c64Step();
    assert(c64Status(broken) == VM_C64_BUS_FAILED && asked.size() == 1);
    answer = 1;
    assert(c64Write(two, 2, 4, 0));
    puts("PASS: service bit 20's job; seven refusals before the bus, slices in span order from each span's own "
         "source, a span per grant, tickets, the 1.5 mS turn, a client that never grants, one that grants slowly, "
         "and a slice the bus fails");
    return 0;
}
