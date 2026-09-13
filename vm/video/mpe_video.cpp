#include "mpe_video.h"

#include <limits.h>
#include <string.h>

namespace mpe_video {
namespace {

const uint8_t kVicPalette[16][3] = {
    {0, 0, 0},       {255, 255, 255}, {136, 57, 50},   {103, 182, 189},
    {139, 63, 150},  {85, 160, 73},   {64, 49, 141},   {191, 206, 114},
    {139, 84, 41},   {87, 66, 0},     {184, 105, 98},  {80, 80, 80},
    {120, 120, 120}, {148, 224, 137}, {120, 105, 196}, {159, 159, 159}};

uint32_t packedRgb(const uint8_t *rgb) {
  return uint32_t(rgb[0]) | (uint32_t(rgb[1]) << 8u) |
         (uint32_t(rgb[2]) << 16u);
}

uint32_t distanceSquared(uint32_t rgb, uint8_t color) {
  uint32_t total = 0;
  for (uint8_t channel = 0; channel != 3; ++channel) {
    const int32_t delta = int32_t(uint8_t(rgb >> (channel * 8u))) -
                          kVicPalette[color][channel];
    total += uint32_t(delta * delta);
  }
  return total;
}

bool validMode(RenderMode mode) {
  return mode == RenderMode::Color || mode == RenderMode::Sharp ||
         mode == RenderMode::Auto8 || mode == RenderMode::Enhanced25;
}

bool samePlan(const FramePlan &left, const FramePlan &right) {
  if (left.mode != right.mode || left.background != right.background ||
      left.enhancedMask != right.enhancedMask)
    return false;
  for (uint8_t band = 0; band != Video::Bands; ++band)
    if (left.split[band] != right.split[band]) return false;
  return true;
}

}  // namespace

const uint8_t *vicPaletteRgb() { return &kVicPalette[0][0]; }

bool Video::start(void *workspace, size_t bytes) {
  // A rejected rebind detaches first: the old arena may already have been
  // returned to another VM by the time start() is called again.
  target_ = nullptr;
  shown_ = nullptr;
  dirty_ = nullptr;
  distanceScratch_ = nullptr;
  sampleRgbScratch_ = nullptr;
  pendingCells_ = cursor_ = offeredEndCursor_ = 0;
  offeredCount_ = 0;
  acceptedFrames_ = droppedFrames_ = 0;
  initialComplete_ = false;
  targetPlan_ = FramePlan{};
  publishedPlan_ = FramePlan{};
  if (!workspace || bytes < WorkspaceBytes) return false;

  target_ = static_cast<uint8_t *>(workspace);
  shown_ = target_ + FrameBytes;
  dirty_ = shown_ + FrameBytes;
  const uintptr_t scratchAddress =
      (reinterpret_cast<uintptr_t>(dirty_ + DirtyBytes) +
       alignof(uint32_t) - 1u) &
      ~uintptr_t(alignof(uint32_t) - 1u);
  distanceScratch_ = reinterpret_cast<uint32_t *>(scratchAddress);
  sampleRgbScratch_ = distanceScratch_ + CellSamples * VicColors;
  reset();
  return true;
}

void Video::reset() {
  if (target_) memset(target_, 0, WorkspaceBytes);
  pendingCells_ = cursor_ = offeredEndCursor_ = 0;
  offeredCount_ = 0;
  acceptedFrames_ = droppedFrames_ = 0;
  initialComplete_ = false;
  targetPlan_ = FramePlan{};
  publishedPlan_ = FramePlan{};
}

void Video::renderColorCell(const uint8_t *framebuffer,
                            const uint8_t *paletteRgb, uint16_t cell,
                            uint8_t background,
                            uint8_t out[CellBytes]) {
  uint8_t weights[CellSamples] = {};
  uint8_t sampleToUnique[CellSamples];
  const uint16_t cellX = uint16_t((cell % CellsPerBand) * 8u);
  const uint16_t cellY = uint16_t((cell / CellsPerBand) * 8u);

  uint8_t sample = 0;
  uint8_t uniqueSamples = 0;
  for (uint8_t y = 0; y != 8; ++y) {
    const size_t row = size_t(cellY + y) * SourceWidth;
    for (uint8_t x = 0; x != 4; ++x, ++sample) {
      const size_t left = row + cellX + x * 2u;
      const uint8_t leftIndex = framebuffer[left];
      const uint8_t rightIndex = framebuffer[left + 1u];
      uint32_t rgb = 0;
      for (uint8_t channel = 0; channel != 3; ++channel) {
        const uint16_t sum =
            uint16_t(paletteRgb[size_t(leftIndex) * 3u + channel]) +
            paletteRgb[size_t(rightIndex) * 3u + channel];
        rgb |= uint32_t(uint8_t((sum + 1u) >> 1u)) << (channel * 8u);
      }

      uint8_t unique = 0;
      while (unique != uniqueSamples && sampleRgbScratch_[unique] != rgb)
        ++unique;
      if (unique == uniqueSamples) {
        sampleRgbScratch_[unique] = rgb;
        uint32_t *distances = distanceScratch_ + size_t(unique) * VicColors;
        for (uint8_t color = 0; color != VicColors; ++color)
          distances[color] = distanceSquared(rgb, color);
        ++uniqueSamples;
      }
      sampleToUnique[sample] = unique;
      ++weights[unique];
    }
  }

  // Ascending exhaustive search plus strict replacement makes equal-cost
  // triples lexicographic. The firmware-selected shared background occupies
  // slot zero and is excluded from the three cell-local colors.
  uint32_t bestError = UINT32_MAX;
  uint8_t best[3] = {};
  for (uint8_t first = 0; first <= 13; ++first) {
    if (first == background) continue;
    for (uint8_t second = uint8_t(first + 1u); second <= 14; ++second) {
      if (second == background) continue;
      for (uint8_t third = uint8_t(second + 1u); third <= 15; ++third) {
        if (third == background) continue;
        uint32_t error = 0;
        for (uint8_t unique = 0; unique != uniqueSamples; ++unique) {
          const uint32_t *distances =
              distanceScratch_ + size_t(unique) * VicColors;
          uint32_t closest = distances[background];
          if (distances[first] < closest) closest = distances[first];
          if (distances[second] < closest) closest = distances[second];
          if (distances[third] < closest) closest = distances[third];
          error += closest * weights[unique];
        }
        if (error < bestError) {
          bestError = error;
          best[0] = first;
          best[1] = second;
          best[2] = third;
        }
      }
    }
  }

  memset(out, 0, CellBytes);
  sample = 0;
  for (uint8_t y = 0; y != 8; ++y) {
    for (uint8_t x = 0; x != 4; ++x, ++sample) {
      const uint32_t *distances =
          distanceScratch_ + size_t(sampleToUnique[sample]) * VicColors;
      uint8_t slot = 0;
      uint32_t closest = distances[background];
      for (uint8_t candidate = 0; candidate != 3; ++candidate) {
        if (distances[best[candidate]] < closest) {
          closest = distances[best[candidate]];
          slot = uint8_t(candidate + 1u);
        }
      }
      out[y] |= uint8_t(slot << (6u - x * 2u));
    }
  }
  out[8] = uint8_t((best[0] << 4u) | best[1]);
  out[9] = best[2];
}

Video::PairChoice Video::choosePair(const uint8_t *framebuffer,
                                    const uint8_t *paletteRgb, uint16_t cell,
                                    uint8_t firstRow, uint8_t rows,
                                    uint8_t *bitmap) {
  uint8_t weights[CellSamples] = {};
  uint8_t sampleToUnique[CellSamples];
  const uint16_t cellX = uint16_t((cell % CellsPerBand) * 8u);
  const uint16_t cellY = uint16_t((cell / CellsPerBand) * 8u);

  uint8_t sample = 0;
  uint8_t uniqueSamples = 0;
  for (uint8_t localY = 0; localY != rows; ++localY) {
    const size_t row = size_t(cellY + firstRow + localY) * SourceWidth;
    for (uint8_t x = 0; x != 8; ++x, ++sample) {
      const uint8_t index = framebuffer[row + cellX + x];
      const uint32_t rgb = packedRgb(paletteRgb + size_t(index) * 3u);
      uint8_t unique = 0;
      while (unique != uniqueSamples && sampleRgbScratch_[unique] != rgb)
        ++unique;
      if (unique == uniqueSamples) {
        sampleRgbScratch_[unique] = rgb;
        uint32_t *distances = distanceScratch_ + size_t(unique) * VicColors;
        for (uint8_t color = 0; color != VicColors; ++color)
          distances[color] = distanceSquared(rgb, color);
        ++uniqueSamples;
      }
      sampleToUnique[sample] = unique;
      ++weights[unique];
    }
  }

  PairChoice best{UINT32_MAX, 0, 0};
  for (uint8_t low = 0; low != VicColors; ++low) {
    for (uint8_t high = low; high != VicColors; ++high) {
      uint32_t error = 0;
      for (uint8_t unique = 0; unique != uniqueSamples; ++unique) {
        const uint32_t *distances =
            distanceScratch_ + size_t(unique) * VicColors;
        const uint32_t closest = distances[low] <= distances[high]
                                     ? distances[low]
                                     : distances[high];
        error += closest * weights[unique];
      }
      if (error < best.error) best = PairChoice{error, low, high};
    }
  }

  if (bitmap) {
    sample = 0;
    for (uint8_t localY = 0; localY != rows; ++localY) {
      const uint8_t row = uint8_t(firstRow + localY);
      for (uint8_t x = 0; x != 8; ++x, ++sample) {
        const uint32_t *distances =
            distanceScratch_ + size_t(sampleToUnique[sample]) * VicColors;
        if (distances[best.high] < distances[best.low])
          bitmap[row] |= uint8_t(0x80u >> x);
      }
    }
  }
  return best;
}

uint32_t Video::renderSharpCell(const uint8_t *framebuffer,
                                const uint8_t *paletteRgb, uint16_t cell,
                                uint8_t out[CellBytes]) {
  memset(out, 0, CellBytes);
  const PairChoice pair =
      choosePair(framebuffer, paletteRgb, cell, 0, 8, out);
  const uint8_t screen = uint8_t((pair.high << 4u) | pair.low);
  out[8] = screen;
  out[9] = screen;
  return pair.error;
}

void Video::renderEnhancedCell(const uint8_t *framebuffer,
                               const uint8_t *paletteRgb, uint16_t cell,
                               uint8_t split, uint8_t out[CellBytes]) {
  memset(out, 0, CellBytes);
  const PairChoice upper =
      choosePair(framebuffer, paletteRgb, cell, 0, split, out);
  const PairChoice lower = choosePair(framebuffer, paletteRgb, cell, split,
                                      uint8_t(8u - split), out);
  out[8] = uint8_t((upper.high << 4u) | upper.low);
  out[9] = uint8_t((lower.high << 4u) | lower.low);
}

FramePlan Video::renderFrame(const uint8_t *framebuffer,
                             const uint8_t *paletteRgb, RenderMode mode,
                             uint8_t background) {
  FramePlan plan{};
  plan.mode = mode;
  plan.background = mode == RenderMode::Color ? background
                                               : DefaultBackgroundColor;
  if (mode == RenderMode::Color) {
    for (uint16_t cell = 0; cell != Cells; ++cell)
      renderColorCell(framebuffer, paletteRgb, cell, background,
                      target_ + size_t(cell) * CellBytes);
    return plan;
  }

  uint32_t sharpError[Bands] = {};
  for (uint16_t cell = 0; cell != Cells; ++cell) {
    const uint8_t band = uint8_t(cell / CellsPerBand);
    sharpError[band] +=
        renderSharpCell(framebuffer, paletteRgb, cell,
                        target_ + size_t(cell) * CellBytes);
  }
  if (mode == RenderMode::Sharp) return plan;

  uint32_t gain[Bands] = {};
  uint8_t candidateSplit[Bands] = {};
  for (uint8_t band = 0; band != Bands; ++band) {
    uint32_t bestError = UINT32_MAX;
    uint8_t bestSplit = 1;
    for (uint8_t split = 1; split != 8; ++split) {
      uint32_t error = 0;
      for (uint8_t column = 0; column != CellsPerBand; ++column) {
        const uint16_t cell = uint16_t(band) * CellsPerBand + column;
        error += choosePair(framebuffer, paletteRgb, cell, 0, split, nullptr)
                     .error;
        error += choosePair(framebuffer, paletteRgb, cell, split,
                            uint8_t(8u - split), nullptr)
                     .error;
      }
      if (error < bestError) {
        bestError = error;
        bestSplit = split;
      }
    }
    if (bestError < sharpError[band]) {
      gain[band] = sharpError[band] - bestError;
      candidateSplit[band] = bestSplit;
    }
  }

  if (mode == RenderMode::Enhanced25) {
    for (uint8_t band = 0; band != Bands; ++band)
      if (gain[band]) plan.enhancedMask |= uint32_t(1u) << band;
  } else {
    // Repeated maximum selection avoids a sorting dependency. Equal gains
    // choose the lower band number, making the top-eight result stable.
    uint32_t selected = 0;
    for (uint8_t slot = 0; slot != 8; ++slot) {
      uint32_t bestGain = 0;
      uint8_t bestBand = Bands;
      for (uint8_t band = 0; band != Bands; ++band) {
        if ((selected & (uint32_t(1u) << band)) != 0 || !gain[band]) continue;
        if (gain[band] > bestGain ||
            (gain[band] == bestGain && band < bestBand)) {
          bestGain = gain[band];
          bestBand = band;
        }
      }
      if (bestBand == Bands) break;
      selected |= uint32_t(1u) << bestBand;
    }
    plan.enhancedMask = selected;
  }

  for (uint8_t band = 0; band != Bands; ++band) {
    if (!plan.bandEnhanced(band)) continue;
    plan.split[band] = candidateSplit[band];
    for (uint8_t column = 0; column != CellsPerBand; ++column) {
      const uint16_t cell = uint16_t(band) * CellsPerBand + column;
      renderEnhancedCell(framebuffer, paletteRgb, cell, plan.split[band],
                         target_ + size_t(cell) * CellBytes);
    }
  }
  return plan;
}

StageResult Video::stageFrame(const uint8_t *framebuffer,
                              size_t framebufferBytes,
                              const uint8_t *paletteRgb, size_t paletteBytes,
                              RenderMode mode, uint8_t background) {
  if (!target_ || !framebuffer || framebufferBytes < SourceBytes ||
      !paletteRgb || paletteBytes < PaletteBytes || !validMode(mode) ||
      background >= VicColors)
    return StageResult::InvalidArgument;
  if (busy()) {
    ++droppedFrames_;
    return StageResult::DroppedBusy;
  }

  const FramePlan nextPlan =
      renderFrame(framebuffer, paletteRgb, mode, background);
  const bool replaceAll =
      !initialComplete_ || !samePlan(nextPlan, publishedPlan_);
  targetPlan_ = nextPlan;
  memset(dirty_, 0, DirtyBytes);
  pendingCells_ = 0;
  cursor_ = 0;
  for (uint16_t cell = 0; cell != Cells; ++cell) {
    const uint8_t *target = target_ + size_t(cell) * CellBytes;
    if (replaceAll ||
        memcmp(target, shown_ + size_t(cell) * CellBytes, CellBytes)) {
      dirty_[cell >> 3u] |= uint8_t(1u << (cell & 7u));
      ++pendingCells_;
    }
  }
  ++acceptedFrames_;
  return StageResult::Accepted;
}

uint16_t Video::changes(uint8_t *records, uint16_t maximum) {
  if (!target_ || !records || !maximum || !pendingCells_ || offeredCount_)
    return 0;
  const uint16_t limit = maximum < MaximumRecordsPerBatch
                             ? maximum
                             : MaximumRecordsPerBatch;
  uint16_t count = 0;
  uint16_t scan = cursor_;
  while (scan != Cells && count != limit) {
    const uint16_t cell = scan++;
    const uint8_t mask = uint8_t(1u << (cell & 7u));
    if (!(dirty_[cell >> 3u] & mask)) continue;

    uint8_t *record = records + size_t(count) * RecordBytes;
    record[0] = uint8_t(cell);
    record[1] = uint8_t(cell >> 8u);
    const uint8_t *target = target_ + size_t(cell) * CellBytes;
    memcpy(record + 2, target, CellBytes);
    offeredCells_[count] = cell;
    ++count;
  }
  if (count) {
    offeredCount_ = uint8_t(count);
    offeredEndCursor_ = scan;
  }
  return count;
}

bool Video::acknowledgeChanges() {
  if (!target_ || !offeredCount_) return false;

  // Validate the entire offer before mutating publication state. An internal
  // inconsistency must never turn a packet acknowledgement into a partial
  // commit.
  for (uint8_t index = 0; index != offeredCount_; ++index) {
    const uint16_t cell = offeredCells_[index];
    if (cell >= Cells ||
        !(dirty_[cell >> 3u] & uint8_t(1u << (cell & 7u))))
      return false;
  }
  for (uint8_t index = 0; index != offeredCount_; ++index) {
    const uint16_t cell = offeredCells_[index];
    const uint8_t mask = uint8_t(1u << (cell & 7u));
    dirty_[cell >> 3u] &= uint8_t(~mask);
    memcpy(shown_ + size_t(cell) * CellBytes,
           target_ + size_t(cell) * CellBytes, CellBytes);
    --pendingCells_;
  }
  cursor_ = offeredEndCursor_;
  offeredCount_ = 0;
  offeredEndCursor_ = cursor_;

  if (!pendingCells_) {
    cursor_ = 0;
    offeredEndCursor_ = 0;
    initialComplete_ = true;
    publishedPlan_ = targetPlan_;
  }
  return true;
}

void Video::rejectChanges() {
  offeredCount_ = 0;
  offeredEndCursor_ = cursor_;
}

}  // namespace mpe_video
