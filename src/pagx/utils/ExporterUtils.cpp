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

#include "pagx/utils/ExporterUtils.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/Base64.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Shader.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

static const uint8_t PNG_SIGNATURE[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

// Reads a big-endian 16-bit unsigned integer at `data`.
// Used by JPEG (segment length, SOF width/height, JFIF density) decoders.
static uint16_t ReadBE16(const uint8_t* data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               static_cast<uint16_t>(data[1]));
}

// Reads a big-endian 32-bit unsigned integer at `data`.
// Used by PNG (chunk lengths, IHDR width/height, pHYs ppm) decoders.
static uint32_t ReadBE32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
    if (info.fill && info.stroke && info.textBox) {
      break;
    }
  }
  return info;
}

Matrix BuildLayerMatrix(const Layer* layer) {
  // matrix3D > matrix > renderPosition. When matrix3D is non-identity it overrides
  // both matrix and renderPosition. We project it onto the z=0 plane to get the
  // equivalent 2D affine transform used by the 2D exporters (PPT/SVG).
  if (!layer->matrix3D.isIdentity()) {
    const float* v = layer->matrix3D.values;
    Matrix m = {};
    m.a = v[0];
    m.b = v[1];
    m.c = v[4];
    m.d = v[5];
    m.tx = v[12];
    m.ty = v[13];
    return m;
  }
  Matrix m = layer->matrix;
  // T(px,py) * m only shifts the translation column, so apply it directly to
  // avoid the full 6-multiply Matrix::operator* dance for the common case
  // where the layer's render position carries the translation. The authored
  // x/y attributes are layout inputs only; the layout pass resolves them
  // (together with constraints like left/right/top/bottom, flex, padding) into
  // renderPosition(), which is what the renderer uses.
  auto renderPos = layer->renderPosition();
  m.tx += renderPos.x;
  m.ty += renderPos.y;
  return m;
}

