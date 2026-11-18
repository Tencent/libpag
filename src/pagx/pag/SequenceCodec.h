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

#include <cstdint>
#include <memory>
#include <vector>
#include "pagx/types/Data.h"

namespace pagx {

/**
 * In-memory form of a video sequence container (spec 6.6.1, mediaType 2): codec headers plus
 * the frame list with per-frame presentation indices and raw H.264 payloads.
 */
struct VideoSequenceFrame {
  bool isKeyframe = false;
  uint32_t frameIndex = 0;
  std::shared_ptr<Data> data = nullptr;
};

/**
 * An inclusive range of sequence frames whose content does not change: every frame inside it shows
 * the range's first frame (the encoder does not store meaningful pictures for the rest).
 */
struct SequenceStaticRange {
  uint32_t start = 0;
  uint32_t end = 0;
};

struct VideoSequenceContainer {
  float frameRate = 30.0f;
  uint32_t width = 0;
  uint32_t height = 0;
  int32_t alphaStartX = 0;
  int32_t alphaStartY = 0;
  std::vector<std::shared_ptr<Data>> headers = {};
  std::vector<VideoSequenceFrame> frames = {};
  std::vector<SequenceStaticRange> staticRanges = {};
};

/**
 * In-memory form of a bitmap sequence container (spec 6.6.1, mediaType 3): every frame carries
 * a list of positioned bitmap blocks.
 */
struct BitmapSequenceBlock {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  std::shared_ptr<Data> data = nullptr;
};

struct BitmapSequenceFrame {
  bool isKeyframe = false;
  std::vector<BitmapSequenceBlock> bitmaps = {};
};

struct BitmapSequenceContainer {
  float frameRate = 30.0f;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<BitmapSequenceFrame> frames = {};
};

/**
 * Encodes and decodes the sequence container layout of the ImageTable sequence entries
 * (spec 6.6.1). Frame payloads are stored as-is (no re-encode); decoding fails cleanly on
 * malformed or truncated containers.
 */
class SequenceCodec {
 public:
  static std::shared_ptr<Data> EncodeVideoSequence(const VideoSequenceContainer& container);

  static bool DecodeVideoSequence(const std::shared_ptr<Data>& data,
                                  VideoSequenceContainer* outContainer);

  static std::shared_ptr<Data> EncodeBitmapSequence(const BitmapSequenceContainer& container);

  static bool DecodeBitmapSequence(const std::shared_ptr<Data>& data,
                                   BitmapSequenceContainer* outContainer);
};

}  // namespace pagx
