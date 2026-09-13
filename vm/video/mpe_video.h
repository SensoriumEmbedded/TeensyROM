#ifndef MPE_VIDEO_H
#define MPE_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#include "mpe_video_policy.h"

#ifndef MPE_VIDEO_CODE
#define MPE_VIDEO_CODE
#endif

namespace mpe_video {

enum class StageResult : uint8_t {
  Accepted = 0,
  DroppedBusy,
  InvalidArgument
};

// A plan describes one complete immutable staged target. Enhanced bands use
// screen byte 8 above split[band] and byte 9 from that scanline downward.
// split[] is zero for bands whose bit is clear in enhancedMask.
struct FramePlan {
  RenderMode mode = RenderMode::Color;
  uint8_t background = 0;
  uint32_t enhancedMask = 0;
  uint8_t split[25] = {};

  bool bandEnhanced(uint8_t band) const {
    return band < 25u && (enhancedMask & (uint32_t(1u) << band)) != 0;
  }
};

// Converts a complete 320x200 indexed8 surface and 256-entry RGB palette into
// the C64 cell representation used by MPE's existing transport. The converter
// is intentionally independent of the VM ABI, packet protocol and VIC raster
// implementation.
//
// Each target cell contains eight bitmap bytes followed by two attribute bytes.
// Color uses multicolor bitmap data, screen byte 8 and color byte 9. Sharp uses
// hires bitmap data and mirrors its one screen attribute into bytes 8 and 9.
// Auto8 and Enhanced25 use hires data; selected bands store the independently
// reduced upper/lower screen attributes in bytes 8 and 9.
class Video {
 public:
  static constexpr uint16_t SourceWidth = 320;
  static constexpr uint16_t SourceHeight = 200;
  static constexpr uint8_t Bands = 25;
  static constexpr uint8_t CellsPerBand = 40;
  static constexpr uint16_t Cells = uint16_t(Bands) * CellsPerBand;
  static constexpr uint8_t RecordBytes = 12;
  static constexpr uint8_t CellBytes = 10;
  static constexpr uint8_t MaximumRecordsPerBatch = 19;
  static constexpr size_t SourceBytes = size_t(SourceWidth) * SourceHeight;
  static constexpr size_t PaletteBytes = 256u * 3u;
  static constexpr size_t FrameBytes = size_t(Cells) * CellBytes;
  static constexpr size_t DirtyBytes = (Cells + 7u) / 8u;
  static constexpr size_t CellSamples = 8u * 8u;
  static constexpr size_t VicColors = 16u;
  static constexpr size_t ScratchWords =
      CellSamples * VicColors + CellSamples;
  static constexpr size_t ScratchBytes = ScratchWords * sizeof(uint32_t);
  static constexpr size_t PublishedBytes = FrameBytes * 2u + DirtyBytes;
  static constexpr size_t WorkspaceBytes =
      PublishedBytes + (alignof(uint32_t) - 1u) + ScratchBytes;
  static constexpr uint8_t DefaultBackgroundColor = 0;
  static constexpr uint32_t AllBandsMask = (uint32_t(1u) << Bands) - 1u;

  MPE_VIDEO_CODE bool start(void *workspace, size_t bytes);
  MPE_VIDEO_CODE void reset();

  // The framebuffer and palette are consumed during this call and may be
  // reused immediately afterward. If records from an earlier target remain,
  // the new frame is dropped without changing that target or its frame plan.
  MPE_VIDEO_CODE StageResult stageFrame(const uint8_t *framebuffer,
                                        size_t framebufferBytes,
                                        const uint8_t *paletteRgb,
                                        size_t paletteBytes,
                                        RenderMode mode,
                                        uint8_t background =
                                            DefaultBackgroundColor);

  // Begins one transport-sized batch of at most maximum ascending 12-byte
  // records (cell index little-endian followed by ten target bytes). maximum is
  // clamped to MaximumRecordsPerBatch. The target stays busy and unchanged
  // until the caller acknowledges or rejects this whole batch.
  MPE_VIDEO_CODE uint16_t changes(uint8_t *records, uint16_t maximum);
  MPE_VIDEO_CODE bool acknowledgeChanges();
  MPE_VIDEO_CODE void rejectChanges();

  bool busy() const { return pendingCells_ != 0; }
  bool awaitingAcknowledgement() const { return offeredCount_ != 0; }
  bool initialComplete() const { return initialComplete_; }
  uint16_t pendingCells() const { return pendingCells_; }
  uint32_t acceptedFrames() const { return acceptedFrames_; }
  uint32_t droppedFrames() const { return droppedFrames_; }
  const FramePlan &framePlan() const { return targetPlan_; }
  const FramePlan &plan() const { return targetPlan_; }
  const FramePlan &publishedPlan() const { return publishedPlan_; }

 private:
  struct PairChoice {
    uint32_t error;
    uint8_t low;
    uint8_t high;
  };

  uint8_t *target_ = nullptr;
  uint8_t *shown_ = nullptr;
  uint8_t *dirty_ = nullptr;
  uint32_t *distanceScratch_ = nullptr;
  uint32_t *sampleRgbScratch_ = nullptr;
  uint16_t pendingCells_ = 0;
  uint16_t cursor_ = 0;
  uint16_t offeredCells_[MaximumRecordsPerBatch] = {};
  uint16_t offeredEndCursor_ = 0;
  uint8_t offeredCount_ = 0;
  uint32_t acceptedFrames_ = 0;
  uint32_t droppedFrames_ = 0;
  bool initialComplete_ = false;
  FramePlan targetPlan_{};
  FramePlan publishedPlan_{};

  MPE_VIDEO_CODE void renderColorCell(const uint8_t *framebuffer,
                                      const uint8_t *paletteRgb,
                                      uint16_t cell, uint8_t background,
                                      uint8_t out[CellBytes]);
  MPE_VIDEO_CODE PairChoice choosePair(const uint8_t *framebuffer,
                                       const uint8_t *paletteRgb,
                                       uint16_t cell, uint8_t firstRow,
                                       uint8_t rows, uint8_t *bitmap);
  MPE_VIDEO_CODE uint32_t renderSharpCell(const uint8_t *framebuffer,
                                          const uint8_t *paletteRgb,
                                          uint16_t cell,
                                          uint8_t out[CellBytes]);
  MPE_VIDEO_CODE void renderEnhancedCell(const uint8_t *framebuffer,
                                         const uint8_t *paletteRgb,
                                         uint16_t cell, uint8_t split,
                                         uint8_t out[CellBytes]);
  MPE_VIDEO_CODE FramePlan renderFrame(const uint8_t *framebuffer,
                                       const uint8_t *paletteRgb,
                                       RenderMode mode, uint8_t background);
};

// Sixteen consecutive RGB triples indexed by the VIC-II color number.
MPE_VIDEO_CODE const uint8_t *vicPaletteRgb();

}  // namespace mpe_video

#endif
