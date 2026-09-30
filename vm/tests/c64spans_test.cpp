// SPDX-License-Identifier: MIT
// The span rules a remote C64 write is held to before any DMA starts.
#include <cassert>
#include "../../Source/Teensy/MinimalBoot/Common/C64Spans.h"

using C = C64SpansCheck;
static C check(const C64Span *s, uint32_t n, uint32_t buffer = 0x10000) { uint32_t total = 0; return CheckC64Spans(s, n, buffer, &total); }
static C one(uint16_t addr, uint16_t len, uint32_t buffer = 0x10000) { const C64Span s{addr, len}; return check(&s, 1, buffer); }

int main() {
    C64Span many[C64SpansMax + 1];
    for (auto &s : many) s = {0x0400, 1};
    assert(check(many, 0) == C::BadCount);
    assert(check(many, C64SpansMax) == C::OK);
    assert(check(many, C64SpansMax + 1) == C::BadCount);

    assert(one(0x0400, 0) == C::EmptySpan);
    assert(one(0xFFFF, 1) == C::OK);
    assert(one(0xFFFF, 2) == C::Wraps);
    assert(one(0xF000, 0x2000) == C::Wraps);
    assert(one(0x0001, 0xFFFF) == C::HitsIO);

    assert(one(0xDDFF, 1) == C::OK);
    assert(one(0xDDFF, 2) == C::HitsIO);
    assert(one(0xDE00, 1) == C::HitsIO);
    assert(one(0xDFFF, 1) == C::HitsIO);
    assert(one(0xE000, 1) == C::OK);
    assert(one(0xD000, 0x3000) == C::HitsIO);

    const C64Span pair[] = {{0x0400, 1000}, {0xD800, 1000}};
    uint32_t total = 0;
    assert(CheckC64Spans(pair, 2, 2000, &total) == C::OK && total == 2000);
    assert(CheckC64Spans(pair, 2, 1999, &total) == C::TooBig);
    const C64Span late[] = {{0x0400, 1}, {0xDE80, 1}};
    assert(check(late, 2) == C::HitsIO);
    return 0;
}
