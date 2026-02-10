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

#include "pagx/FontEmbedder.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "../MathUtil.h"
#include "SVGPathParser.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/GlyphRun.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"

namespace pagx {

static constexpr int VectorFontUnitsPerEm = 1000;

static std::string PathToSVGString(const tgfx::Path& path) {
  std::string result = {};
  result.reserve(256);
  char buf[64] = {};

  for (const auto& segment : path) {
    switch (segment.verb) {
      case tgfx::PathVerb::Move:
        snprintf(buf, sizeof(buf), "M%d %d", static_cast<int>(std::round(segment.points[0].x)),
                 static_cast<int>(std::round(segment.points[0].y)));
        result += buf;
        break;
      case tgfx::PathVerb::Line:
        snprintf(buf, sizeof(buf), "L%d %d", static_cast<int>(std::round(segment.points[1].x)),
                 static_cast<int>(std::round(segment.points[1].y)));
        result += buf;
        break;
      case tgfx::PathVerb::Quad:
        snprintf(buf, sizeof(buf), "Q%d %d %d %d", static_cast<int>(std::round(segment.points[1].x)),
                 static_cast<int>(std::round(segment.points[1].y)),
                 static_cast<int>(std::round(segment.points[2].x)),
                 static_cast<int>(std::round(segment.points[2].y)));
        result += buf;
        break;
      case tgfx::PathVerb::Cubic:
        snprintf(buf, sizeof(buf), "C%d %d %d %d %d %d",
                 static_cast<int>(std::round(segment.points[1].x)),
                 static_cast<int>(std::round(segment.points[1].y)),
                 static_cast<int>(std::round(segment.points[2].x)),
                 static_cast<int>(std::round(segment.points[2].y)),
                 static_cast<int>(std::round(segment.points[3].x)),
                 static_cast<int>(std::round(segment.points[3].y)));
        result += buf;
        break;
      case tgfx::PathVerb::Close:
        result += "Z";
        break;
      default:
        break;
    }
  }

  return result;
}

struct GlyphKey {
  const tgfx::Typeface* typeface = nullptr;
  tgfx::GlyphID glyphID = 0;

  bool operator==(const GlyphKey& other) const {
    return typeface == other.typeface && glyphID == other.glyphID;
  }
};

struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const {
    size_t h = std::hash<const void*>()(key.typeface);
    h ^= std::hash<uint16_t>()(key.glyphID) << 1;
    return h;
  }
};

struct VectorFontBuilder {
  Font* font = nullptr;
  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> glyphMapping = {};
};

struct BitmapFontBuilder {
  const tgfx::Typeface* typeface = nullptr;
  int backingSize = 0;
  Font* font = nullptr;
  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> glyphMapping = {};
};

static void CollectMaxFontSize(const tgfx::Font& font, tgfx::GlyphID glyphID,
                               std::unordered_map<GlyphKey, float, GlyphKeyHash>& maxSizes) {
  GlyphKey key = {font.getTypeface().get(), glyphID};
  float fontSize = font.getSize();
  auto it = maxSizes.find(key);
  if (it == maxSizes.end() || fontSize > it->second) {
    maxSizes[key] = fontSize;
  }
}