Matrix BuildGroupMatrix(const Group* group) {
  // The authored `position` attribute is a layout input only; the actual
  // rendered translation comes from renderPosition() after constraints,
  // padding, and flex allocation have been resolved. anchor, rotation,
  // scale, and skew are not layout-owned and are read directly.
  auto renderPos = group->renderPosition();
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    shear.c = std::tan(DegreesToRadians(group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(renderPos.x, renderPos.y) * m;
  }

  return m;
}

namespace {

struct PositionedGlyph {
  const Glyph* glyph = nullptr;
  Matrix transform;
};

// Resolves the layer-space pen position for glyph `i` of `run`. The GlyphRun
// exposes three positioning channels that take precedence in the order below;
// only the highest-priority channel present for this glyph contributes.
//   1. `positions[i]` — absolute offset from the run origin. When paired with
//      `xOffsets[i]` the x-offset is additive on top.
//   2. `xOffsets[i]` — absolute x offset from the run origin (y stays on the
//      run baseline).
//   3. Advance-accumulator fallback `currentX` — each glyph's pen position is
//      the sum of all prior glyph advances in the same run.
// `baseX`/`baseY` are the run origin (textPos + run->x / run->y) pre-hoisted
// by WalkGlyphs so we don't re-add them per glyph.
static void ResolveGlyphPosition(const GlyphRun* run, size_t i, float baseX, float baseY,
                                 float currentX, float* posX, float* posY) {
  if (i < run->positions.size()) {
    *posX = baseX + run->positions[i].x;
    *posY = baseY + run->positions[i].y;
    if (i < run->xOffsets.size()) {
      *posX += run->xOffsets[i];
    }
  } else if (i < run->xOffsets.size()) {
    *posX = baseX + run->xOffsets[i];
    *posY = baseY;
  } else {
    *posX = currentX;
    *posY = baseY;
  }
}

// Builds the layer-space transform for a single glyph, mirroring
// GlyphRunRenderer::BuildTextBlob: the font scale is applied first, then the
// pen translation, then (only if any of rotation/skew/glyph-scale is non-trivial)
// the rotation/skew/scale are composed around T(pos)*T(anchor). Anchor lives in
// user (post-font-scale) units. Matching this exact order is what keeps scaled
// glyphs from collapsing onto each other in PPT/SVG output.
static Matrix ComposeGlyphMatrix(const Glyph* glyph, const GlyphRun* run, size_t i, float scale,
                                 float posX, float posY) {
  Matrix glyphMatrix = Matrix::Scale(scale, scale);
  glyphMatrix = Matrix::Translate(posX, posY) * glyphMatrix;

  bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
  bool hasGlyphScale = i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
  bool hasSkew = i < run->skews.size() && run->skews[i] != 0;

  if (!hasRotation && !hasGlyphScale && !hasSkew) {
    return glyphMatrix;
  }

  float anchorX = glyph->advance * 0.5f * scale;
  float anchorY = 0;
  if (i < run->anchors.size()) {
    anchorX += run->anchors[i].x;
    anchorY += run->anchors[i].y;
  }

  glyphMatrix = Matrix::Translate(anchorX, anchorY) * glyphMatrix;
  if (hasRotation) {
    glyphMatrix = Matrix::Rotate(run->rotations[i]) * glyphMatrix;
  }
  if (hasSkew) {
    // Match GlyphRunRenderer::BuildTextBlob: skewX = -tan(angle). The
    // sign is what makes positive skew tilt the glyph forward (top-right)
    // once it's combined with the ascender-negative-Y glyph path coords.
    Matrix shear = {};
    shear.c = -std::tan(pag::DegreesToRadians(run->skews[i]));
    glyphMatrix = shear * glyphMatrix;
  }
  if (hasGlyphScale) {
    glyphMatrix = Matrix::Scale(run->scales[i].x, run->scales[i].y) * glyphMatrix;
  }
  glyphMatrix = Matrix::Translate(-anchorX, -anchorY) * glyphMatrix;
  return glyphMatrix;
}

// Shared per-glyph walker — both ComputeGlyphPaths (vector outlines) and
// ComputeGlyphImages (bitmap glyphs) need exactly the same positioning logic
// (positions / xOffsets / per-glyph anchor + rotation / skew / scale) and only
// differ in which glyph kind they emit. Keeping the math in one place ensures
// path glyphs and image glyphs in the same GlyphRun stay perfectly aligned.
//
// Returns every non-zero glyph in document order with the layer-space glyph
// matrix already composed. Callers filter for the kind they need (path vs.
// image) instead of the walker knowing about either.
static std::vector<PositionedGlyph> WalkGlyphs(const Text& text, float textPosX, float textPosY) {
  std::vector<PositionedGlyph> result;
  size_t totalGlyphs = 0;
  for (const auto* run : text.glyphRuns) {
    totalGlyphs += run->glyphs.size();
  }
  result.reserve(totalGlyphs);
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float baseX = textPosX + run->x;
    float baseY = textPosY + run->y;
    float currentX = baseX;
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (!glyph) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      ResolveGlyphPosition(run, i, baseX, baseY, currentX, &posX, &posY);
      // Advance the cursor for the no-positions / no-xOffsets fallback path
      // even when this particular glyph is later filtered out, so subsequent
      // glyphs land where the renderer expects them.
      currentX += glyph->advance * scale;

      result.push_back({glyph, ComposeGlyphMatrix(glyph, run, i, scale, posX, posY)});
    }
  }
  return result;
}

}  // namespace

namespace {

// Turns a PositionedGlyph into a GlyphImage entry if the glyph carries a
// bitmap, applying the design-space offset so pixel (0,0) maps to the image's
// top-left in layer space. Kept separate from the walker so ComputeGlyphImages
// and the combined API share identical image-placement math.
static bool TryMakeGlyphImage(const PositionedGlyph& entry, GlyphImage* out) {
  const auto* glyph = entry.glyph;
  if (!glyph->image) {
    return false;
  }
  Matrix imageMatrix = entry.transform;
  if (glyph->offset.x != 0 || glyph->offset.y != 0) {
    imageMatrix = imageMatrix * Matrix::Translate(glyph->offset.x, glyph->offset.y);
  }
  *out = {imageMatrix, glyph->image};
  return true;
}

static bool TryMakeGlyphPath(const PositionedGlyph& entry, GlyphPath* out) {
  if (!entry.glyph->path || entry.glyph->path->isEmpty()) {
    return false;
  }
  *out = {entry.transform, entry.glyph->path};
  return true;
}

}  // namespace

