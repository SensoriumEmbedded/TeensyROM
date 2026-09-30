// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// A run of C64 memory to write by DMA.  Pure, so the native tests can hold it to its rules.
struct C64Span { uint16_t Addr, Len; };

enum : uint32_t { C64SpansMax = 64 };

enum class C64SpansCheck : uint8_t { OK, BadCount, EmptySpan, Wraps, HitsIO, TooBig };

// Every span must end at or below $FFFF and stay off $DE00-$DFFF, which is this cartridge's own
// IO1/IO2 and not memory.  *Total is the payload they need, in span order, when OK.
static inline C64SpansCheck CheckC64Spans(const C64Span *Spans, uint32_t Count, uint32_t BufferBytes, uint32_t *Total)
{
   if (Count == 0 || Count > C64SpansMax) return C64SpansCheck::BadCount;
   uint32_t Sum = 0;
   for (uint32_t Num = 0; Num < Count; Num++)
   {
      const uint32_t Start = Spans[Num].Addr, End = Start + Spans[Num].Len;
      if (Spans[Num].Len == 0) return C64SpansCheck::EmptySpan;
      if (End > 0x10000) return C64SpansCheck::Wraps;
      if (Start < 0xE000 && End > 0xDE00) return C64SpansCheck::HitsIO;
      Sum += Spans[Num].Len;
   }
   if (Sum > BufferBytes) return C64SpansCheck::TooBig;
   *Total = Sum;
   return C64SpansCheck::OK;
}
