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
static uint32_t grantEvery, grantBase;   // when set, the client grants at grantBase + n * grantEvery
static uint32_t heardUntil;              // the grants before this have been answered for

static uint8_t bytes[0x10000];

namespace VmRuntime {
#include "../../Source/Teensy/MinimalBoot/VMHostTransfer.h"
// Stands in for moduleWindow(), which the test holds to its rules directly: here the module's
// memory is `bytes`.
static bool sourceReadable(const uint8_t *source, uint32_t n) {
    return source >= bytes && source < bytes + sizeof bytes && n <= sizeof bytes - uint32_t(source - bytes);
}
static int32_t grantedSlice(uint16_t address, const uint8_t *source, uint32_t bytes, uint32_t until) {
    asked.push_back({ address, source, bytes });
    if (grantEvery) {   // and a grant lands only while a slice waits for it, as VMHostIO2 has it
        auto from = [](uint32_t t) { return grantBase + (t - grantBase + grantEvery - 1) / grantEvery * grantEvery; };
        const bool missed = int32_t(from(heardUntil) - now) < 0;   // one came with nothing armed
        const uint32_t next = from(now);
        if (int32_t(next - until) >= 0) { now = heardUntil = until; return missed ? SliceMissed : SliceWaiting; }
        now = heardUntil = next + costs; return SliceLanded;
    }
    now += answer ? costs : until - now;   // a slice that is never granted waits its whole turn
    return answer;
}
}
using namespace VmRuntime;

static void run() { for (int turn = 0; turn < 1000 && jobStatus == VM_C64_PENDING; turn++) c64Step(); }

int main() {
    // Refused whole, before anything reaches the bus.
    const VmC64Span one[] = { { bytes, 0x4000, 16 } };
    assert(!c64Write(one, 1, 0, 1));                                        // flags are reserved
    assert(!c64Write(one, 0, 0, 0));
    std::vector<VmC64Span> many(VM_C64_SPANS_MAX + 1, one[0]);
    assert(!c64Write(many.data(), VM_C64_SPANS_MAX + 1, 0, 0));
    const VmC64Span nothing[] = { { nullptr, 0x4000, 16 } }, offEnd[] = { { bytes + sizeof bytes - 8, 0x4000, 16 } };
    assert(!c64Write(nothing, 1, 0, 0) && !c64Write(offEnd, 1, 0, 0));
    // A source must lie wholly inside one window the module is lent.
    assert(moduleWindow(VM_DATA_BASE, VM_DATA_BYTES) && moduleWindow(VM_RAM_BASE, VM_RAM_BYTES));
    assert(moduleWindow(VM_CODE_LIMIT - 1, 1) && !moduleWindow(VM_CODE_LIMIT - 1, 2));
    assert(!moduleWindow(VM_DATA_LIMIT - 8, 16) && !moduleWindow(VM_RAM_LIMIT - 8, 0xBE00));
    assert(!moduleWindow(0x10000000, 1) && !moduleWindow(VM_DATA_BASE - 1, 2));
    moduleRamLimit = VM_RAM_LIMIT - VM_RAM_RESERVED_BYTES;                  // profile 1
    assert(moduleWindow(VM_RAM2_RO_BASE, VM_RAM2_RO_BYTES) && !moduleWindow(VM_RAM2_RO_BASE, VM_RAM2_RO_BYTES + 1));
    moduleRamLimit = VM_RAM_LIMIT;
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
    answer = 1; grantEvery = VM_C64_GRANT_MS * 600u; grantBase = heardUntil = now;
    const uint32_t slow = c64Write(two, 2, 4, 0), started = now;
    for (int turn = 0; turn < 1000 && c64Status(slow) == VM_C64_PENDING; turn++) c64Step();
    assert(c64Status(slow) == VM_C64_DONE && now - started >= 4 * grantEvery);
    // Grants that come during the module's turns start nothing, and that time is not waiting:
    // with 8.5 mS turns between steps, a grant every 17 mS lands two slices 119 mS apart.
    grantEvery = 17000; grantBase = heardUntil = now;
    const uint32_t busy = c64Write(two, 2, 4, 0), from = now;
    for (int turn = 0; turn < 1000 && c64Status(busy) == VM_C64_PENDING; turn++) { c64Step(); now += 8500; }
    assert(c64Status(busy) == VM_C64_DONE && now - from > VM_C64_GRANT_MS * 1000u);
    // Grants in step with the turns, every one between two of them: the job waits, and is not
    // refused for it, for as long as the client goes on granting. Then one lands.
    grantEvery = 20000; grantBase = now - 10000; heardUntil = now;
    const uint32_t locked = c64Write(one, 1, 0, 0);
    for (int turn = 0; turn < 200; turn++) { c64Step(); now += 18500; }
    assert(c64Status(locked) == VM_C64_PENDING);
    grantBase = heardUntil = now;
    c64Step();
    assert(c64Status(locked) == VM_C64_DONE);
    grantEvery = 0;

    // A slice the bus does not complete ends the job there, and nothing more is asked of it.
    asked.clear(); answer = VM_C64_BUS_FAILED;
    const uint32_t broken = c64Write(two, 2, 4, 0);
    run(); c64Step();
    assert(c64Status(broken) == VM_C64_BUS_FAILED && asked.size() == 1);
    answer = 1;
    assert(c64Write(two, 2, 4, 0));
    puts("PASS: service bit 20's job; eight refusals before the bus, the module windows a source must lie in, slices in span order from each span's own "
         "source, a span per grant, tickets, the 1.5 mS turn, a client that never grants, one that grants slowly, between turns or in step with them, "
         "and a slice the bus fails");
    return 0;
}