static void CollectVectorGlyph(PAGXDocument* document, const tgfx::Font& font,
                               tgfx::GlyphID glyphID,
                               const std::unordered_map<GlyphKey, float, GlyphKeyHash>& maxSizes,
                               VectorFontBuilder& builder) {
  GlyphKey key = {font.getTypeface().get(), glyphID};

  if (builder.glyphMapping.count(key) > 0) {
    return;
  }

  auto it = maxSizes.find(key);
  if (it == maxSizes.end()) {
    return;
  }

  float maxSize = it->second;
  if (std::abs(font.getSize() - maxSize) > 0.001f) {
    return;
  }

  tgfx::Path glyphPath = {};
  if (!font.getPath(glyphID, &glyphPath) || glyphPath.isEmpty()) {
    return;
  }

  float scale = static_cast<float>(VectorFontUnitsPerEm) / font.getSize();
  glyphPath.transform(tgfx::Matrix::MakeScale(scale, scale));

  if (builder.font == nullptr) {
    builder.font = document->makeNode<Font>();
    builder.font->unitsPerEm = VectorFontUnitsPerEm;
  }

  auto glyph = document->makeNode<Glyph>();
  glyph->path = document->makeNode<PathData>();
  *glyph->path = PathDataFromSVGString(PathToSVGString(glyphPath));

  // Get advance in font size space and scale to unitsPerEm space
  float advance = font.getAdvance(glyphID);
  glyph->advance = advance * scale;

  builder.font->glyphs.push_back(glyph);
  builder.glyphMapping[key] = static_cast<tgfx::GlyphID>(builder.font->glyphs.size());
}

static void CollectBitmapGlyph(
    PAGXDocument* document, const tgfx::Font& font, tgfx::GlyphID glyphID,
    std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder>& builders) {
  auto* typeface = font.getTypeface().get();
  auto& builder = builders[typeface];

  GlyphKey key = {typeface, glyphID};
  if (builder.glyphMapping.count(key) > 0) {
    return;
  }

  tgfx::Matrix imageMatrix = {};
  auto imageCodec = font.getImage(glyphID, nullptr, &imageMatrix);
  if (!imageCodec) {
    return;
  }

  if (builder.backingSize == 0) {
    float scaleX = std::abs(imageMatrix.getScaleX());
    if (scaleX < 0.001f) {
      return;
    }
    builder.backingSize = static_cast<int>(std::round(font.getSize() / scaleX));
    builder.typeface = typeface;
    builder.font = document->makeNode<Font>();
    builder.font->unitsPerEm = builder.backingSize;
  }

  int width = imageCodec->width();
  int height = imageCodec->height();
  if (width <= 0 || height <= 0) {
    return;
  }

  tgfx::Bitmap bitmap(width, height, false, false);
  if (bitmap.isEmpty()) {
    return;
  }
  auto* pixels = bitmap.lockPixels();
  if (pixels == nullptr) {
    return;
  }
  bool success = imageCodec->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  if (!success) {
    return;
  }

  auto pngData = bitmap.encode(tgfx::EncodedFormat::PNG, 100);
  if (pngData == nullptr) {
    return;
  }

  auto glyph = document->makeNode<Glyph>();
  auto image = document->makeNode<Image>();
  image->data = pagx::Data::MakeWithCopy(pngData->data(), pngData->size());
  glyph->image = image;

  float fontSize = font.getSize();
  float backingSize = static_cast<float>(builder.backingSize);
  glyph->offset.x = imageMatrix.getTranslateX() / fontSize * backingSize;
  glyph->offset.y = imageMatrix.getTranslateY() / fontSize * backingSize;

  // Get advance in font size space and scale to unitsPerEm (backingSize) space
  float advance = font.getAdvance(glyphID);
  glyph->advance = advance / fontSize * backingSize;

  builder.font->glyphs.push_back(glyph);
  builder.glyphMapping[key] = static_cast<tgfx::GlyphID>(builder.font->glyphs.size());
}

static bool IsVectorGlyph(const tgfx::Font& font, tgfx::GlyphID glyphID) {
  tgfx::Path glyphPath = {};
  return font.getPath(glyphID, &glyphPath) && !glyphPath.isEmpty();
}

