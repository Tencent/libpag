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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

namespace pag {
/**
 * The frame size described by an H.264 sequence parameter set. The values are the coded size, which
 * is the size of the frame buffer a decoder allocates for the stream, so it can be a little larger
 * than the cropped size the decoder displays.
 */
struct H264FrameSize {
  int width = 0;
  int height = 0;
};

/**
 * Parses the coded frame size out of an H.264 sequence parameter set. The data must start with the
 * four byte NAL prefix that ReadByteDataWithStartCode() adds. Returns false if the data is not a
 * sequence parameter set or describes an unusable frame size.
 */
bool ParseH264SPSFrameSize(const uint8_t* data, size_t length, H264FrameSize* frameSize);
}  // namespace pag
