// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// A run of C64 memory to write by DMA.  Pure, so the native tests can hold it to its rules.
struct C64Span { uint16_t Addr, Len; };

// A slice can wait ~5 mS to start (a C64 loop that only reads, screen blanked), so the slice
// cap keeps a job that plays by the rules to about 5 S.
enum : uint32_t { C64SpansMax = 64, C64SlicesMax = 1024 };

enum class C64SpansCheck : uint8_t { OK, BadCount, EmptySpan, Wraps, HitsIO, TooBig, TooManySlices };

// WriteC64Spans header flags.  LockToStore starts each slice on the cycle after the C64 writes
// StoreAddr, which the first span-list entry names in place of a span: a sample player's store to
// its DAC comes right after its interrupt is acknowledged, so a slice that starts there and fits
// the rest of the period never spans a second timer underflow.
enum : uint8_t { C64SpansFlagLockToStore = 0x01, C64SpansFlagsKnown = C64SpansFlagLockToStore };

// Counters of the sample store watch, IntervalCyc and LongestCyc in ARM cycles.  Locked and
// Unlocked count the slices that did and did not find a store to start on.
struct SampleWatchStats { uint32_t Intervals; uint64_t IntervalCyc; uint32_t LongestCyc, Locked, Unlocked; };

// Every span must end at or below $FFFF and stay off $DE00-$DFFF, which is this cartridge's own
// IO1/IO2 and not memory.  *Total is the payload they need, in span order, when OK.  SliceBytes
// 0 is a span per slice.
static inline C64SpansCheck CheckC64Spans(const C64Span *Spans, uint32_t Count, uint32_t SliceBytes,
                                          uint32_t BufferBytes, uint32_t *Total)
{
   if (Count == 0 || Count > C64SpansMax) return C64SpansCheck::BadCount;
   uint32_t Sum = 0, Slices = 0;
   for (uint32_t Num = 0; Num < Count; Num++)
   {
      const uint32_t Start = Spans[Num].Addr, End = Start + Spans[Num].Len;
      if (Spans[Num].Len == 0) return C64SpansCheck::EmptySpan;
      if (End > 0x10000) return C64SpansCheck::Wraps;
      if (Start < 0xE000 && End > 0xDE00) return C64SpansCheck::HitsIO;
      Sum += Spans[Num].Len;
      Slices += SliceBytes ? (Spans[Num].Len + SliceBytes - 1) / SliceBytes : 1;
   }
   if (Sum > BufferBytes) return C64SpansCheck::TooBig;
   if (Slices > C64SlicesMax) return C64SpansCheck::TooManySlices;
   *Total = Sum;
   return C64SpansCheck::OK;
}