static bool CanUseDefaultMode(const tgfx::GlyphRun& run, const std::vector<size_t>& indices,
                              Font* font,
                              const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& map,
                              float fontSize, float* outOffsetX, float* outOffsetY) {
  if (run.positioning != tgfx::GlyphPositioning::Horizontal &&
      run.positioning != tgfx::GlyphPositioning::Default) {
    return false;
  }
  if (run.positioning == tgfx::GlyphPositioning::Default) {
    // Default mode doesn't have explicit positions, so we can't extract startX from positions.
    // This case is already Default mode, just pass through.
    *outOffsetX = 0;
    *outOffsetY = run.offsetY;
    return true;
  }
  if (indices.empty()) {
    return false;
  }
  float scale = fontSize / static_cast<float>(font->unitsPerEm);
  // Use first glyph's x position as the starting point
  float startX = run.positions[indices[0]];
  float expectedX = startX;
  auto* typeface = run.font.getTypeface().get();
  for (size_t i : indices) {
    float actualX = run.positions[i];
    if (!FloatNearlyEqual(actualX, expectedX)) {
      return false;
    }
    GlyphKey key = {typeface, run.glyphs[i]};
    auto it = map.find(key);
    if (it == map.end() || it->second == 0) {
      return false;
    }
    tgfx::GlyphID mappedID = it->second;
    if (mappedID > font->glyphs.size()) {
      return false;
    }
    float advance = font->glyphs[mappedID - 1]->advance * scale;
    expectedX += advance;
  }
  *outOffsetX = startX;
  *outOffsetY = run.offsetY;
  return true;
}