void ComputeGlyphPathsAndImages(const Text& text, float textPosX, float textPosY,
                                std::vector<GlyphPath>* paths, std::vector<GlyphImage>* images) {
  auto positioned = WalkGlyphs(text, textPosX, textPosY);
  if (paths) {
    paths->reserve(paths->size() + positioned.size());
  }
  if (images) {
    images->reserve(images->size() + positioned.size());
  }
  for (const auto& entry : positioned) {
    if (paths) {
      GlyphPath path = {};
      if (TryMakeGlyphPath(entry, &path)) {
        paths->push_back(path);
      }
    }
    if (images) {
      GlyphImage image = {};
      if (TryMakeGlyphImage(entry, &image)) {
        images->push_back(image);
      }
    }
  }
}

std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphPath> result;
  ComputeGlyphPathsAndImages(text, textPosX, textPosY, &result, nullptr);
  return result;
}

std::vector<GlyphImage> ComputeGlyphImages(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphImage> result;
  ComputeGlyphPathsAndImages(text, textPosX, textPosY, nullptr, &result);
  return result;
}

FillRule DetectMaskFillRule(const Layer* maskLayer) {
  for (const auto* element : maskLayer->contents) {
    if (element->nodeType() == NodeType::Fill) {
      auto rule = static_cast<const Fill*>(element)->fillRule;
      if (rule == FillRule::EvenOdd) {
        return FillRule::EvenOdd;
      }
    }
  }
  for (const auto* child : maskLayer->children) {
    if (DetectMaskFillRule(child) == FillRule::EvenOdd) {
      return FillRule::EvenOdd;
    }
  }
  return FillRule::Winding;
}

void DecomposeScale(const Matrix& m, float* sx, float* sy) {
  *sx = std::sqrt(m.a * m.a + m.b * m.b);
  float det = m.a * m.d - m.b * m.c;
  *sy = (*sx > 0) ? std::abs(det) / *sx : 0;
}

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 24) {
    return false;
  }
  if (memcmp(data, PNG_SIGNATURE, 8) != 0) {
    return false;
  }
  *width = static_cast<int>(ReadBE32(data + 16));
  *height = static_cast<int>(ReadBE32(data + 20));
  return *width > 0 && *height > 0;
}

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height) {
  if (path.rfind("data:", 0) == 0) {
    auto decoded = DecodeBase64DataURI(path);
    if (!decoded) {
      return false;
    }
    return GetPNGDimensions(decoded->bytes(), decoded->size(), width, height);
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  uint8_t header[24];
  if (!file.read(reinterpret_cast<char*>(header), 24)) {
    return false;
  }
  return GetPNGDimensions(header, 24, width, height);
}

bool GetImagePNGDimensions(const Image* image, int* width, int* height) {
  if (image->data) {
    return GetPNGDimensions(image->data->bytes(), image->data->size(), width, height);
  }
  if (!image->filePath.empty()) {
    return GetPNGDimensionsFromPath(image->filePath, width, height);
  }
  return false;
}

bool GetJPEGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segmentLength = static_cast<size_t>(ReadBE16(data + offset + 2)) + 2;
    // SOF0..SOF3 markers contain image dimensions.
    if (marker >= 0xC0 && marker <= 0xC3 && offset + 9 < size) {
      *height = static_cast<int>(ReadBE16(data + offset + 5));
      *width = static_cast<int>(ReadBE16(data + offset + 7));
      return *width > 0 && *height > 0;
    }
    offset += segmentLength;
  }
  return false;
}

