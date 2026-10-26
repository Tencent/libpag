/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include "tgfx/core/Image.h"

namespace pag {
class SequenceReader;
}  // namespace pag

namespace pagx {

struct LegacySequenceHolder;
class Image;
struct VideoSequenceContainer;
struct BitmapSequenceContainer;

/**
 * SequencePlayer drives the frame scheduling of an ImageTable sequence entry (spec 6.6.1,
 * design doc 3.3). It rebuilds the decoded container into the legacy in-memory sequence
 * structures and reuses the legacy demuxer/decoder stack (VideoSequenceDemuxer + VideoReader
 * with the platform decoder factories, or BitmapSequenceReader) unchanged. frameAt() returns a
 * lazy tgfx Image whose generator decodes the requested frame on demand; one reader instance
 * serves all frames so the decoder state machine handles seeking across time.
 */
class SequencePlayer {
 public:
  /**
   * Creates a player for a sequence image (isSequence with a decodable container). Returns
   * nullptr for non-sequence images or malformed containers.
   */
  static std::shared_ptr<SequencePlayer> Make(const Image* sequenceImage);

  /**
   * Returns the image of the frame at the given composition time (seconds). The frame index
   * follows the legacy TimeToFrame rounding and clamps to the sequence range. The returned
   * image decodes lazily on first draw.
   */
  std::shared_ptr<tgfx::Image> frameAt(double compositionTime);

  float frameRate() const;

  uint32_t frameCount() const;

  int width() const;

  int height() const;

 private:
  SequencePlayer() = default;

  std::shared_ptr<pag::SequenceReader> reader = nullptr;
  // The rebuilt legacy structures stay alive for the player's lifetime: the reader holds raw
  // pointers into them (sequence, frames, headers, and the owning composition).
  std::shared_ptr<LegacySequenceHolder> holder = nullptr;
  float _frameRate = 0.0f;
  uint32_t _frameCount = 0;
  int _width = 0;
  int _height = 0;
  // Video frames decode into a dual-region buffer (RGB area + alpha area, spec of the legacy
  // RGBAAA layout); makeRGBAAA resamples it into the display size. Bitmap frames are plain RGBA.
  bool _isVideo = false;
  int32_t _alphaStartX = 0;
  int32_t _alphaStartY = 0;

  friend std::shared_ptr<SequencePlayer> BuildVideoPlayer(
      const VideoSequenceContainer& container, const std::shared_ptr<LegacySequenceHolder>& holder);
  friend std::shared_ptr<SequencePlayer> BuildBitmapPlayer(
      const BitmapSequenceContainer& container,
      const std::shared_ptr<LegacySequenceHolder>& holder);
};
}  // namespace pagx