static GlyphRun* CreateGlyphRunForIndices(
    PAGXDocument* document, const tgfx::GlyphRun& run, const std::vector<size_t>& indices,
    Font* font,
    const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& glyphMapping, float fontSize) {
  auto glyphRun = document->makeNode<GlyphRun>();
  glyphRun->font = font;
  glyphRun->fontSize = fontSize;

  auto* typeface = run.font.getTypeface().get();
  for (size_t i : indices) {
    GlyphKey key = {typeface, run.glyphs[i]};
    auto it = glyphMapping.find(key);
    if (it != glyphMapping.end()) {
      glyphRun->glyphs.push_back(it->second);
    } else {
      glyphRun->glyphs.push_back(0);
    }
  }

  // Try to use Default mode if positions match advance-based layout
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  if (CanUseDefaultMode(run, indices, font, glyphMapping, fontSize, &offsetX, &offsetY)) {
    glyphRun->x = offsetX;
    glyphRun->y = offsetY;
    return glyphRun;
  }

  float advanceScale = fontSize / static_cast<float>(font->unitsPerEm);

  switch (run.positioning) {
    case tgfx::GlyphPositioning::Horizontal: {
      glyphRun->y = run.offsetY;
      for (size_t i : indices) {
        glyphRun->xOffsets.push_back(run.positions[i]);
      }
      break;
    }
    case tgfx::GlyphPositioning::Point: {
      auto* points = reinterpret_cast<const tgfx::Point*>(run.positions);
      for (size_t i : indices) {
        glyphRun->positions.push_back({points[i].x, points[i].y});
      }
      break;
    }
    case tgfx::GlyphPositioning::RSXform: {
      // Decompose RSXform into position, scale, and rotation
      auto* xforms = reinterpret_cast<const tgfx::RSXform*>(run.positions);
      bool hasNonDefaultScale = false;
      bool hasNonDefaultRotation = false;

      // First pass: check if we have non-default values
      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        const auto& xform = xforms[i];
        float scale = std::sqrt(xform.scos * xform.scos + xform.ssin * xform.ssin);
        float rotation = RadiansToDegrees(std::atan2(xform.ssin, xform.scos));
        if (!FloatNearlyEqual(scale, 1.0f)) {
          hasNonDefaultScale = true;
        }
        if (!FloatNearlyEqual(rotation, 0.0f)) {
          hasNonDefaultRotation = true;
        }
      }

      // Second pass: extract values
      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        const auto& xform = xforms[i];

        // Get glyph advance for anchor calculation
        float glyphAdvance = 0.0f;
        if (glyphRun->glyphs[idx] > 0 && glyphRun->glyphs[idx] <= font->glyphs.size()) {
          glyphAdvance = font->glyphs[glyphRun->glyphs[idx] - 1]->advance * advanceScale;
        }
        float defaultAnchorX = glyphAdvance * 0.5f;

        // Decompose: scale = sqrt(scos^2 + ssin^2), rotation = atan2(ssin, scos)
        float scale = std::sqrt(xform.scos * xform.scos + xform.ssin * xform.ssin);
        float rotation = RadiansToDegrees(std::atan2(xform.ssin, xform.scos));

        // The RSXform position (tx, ty) is the position after the anchor transform
        // We need to reverse-calculate the original position
        // Original transform: translate(-anchor) -> scale -> rotate -> translate(anchor) -> translate(pos)
        // RSXform encodes: scale*rotate matrix + final position
        // So tx, ty = anchor + pos - rotatedScaledAnchor
        // pos = tx - anchor + rotatedScaledAnchor - anchor... this is complex
        // For simplicity, just store position as tx, ty (the final transformed position)
        glyphRun->positions.push_back({xform.tx, xform.ty});

        if (hasNonDefaultScale) {
          glyphRun->scales.push_back({scale, scale});
        }
        if (hasNonDefaultRotation) {
          glyphRun->rotations.push_back(rotation);
        }
      }
      break;
    }
    case tgfx::GlyphPositioning::Matrix: {
      // Decompose full matrix into position, scale, rotation, skew
      auto* matrices = reinterpret_cast<const float*>(run.positions);
      bool hasNonDefaultScale = false;
      bool hasNonDefaultRotation = false;
      bool hasNonDefaultSkew = false;

      // First pass: check if we have non-default values
      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        float a = matrices[i * 6 + 0];
        float b = matrices[i * 6 + 1];
        float c = matrices[i * 6 + 2];
        float d = matrices[i * 6 + 3];

        // Decompose matrix: M = T * R * S * Skew
        // For a uniform scale + rotation matrix: a = s*cos, b = s*sin, c = -s*sin, d = s*cos
        float scaleX = std::sqrt(a * a + b * b);
        float scaleY = std::sqrt(c * c + d * d);
        float rotation = RadiansToDegrees(std::atan2(b, a));

        // Check for skew (non-orthogonal matrix)
        float dotProduct = a * c + b * d;
        float skew = 0.0f;
        if (scaleX > 0.001f && scaleY > 0.001f) {
          skew = RadiansToDegrees(std::atan2(dotProduct, scaleX * scaleY));
        }

        if (!FloatNearlyEqual(scaleX, 1.0f) || !FloatNearlyEqual(scaleY, 1.0f)) {
          hasNonDefaultScale = true;
        }
        if (!FloatNearlyEqual(rotation, 0.0f)) {
          hasNonDefaultRotation = true;
        }
        if (!FloatNearlyEqual(skew, 0.0f)) {
          hasNonDefaultSkew = true;
        }
      }

      // Second pass: extract values
      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        float a = matrices[i * 6 + 0];
        float b = matrices[i * 6 + 1];
        float c = matrices[i * 6 + 2];
        float d = matrices[i * 6 + 3];
        float tx = matrices[i * 6 + 4];
        float ty = matrices[i * 6 + 5];

        // Store position
        glyphRun->positions.push_back({tx, ty});

        // Decompose matrix
        float scaleX = std::sqrt(a * a + b * b);
        float scaleY = std::sqrt(c * c + d * d);
        float rotation = RadiansToDegrees(std::atan2(b, a));

        float dotProduct = a * c + b * d;
        float skew = 0.0f;
        if (scaleX > 0.001f && scaleY > 0.001f) {
          skew = RadiansToDegrees(std::atan2(dotProduct, scaleX * scaleY));
        }

        if (hasNonDefaultScale) {
          glyphRun->scales.push_back({scaleX, scaleY});
        }
        if (hasNonDefaultRotation) {
          glyphRun->rotations.push_back(rotation);
        }
        if (hasNonDefaultSkew) {
          glyphRun->skews.push_back(skew);
        }
      }
      break;
    }
    default:
      break;
  }

  return glyphRun;
}