bool GetWebPDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 16 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) {
    return false;
  }
  if (size < 20) {
    return false;
  }
  const uint8_t* chunk = data + 12;
  size_t remaining = size - 12;
  if (remaining >= 18 && memcmp(chunk, "VP8 ", 4) == 0) {
    const uint8_t* vp8 = chunk + 8;
    size_t vp8Size = remaining - 8;
    if (vp8Size >= 10 && vp8[3] == 0x9D && vp8[4] == 0x01 && vp8[5] == 0x2A) {
      *width = static_cast<int>((vp8[6] | (vp8[7] << 8)) & 0x3FFF);
      *height = static_cast<int>((vp8[8] | (vp8[9] << 8)) & 0x3FFF);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 13 && memcmp(chunk, "VP8L", 4) == 0) {
    const uint8_t* vp8l = chunk + 8;
    size_t vp8lSize = remaining - 8;
    if (vp8lSize >= 5 && vp8l[0] == 0x2F) {
      uint32_t bits = vp8l[1] | (static_cast<uint32_t>(vp8l[2]) << 8) |
                      (static_cast<uint32_t>(vp8l[3]) << 16) |
                      (static_cast<uint32_t>(vp8l[4]) << 24);
      *width = static_cast<int>((bits & 0x3FFF) + 1);
      *height = static_cast<int>(((bits >> 14) & 0x3FFF) + 1);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 18 && memcmp(chunk, "VP8X", 4) == 0) {
    const uint8_t* vp8x = chunk + 8;
    *width = static_cast<int>((vp8x[4] | (vp8x[5] << 8) | (vp8x[6] << 16)) + 1);
    *height = static_cast<int>((vp8x[7] | (vp8x[8] << 8) | (vp8x[9] << 16)) + 1);
    return *width > 0 && *height > 0;
  }
  return false;
}

bool GetImageDimensions(const Image* image, int* width, int* height) {
  if (GetImagePNGDimensions(image, width, height)) {
    return true;
  }
  auto data = GetImageData(image);
  if (data && data->size() > 0) {
    if (GetJPEGDimensions(data->bytes(), data->size(), width, height)) {
      return true;
    }
    return GetWebPDimensions(data->bytes(), data->size(), width, height);
  }
  return false;
}

bool GetPNGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 8 || memcmp(data, PNG_SIGNATURE, 8) != 0) {
    return false;
  }
  size_t offset = 8;
  while (offset + 12 <= size) {
    auto chunkLen = ReadBE32(data + offset);
    if (memcmp(data + offset + 4, "pHYs", 4) == 0) {
      if (chunkLen == 9 && offset + 8 + 9 <= size) {
        const uint8_t* d = data + offset + 8;
        auto ppuX = ReadBE32(d);
        auto ppuY = ReadBE32(d + 4);
        uint8_t unit = d[8];
        if (unit == 1 && ppuX > 0 && ppuY > 0) {
          *dpiX = static_cast<float>(ppuX) * 0.0254f;
          *dpiY = static_cast<float>(ppuY) * 0.0254f;
          return true;
        }
      }
      return false;
    }
    if (memcmp(data + offset + 4, "IDAT", 4) == 0) {
      break;
    }
    offset += 12 + chunkLen;
  }
  return false;
}

bool GetJPEGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segLen = ReadBE16(data + offset + 2);
    if (marker == 0xE0 && segLen >= 16 && offset + 2 + segLen <= size) {
      if (memcmp(data + offset + 4, "JFIF\0", 5) == 0) {
        uint8_t units = data[offset + 11];
        auto xDensity = ReadBE16(data + offset + 12);
        auto yDensity = ReadBE16(data + offset + 14);
        if (xDensity > 0 && yDensity > 0) {
          if (units == 1) {
            *dpiX = static_cast<float>(xDensity);
            *dpiY = static_cast<float>(yDensity);
            return true;
          } else if (units == 2) {
            *dpiX = static_cast<float>(xDensity) * 2.54f;
            *dpiY = static_cast<float>(yDensity) * 2.54f;
            return true;
          }
        }
      }
    }
    offset += 2 + segLen;
  }
  return false;
}

