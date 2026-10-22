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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <cstdint>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/types/Color.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/types/Padding.h"

namespace pagx {

// Value IO for the PAG v4 binary format (spec/pag_binary_spec.md §3). Varuints and
// sign-in-LSB varints map onto the legacy stream helpers with identical semantics, so
// they are used directly.

/**
 * Colors (spec §3.7): a leading type byte, 0 = sRGB with four u8 components, 1 = extended
 * with a color space byte plus four float32 components. sRGB in-range colors must use
 * type 0; P3 or out-of-range components must use type 1.
 */
inline void WritePAGColor(pag::EncodeStream* stream, const Color& color) {
  auto inRange = [](float v) { return v >= 0.0f && v <= 1.0f; };
  if (color.colorSpace == ColorSpace::SRGB && inRange(color.red) && inRange(color.green) &&
      inRange(color.blue) && inRange(color.alpha)) {
    stream->writeUint8(0);
    stream->writeUint8(static_cast<uint8_t>(lroundf(color.red * 255.0f)));
    stream->writeUint8(static_cast<uint8_t>(lroundf(color.green * 255.0f)));
    stream->writeUint8(static_cast<uint8_t>(lroundf(color.blue * 255.0f)));
    stream->writeUint8(static_cast<uint8_t>(lroundf(color.alpha * 255.0f)));
  } else {
    stream->writeUint8(1);
    stream->writeUint8(static_cast<uint8_t>(color.colorSpace));
    stream->writeFloat(color.red);
    stream->writeFloat(color.green);
    stream->writeFloat(color.blue);
    stream->writeFloat(color.alpha);
  }
}

inline Color ReadPAGColor(pag::DecodeStream* stream) {
  Color color = {};
  auto type = stream->readUint8();
  if (type == 0) {
    color.red = static_cast<float>(stream->readUint8()) / 255.0f;
    color.green = static_cast<float>(stream->readUint8()) / 255.0f;
    color.blue = static_cast<float>(stream->readUint8()) / 255.0f;
    color.alpha = static_cast<float>(stream->readUint8()) / 255.0f;
    color.colorSpace = ColorSpace::SRGB;
  } else {
    color.colorSpace = static_cast<ColorSpace>(stream->readUint8());
    color.red = stream->readFloat();
    color.green = stream->readFloat();
    color.blue = stream->readFloat();
    color.alpha = stream->readFloat();
  }
  return color;
}

/**
 * 2D matrices (spec §3.8): six float32 in a, b, c, d, tx, ty order.
 */
inline void WritePAGMatrix(pag::EncodeStream* stream, const Matrix& matrix) {
  stream->writeFloat(matrix.a);
  stream->writeFloat(matrix.b);
  stream->writeFloat(matrix.c);
  stream->writeFloat(matrix.d);
  stream->writeFloat(matrix.tx);
  stream->writeFloat(matrix.ty);
}

inline Matrix ReadPAGMatrix(pag::DecodeStream* stream) {
  Matrix matrix = {};
  matrix.a = stream->readFloat();
  matrix.b = stream->readFloat();
  matrix.c = stream->readFloat();
  matrix.d = stream->readFloat();
  matrix.tx = stream->readFloat();
  matrix.ty = stream->readFloat();
  return matrix;
}

/**
 * 3D matrices (spec §3.8): sixteen float32 in column-major values[] order.
 */
inline void WritePAGMatrix3D(pag::EncodeStream* stream, const Matrix3D& matrix) {
  for (int i = 0; i < 16; i++) {
    stream->writeFloat(matrix.values[i]);
  }
}

inline Matrix3D ReadPAGMatrix3D(pag::DecodeStream* stream) {
  Matrix3D matrix = {};
  for (int i = 0; i < 16; i++) {
    matrix.values[i] = stream->readFloat();
  }
  return matrix;
}

/**
 * Padding (spec §3.11): top, right, bottom, left.
 */
inline void WritePAGPadding(pag::EncodeStream* stream, const Padding& padding) {
  stream->writeFloat(padding.top);
  stream->writeFloat(padding.right);
  stream->writeFloat(padding.bottom);
  stream->writeFloat(padding.left);
}

inline Padding ReadPAGPadding(pag::DecodeStream* stream) {
  Padding padding = {};
  padding.top = stream->readFloat();
  padding.right = stream->readFloat();
  padding.bottom = stream->readFloat();
  padding.left = stream->readFloat();
  return padding;
}

/**
 * Frame times (spec §3.9): non-negative int64 frame numbers as varuints.
 */
inline void WritePAGFrame(pag::EncodeStream* stream, Frame frame) {
  stream->writeEncodedUint64(static_cast<uint64_t>(frame));
}

inline Frame ReadPAGFrame(pag::DecodeStream* stream) {
  return static_cast<Frame>(stream->readEncodedUint64());
}

}  // namespace pagx