bool FontEmbedder::embed(PAGXDocument* document, const ShapedTextMap& shapedTextMap) {
  if (document == nullptr) {
    return false;
  }

  std::unordered_map<GlyphKey, float, GlyphKeyHash> maxFontSizes = {};
  VectorFontBuilder vectorBuilder = {};
  std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder> bitmapBuilders = {};

  // First pass: collect max font size for each vector glyph
  for (const auto& [text, shapedText] : shapedTextMap) {
    if (shapedText.textBlob == nullptr) {
      continue;
    }
    for (const auto& run : *shapedText.textBlob) {
      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        if (IsVectorGlyph(run.font, glyphID)) {
          CollectMaxFontSize(run.font, glyphID, maxFontSizes);
        }
      }
    }
  }

  // Second pass: collect glyphs using max font size for vector, first encounter for bitmap
  for (const auto& [text, shapedText] : shapedTextMap) {
    if (shapedText.textBlob == nullptr) {
      continue;
    }
    for (const auto& run : *shapedText.textBlob) {
      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        if (IsVectorGlyph(run.font, glyphID)) {
          CollectVectorGlyph(document, run.font, glyphID, maxFontSizes, vectorBuilder);
        } else {
          CollectBitmapGlyph(document, run.font, glyphID, bitmapBuilders);
        }
      }
    }
  }

  // Assign sequential IDs to all fonts
  int fontIndex = 1;
  if (vectorBuilder.font != nullptr) {
    vectorBuilder.font->id = "font" + std::to_string(fontIndex++);
  }
  for (auto& [typeface, builder] : bitmapBuilders) {
    if (builder.font != nullptr) {
      builder.font->id = "font" + std::to_string(fontIndex++);
    }
  }

  // Third pass: create GlyphRuns for each Text
  for (const auto& [text, shapedText] : shapedTextMap) {
    if (shapedText.textBlob == nullptr) {
      continue;
    }

    text->glyphRuns.clear();

    for (const auto& run : *shapedText.textBlob) {
      if (run.glyphCount == 0) {
        continue;
      }

      auto* typeface = run.font.getTypeface().get();
      float fontSize = run.font.getSize();

      std::vector<size_t> vectorIndices = {};
      std::vector<size_t> bitmapIndices = {};

      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        GlyphKey key = {typeface, glyphID};

        if (vectorBuilder.glyphMapping.count(key) > 0) {
          vectorIndices.push_back(i);
        } else {
          auto builderIt = bitmapBuilders.find(typeface);
          if (builderIt != bitmapBuilders.end() && builderIt->second.glyphMapping.count(key) > 0) {
            bitmapIndices.push_back(i);
          }
        }
      }

      if (!vectorIndices.empty() && vectorBuilder.font != nullptr) {
        auto glyphRun = CreateGlyphRunForIndices(document, run, vectorIndices, vectorBuilder.font,
                                                 vectorBuilder.glyphMapping, fontSize);
        if (glyphRun != nullptr) {
          text->glyphRuns.push_back(glyphRun);
        }
      }

      if (!bitmapIndices.empty()) {
        auto builderIt = bitmapBuilders.find(typeface);
        if (builderIt != bitmapBuilders.end() && builderIt->second.font != nullptr) {
          auto glyphRun =
              CreateGlyphRunForIndices(document, run, bitmapIndices, builderIt->second.font,
                                       builderIt->second.glyphMapping, fontSize);
          if (glyphRun != nullptr) {
            text->glyphRuns.push_back(glyphRun);
          }
        }
      }
    }
  }

  return true;
}

}  // namespace pagx