bool GetImageDPI(const Image* image, float* dpiX, float* dpiY) {
  auto data = GetImageData(image);
  if (!data || data->size() == 0) {
    return false;
  }
  if (GetPNGDPI(data->bytes(), data->size(), dpiX, dpiY)) {
    return true;
  }
  return GetJPEGDPI(data->bytes(), data->size(), dpiX, dpiY);
}

bool IsJPEG(const uint8_t* data, size_t size) {
  return size >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

bool IsWebP(const uint8_t* data, size_t size) {
  return size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
}

std::shared_ptr<tgfx::Data> ConvertWebPToPNG(const std::shared_ptr<tgfx::Data>& webpData) {
  if (!webpData) {
    return nullptr;
  }
  auto codec = tgfx::ImageCodec::MakeFrom(webpData);
  if (!codec) {
    return nullptr;
  }
  tgfx::Bitmap bitmap(codec->width(), codec->height(), false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!codec->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}

std::shared_ptr<tgfx::Data> GetImageData(const Image* image) {
  if (image->data) {
    return tgfx::Data::MakeWithoutCopy(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return tgfx::Data::MakeFromFile(image->filePath);
  }
  return nullptr;
}

bool HasNonASCII(const std::string& str) {
  for (unsigned char c : str) {
    if (c > 127) {
      return true;
    }
  }
  return false;
}

std::string UTF8ToUTF16BEHex(const std::string& utf8) {
  std::string hex;
  hex.reserve(utf8.size() * 6);
  size_t i = 0;
  while (i < utf8.size()) {
    uint32_t cp = 0;
    auto c = static_cast<unsigned char>(utf8[i]);
    size_t bytes = 1;
    if (c < 0x80) {
      cp = c;
    } else if (c < 0xE0) {
      cp = c & 0x1Fu;
      bytes = 2;
    } else if (c < 0xF0) {
      cp = c & 0x0Fu;
      bytes = 3;
    } else {
      cp = c & 0x07u;
      bytes = 4;
    }
    for (size_t j = 1; j < bytes && (i + j) < utf8.size(); j++) {
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + j]) & 0x3Fu);
    }
    i += bytes;
    char buf[9];
    if (cp <= 0xFFFF) {
      snprintf(buf, sizeof(buf), "%04X", cp);
    } else {
      cp -= 0x10000;
      snprintf(buf, sizeof(buf), "%04X%04X", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
    }
    hex += buf;
  }
  return hex;
}

// Shared tail for off-screen render helpers: reads the surface contents into a
// tightly packed pixmap and encodes it as PNG. Returns nullptr if the backing
// bitmap could not be allocated or the surface read failed.
static std::shared_ptr<tgfx::Data> EncodeSurfaceToPNG(const std::shared_ptr<tgfx::Surface>& surface,
                                                      int width, int height) {
  tgfx::Bitmap bitmap(width, height, false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}

// Common skeleton shared by every off-screen GPU render path in this file.
// Validates the requested size, allocates a Surface at `width * pixelScale` by
// `height * pixelScale` pixels, pre-applies a matching scale on the Canvas so
// that drawer commands can stay in the caller's logical coordinate space, then
// encodes the surface as PNG. The encoded PNG is the scaled pixel size — the
// caller is responsible for placing it at the original logical extent so that
// the consumer stretches the higher-density bitmap back over the same visible
// area. `Drawer` must be callable as `drawer(tgfx::Canvas*)`; named functors
// defined below supply the actual draw logic (the project forbids lambdas).
template <typename Drawer>
static std::shared_ptr<tgfx::Data> RenderToPNG(tgfx::Context* context, int width, int height,
                                               float pixelScale, Drawer drawer) {
  if (width <= 0 || height <= 0 || pixelScale <= 0.0f) {
    return nullptr;
  }
  int pixelWidth = static_cast<int>(std::ceil(static_cast<float>(width) * pixelScale));
  int pixelHeight = static_cast<int>(std::ceil(static_cast<float>(height) * pixelScale));
  if (pixelWidth <= 0 || pixelHeight <= 0) {
    return nullptr;
  }
  auto surface = tgfx::Surface::Make(context, pixelWidth, pixelHeight);
  if (surface == nullptr) {
    return nullptr;
  }
  auto* canvas = surface->getCanvas();
  canvas->scale(pixelScale, pixelScale);
  drawer(canvas);
  return EncodeSurfaceToPNG(surface, pixelWidth, pixelHeight);
}

// Draws `targetLayer` (and only that layer) into the off-screen canvas, with
// the global bounds origin mapped to (0,0) so the emitted PNG is tightly
// cropped. Used for mask / scrollRect bakes where the layer's visual result
// does not depend on the backdrop.
struct MaskedLayerDrawer {
  const std::shared_ptr<tgfx::Layer>& root;
  const std::shared_ptr<tgfx::Layer>& targetLayer;
  const tgfx::Rect& globalBounds;
  void operator()(tgfx::Canvas* canvas) const {
    canvas->translate(-globalBounds.left, -globalBounds.top);
    canvas->concat(targetLayer->getRelativeMatrix(root.get()));
    targetLayer->draw(canvas);
  }
};

// Draws the entire scene from `root` downward into the off-screen canvas,
// clipped to the target layer's global bounds. Used when the target layer's
// visual result depends on the backdrop pixels below it — e.g. a non-Normal
// `Layer.blendMode`, which the tgfx renderer composites against whatever has
// already been drawn underneath. Drawing only the target against an empty
// canvas (as MaskedLayerDrawer does) would blend it with transparent-black
// and lose the intended composite; drawing `root` with a clip preserves it.
struct BackdropCompositeDrawer {
  const std::shared_ptr<tgfx::Layer>& root;
  const tgfx::Rect& globalBounds;
  void operator()(tgfx::Canvas* canvas) const {
    canvas->translate(-globalBounds.left, -globalBounds.top);
    canvas->clipRect(globalBounds);
    root->draw(canvas);
  }
};

// Fills the entire canvas with a pre-built image shader to rasterize a tiled
// pattern into a PNG tile.
struct TiledPatternDrawer {
  std::shared_ptr<tgfx::Shader> shader;
  int width;
  int height;
  void operator()(tgfx::Canvas* canvas) const {
    tgfx::Paint paint;
    paint.setShader(shader);
    canvas->drawRect(tgfx::Rect::MakeWH(width, height), paint);
  }
};

GPUContext::~GPUContext() {
  _device = nullptr;
}

tgfx::Context* GPUContext::lockContext() {
  if (!_device) {
    _device = tgfx::GLDevice::Make();
    if (!_device) {
      return nullptr;
    }
  }
  return _device->lockContext();
}

void GPUContext::unlock() {
  if (_device) {
    _device->unlock();
  }
}

std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              float pixelScale) {
  auto globalBounds = targetLayer->getBounds(root.get(), true);
  auto context = gpu->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  int width = static_cast<int>(ceilf(globalBounds.width()));
  int height = static_cast<int>(ceilf(globalBounds.height()));
  auto result = RenderToPNG(context, width, height, pixelScale,
                            MaskedLayerDrawer{root, targetLayer, globalBounds});
  gpu->unlock();
  return result;
}

std::shared_ptr<tgfx::Data> RenderLayerCompositeWithBackdrop(
    GPUContext* gpu, const std::shared_ptr<tgfx::Layer>& root,
    const std::shared_ptr<tgfx::Layer>& targetLayer, float pixelScale) {
  auto globalBounds = targetLayer->getBounds(root.get(), true);
  auto context = gpu->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  int width = static_cast<int>(ceilf(globalBounds.width()));
  int height = static_cast<int>(ceilf(globalBounds.height()));
  auto result =
      RenderToPNG(context, width, height, pixelScale, BackdropCompositeDrawer{root, globalBounds});
  gpu->unlock();
  return result;
}

static tgfx::TileMode ToTGFXTileMode(TileMode mode) {
  switch (mode) {
    case TileMode::Repeat:
      return tgfx::TileMode::Repeat;
    case TileMode::Mirror:
      return tgfx::TileMode::Mirror;
    case TileMode::Decal:
      return tgfx::TileMode::Decal;
    default:
      return tgfx::TileMode::Clamp;
  }
}

std::shared_ptr<tgfx::Data> RenderTiledPattern(GPUContext* gpu, const ImagePattern* pattern,
                                               int width, int height, float offsetX, float offsetY,
                                               float pixelScale) {
  if (width <= 0 || height <= 0 || !pattern || !pattern->image) {
    return nullptr;
  }
  auto imageData = GetImageData(pattern->image);
  if (!imageData) {
    return nullptr;
  }
  auto tgfxImage = tgfx::Image::MakeFromEncoded(imageData);
  if (!tgfxImage) {
    return nullptr;
  }
  auto shader = tgfx::Shader::MakeImageShader(
      std::move(tgfxImage), ToTGFXTileMode(pattern->tileModeX), ToTGFXTileMode(pattern->tileModeY));
  if (!shader) {
    return nullptr;
  }
  const auto& M = pattern->matrix;
  tgfx::Matrix shaderMatrix = tgfx::Matrix::MakeAll(M.a, M.c, offsetX, M.b, M.d, offsetY);
  shader = shader->makeWithMatrix(shaderMatrix);
  if (!shader) {
    return nullptr;
  }
  auto context = gpu->lockContext();
  if (!context) {
    return nullptr;
  }
  auto result = RenderToPNG(context, width, height, pixelScale,
                            TiledPatternDrawer{std::move(shader), width, height});
  gpu->unlock();
  return result;
}

float EffectiveTextBoxWidth(const TextBox* box) {
  if (!std::isnan(box->width)) {
    return box->width;
  }
  // Use the layout-resolved width (set by the constraint layout pass) so TextBoxes
  // sized via left/right/centerX still wrap and lay out as the author intended.
  float resolved = box->layoutBounds().width;
  return resolved > 0 ? resolved : NAN;
}

float EffectiveTextBoxHeight(const TextBox* box) {
  if (!std::isnan(box->height)) {
    return box->height;
  }
  float resolved = box->layoutBounds().height;
  return resolved > 0 ? resolved : NAN;
}

TextLayoutParams MakeTextBoxParams(const TextBox* box) {
  bool hasPadding = !box->padding.isZero();
  float boxW = EffectiveTextBoxWidth(box);
  float boxH = EffectiveTextBoxHeight(box);
  if (hasPadding) {
    if (!std::isnan(boxW)) {
      boxW = std::max(0.0f, boxW - box->padding.left - box->padding.right);
    }
    if (!std::isnan(boxH)) {
      boxH = std::max(0.0f, boxH - box->padding.top - box->padding.bottom);
    }
  }
  TextLayoutParams params = {};
  params.boxWidth = boxW;
  params.boxHeight = boxH;
  params.textAlign = box->textAlign;
  params.paragraphAlign = box->paragraphAlign;
  params.writingMode = box->writingMode;
  params.lineHeight = box->lineHeight;
  params.wordWrap = box->wordWrap;
  params.overflow = box->overflow;
  return params;
}

TextLayoutParams MakeStandaloneParams(const Text* text) {
  TextLayoutParams params = {};
  params.baseline = text->baseline;
  switch (text->textAnchor) {
    case TextAnchor::Start:
      params.textAlign = TextAlign::Start;
      break;
    case TextAnchor::Center:
      params.textAlign = TextAlign::Center;
      break;
    case TextAnchor::End:
      params.textAlign = TextAlign::End;
      break;
  }
  return params;
}

std::string StripQuotes(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace pagx
