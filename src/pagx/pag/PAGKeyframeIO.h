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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/nodes/KeyValue.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/PathData.h"
#include "pagx/types/Color.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Point.h"

namespace pagx {

// Keyframe block IO (spec/pag_binary_spec.md §8): the columnar layout used inside Channel
// tags. One block per channel: key count varuint, 2-bit interpolation types, byte
// alignment, varuint times, one value column per ChannelValueType, then the bezier handles
// (four floats per Bezier keyframe at 0.005 precision). PAGX point-style keyframes carry a
// single value each, unlike the legacy start/end segments.

/**
 * One decoded keyframe block. Exactly one value column is populated, matching the channel's
 * declared value type. Path values are PathData nodes the caller creates through a document
 * and therefore owns; the block never deletes them.
 */
struct PAGKeyframeBlock {
  std::vector<Frame> times = {};
  std::vector<KeyframeInterpolationType> interpolations = {};
  std::vector<Point> bezierOut = {};
  std::vector<Point> bezierIn = {};
  std::vector<float> floatValues = {};
  std::vector<int32_t> intValues = {};
  std::vector<uint8_t> boolValues = {};
  std::vector<uint32_t> stringIndices = {};
  std::vector<Color> colorValues = {};
  std::vector<Matrix> matrixValues = {};
  std::vector<uint32_t> imageIndices = {};
  std::vector<PathData*> pathValues = {};
  // Step exponent applied to every path value on write (spec §8.1; phase 2 bakes geometry
  // scales, 0 keeps the base grid).
  uint8_t pathStepExponent = 0;

  /**
   * Empties all columns. Path values are not deleted; the owning document takes care of
   * them.
   */
  void clear();
};

/**
 * Writes one keyframe block for the given value type. All columns of the block must be
 * consistent: exactly valueCount() entries in the active value column, and times must be
 * non-decreasing.
 */
void WritePAGKeyframeBlock(pag::EncodeStream* stream, ChannelValueType type,
                           const PAGKeyframeBlock& block);

/**
 * Reads one keyframe block. Fills the value column matching the declared type and returns
 * false on malformed input: decreasing times, wrong column size, unknown value type, or a
 * broken path value.
 */
bool ReadPAGKeyframeBlock(pag::DecodeStream* stream, ChannelValueType type,
                          PAGKeyframeBlock* block);

}  // namespace pagx
