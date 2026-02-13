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

#include "FontEmbedder.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/GlyphRun.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"

namespace pagx {

static constexpr int VectorFontUnitsPerEm = 1000;

static void PathToPathData(const tgfx::Path& path, PathData* pathData) {
  for (const auto& segment : path) {
    switch (segment.verb) {
      case tgfx::PathVerb::Move:
        pathData->moveTo(std::round(segment.points[0].x), std::round(segment.points[0].y));
        break;
      case tgfx::PathVerb::Line:
        pathData->lineTo(std::round(segment.points[1].x), std::round(segment.points[1].y));
        break;
      case tgfx::PathVerb::Quad:
        pathData->quadTo(std::round(segment.points[1].x), std::round(segment.points[1].y),
                         std::round(segment.points[2].x), std::round(segment.points[2].y));
        break;
      case tgfx::PathVerb::Cubic:
        pathData->cubicTo(std::round(segment.points[1].x), std::round(segment.points[1].y),
                          std::round(segment.points[2].x), std::round(segment.points[2].y),
                          std::round(segment.points[3].x), std::round(segment.points[3].y));
        break;
      case tgfx::PathVerb::Close:
        pathData->close();
        break;
      default:
        break;
    }
  }
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

static void CollectVectorGlyph(PAGXDocument* document, const tgfx::Font& font,
                               tgfx::GlyphID glyphID,
                               std::unordered_map<GlyphKey, float, GlyphKeyHash>& maxSizes,
                               VectorFontBuilder& builder) {
  GlyphKey key = {font.getTypeface().get(), glyphID};
  float fontSize = font.getSize();

  // Track the maximum font size for each vector glyph, and only collect the path at the largest
  // size to get the best quality when scaling to unitsPerEm space.
  auto [sizeIt, inserted] = maxSizes.emplace(key, fontSize);
  if (!inserted) {
    if (fontSize <= sizeIt->second) {
      return;
    }
    // Found a larger font size. Update the max and re-collect the glyph below.
    sizeIt->second = fontSize;
  }

  tgfx::Path glyphPath = {};
  if (!font.getPath(glyphID, &glyphPath) || glyphPath.isEmpty()) {
    return;
  }
  if (fontSize < 0.001f) {
    return;
  }
  float scale = static_cast<float>(VectorFontUnitsPerEm) / fontSize;
  glyphPath.transform(tgfx::Matrix::MakeScale(scale, scale));

  if (builder.font == nullptr) {
    builder.font = document->makeNode<Font>();
    builder.font->unitsPerEm = VectorFontUnitsPerEm;
  }

  auto mappingIt = builder.glyphMapping.find(key);
  if (mappingIt != builder.glyphMapping.end()) {
    // Re-collect at a larger font size: replace the existing glyph's path data.
    auto existingGlyph = builder.font->glyphs[mappingIt->second - 1];
    existingGlyph->path = document->makeNode<PathData>();
    PathToPathData(glyphPath, existingGlyph->path);
    existingGlyph->advance = font.getAdvance(glyphID) * scale;
    return;
  }

  auto glyph = document->makeNode<Glyph>();
  glyph->path = document->makeNode<PathData>();
  PathToPathData(glyphPath, glyph->path);
  glyph->advance = font.getAdvance(glyphID) * scale;

  builder.font->glyphs.push_back(glyph);
  builder.glyphMapping[key] = static_cast<tgfx::GlyphID>(builder.font->glyphs.size());
}

static void CollectBitmapGlyph(
    PAGXDocument* document, const tgfx::Font& font, tgfx::GlyphID glyphID,
    std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder>& builders,
    std::vector<const tgfx::Typeface*>* typefaceOrder) {
  auto* typeface = font.getTypeface().get();
  auto& builder = builders[typeface];

  GlyphKey key = {typeface, glyphID};
  if (builder.glyphMapping.count(key)) {
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
    if (typefaceOrder != nullptr) {
      typefaceOrder->push_back(typeface);
    }
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

  float fontSize = font.getSize();
  if (fontSize <= 0) {
    return;
  }

  auto glyph = document->makeNode<Glyph>();
  auto image = document->makeNode<Image>();
  image->data = pagx::Data::MakeWithCopy(pngData->data(), pngData->size());
  glyph->image = image;

  float backingSize = static_cast<float>(builder.backingSize);
  glyph->offset.x = imageMatrix.getTranslateX() / fontSize * backingSize;
  glyph->offset.y = imageMatrix.getTranslateY() / fontSize * backingSize;

  // Get advance in font size space and scale to unitsPerEm (backingSize) space
  float advance = font.getAdvance(glyphID);
  glyph->advance = advance / fontSize * backingSize;

  builder.font->glyphs.push_back(glyph);
  builder.glyphMapping[key] = static_cast<tgfx::GlyphID>(builder.font->glyphs.size());
}

enum class GlyphType {
  Unknown,
  Vector,
  Bitmap,
  Spacing,
};

static GlyphType ClassifyGlyph(const tgfx::Font& font, tgfx::GlyphID glyphID) {
  tgfx::Path glyphPath = {};
  if (font.getPath(glyphID, &glyphPath) && !glyphPath.isEmpty()) {
    return GlyphType::Vector;
  }
  tgfx::Matrix imageMatrix = {};
  if (font.getImage(glyphID, nullptr, &imageMatrix) != nullptr) {
    return GlyphType::Bitmap;
  }
  return GlyphType::Spacing;
}

static const ShapedText* FindShapedText(const ShapedTextMap& shapedTextMap, const Text* text) {
  if (text == nullptr) {
    return nullptr;
  }
  auto it = shapedTextMap.find(text);
  if (it == shapedTextMap.end()) {
    return nullptr;
  }
  return &it->second;
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
  if (font->unitsPerEm <= 0) {
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
  glyphRun->glyphs.reserve(indices.size());
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

  switch (run.positioning) {
    case tgfx::GlyphPositioning::Horizontal: {
      glyphRun->y = run.offsetY;
      glyphRun->xOffsets.reserve(indices.size());
      for (size_t i : indices) {
        glyphRun->xOffsets.push_back(run.positions[i]);
      }
      break;
    }
    case tgfx::GlyphPositioning::Point: {
      auto* points = reinterpret_cast<const tgfx::Point*>(run.positions);
      glyphRun->positions.reserve(indices.size());
      for (size_t i : indices) {
        glyphRun->positions.push_back({points[i].x, points[i].y});
      }
      break;
    }
    case tgfx::GlyphPositioning::RSXform: {
      // Decompose RSXform into position, scale, and rotation
      auto* xforms = reinterpret_cast<const tgfx::RSXform*>(run.positions);
      glyphRun->positions.reserve(indices.size());
      glyphRun->scales.reserve(indices.size());
      glyphRun->rotations.reserve(indices.size());
      bool hasNonDefaultScale = false;
      bool hasNonDefaultRotation = false;

      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        const auto& xform = xforms[i];
        float scale = std::hypot(xform.scos, xform.ssin);
        float rotation = RadiansToDegrees(std::atan2(xform.ssin, xform.scos));
        glyphRun->positions.push_back({xform.tx, xform.ty});
        glyphRun->scales.push_back({scale, scale});
        glyphRun->rotations.push_back(rotation);
        if (!FloatNearlyEqual(scale, 1.0f)) {
          hasNonDefaultScale = true;
        }
        if (!FloatNearlyEqual(rotation, 0.0f)) {
          hasNonDefaultRotation = true;
        }
      }
      if (!hasNonDefaultScale) {
        glyphRun->scales.clear();
      }
      if (!hasNonDefaultRotation) {
        glyphRun->rotations.clear();
      }
      break;
    }
    case tgfx::GlyphPositioning::Matrix: {
      // Decompose full matrix into position, scale, rotation, skew
      auto* matrices = reinterpret_cast<const float*>(run.positions);
      glyphRun->positions.reserve(indices.size());
      glyphRun->scales.reserve(indices.size());
      glyphRun->rotations.reserve(indices.size());
      glyphRun->skews.reserve(indices.size());
      bool hasNonDefaultScale = false;
      bool hasNonDefaultRotation = false;
      bool hasNonDefaultSkew = false;

      for (size_t idx = 0; idx < indices.size(); idx++) {
        size_t i = indices[idx];
        float a = matrices[i * 6 + 0];
        float b = matrices[i * 6 + 1];
        float c = matrices[i * 6 + 2];
        float d = matrices[i * 6 + 3];
        float tx = matrices[i * 6 + 4];
        float ty = matrices[i * 6 + 5];

        float scaleX = std::sqrt(a * a + b * b);
        float scaleY = std::sqrt(c * c + d * d);
        float rotation = RadiansToDegrees(std::atan2(b, a));

        float dotProduct = a * c + b * d;
        float skew = 0.0f;
        if (scaleX > 0.001f && scaleY > 0.001f) {
          skew = RadiansToDegrees(std::atan2(dotProduct, scaleX * scaleY));
        }

        glyphRun->positions.push_back({tx, ty});
        glyphRun->scales.push_back({scaleX, scaleY});
        glyphRun->rotations.push_back(rotation);
        glyphRun->skews.push_back(skew);
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
      if (!hasNonDefaultScale) {
        glyphRun->scales.clear();
      }
      if (!hasNonDefaultRotation) {
        glyphRun->rotations.clear();
      }
      if (!hasNonDefaultSkew) {
        glyphRun->skews.clear();
      }
      break;
    }
    default:
      break;
  }

  return glyphRun;
}

static void CollectSpacingGlyph(PAGXDocument* document, const tgfx::Font& font,
                                tgfx::GlyphID glyphID,
                                std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder>&
                                    bitmapBuilders,
                                VectorFontBuilder& vectorBuilder) {
  float advance = font.getAdvance(glyphID);
  if (advance <= 0) {
    return;
  }
  auto* typeface = font.getTypeface().get();
  GlyphKey key = {typeface, glyphID};
  float runFontSize = font.getSize();
  if (runFontSize < 0.001f) {
    return;
  }
  auto bitmapIt = bitmapBuilders.find(typeface);
  if (bitmapIt != bitmapBuilders.end() && bitmapIt->second.font != nullptr) {
    auto& builder = bitmapIt->second;
    if (builder.glyphMapping.count(key)) {
      return;
    }
    auto glyph = document->makeNode<Glyph>();
    float backingSize = static_cast<float>(builder.backingSize);
    glyph->advance = advance / runFontSize * backingSize;
    builder.font->glyphs.push_back(glyph);
    builder.glyphMapping[key] = static_cast<tgfx::GlyphID>(builder.font->glyphs.size());
  } else if (vectorBuilder.font != nullptr) {
    if (vectorBuilder.glyphMapping.count(key) > 0) {
      return;
    }
    float scale = static_cast<float>(VectorFontUnitsPerEm) / runFontSize;
    auto glyph = document->makeNode<Glyph>();
    glyph->advance = advance * scale;
    vectorBuilder.font->glyphs.push_back(glyph);
    vectorBuilder.glyphMapping[key] = static_cast<tgfx::GlyphID>(vectorBuilder.font->glyphs.size());
  }
}

bool FontEmbedder::embed(PAGXDocument* document, const ShapedTextMap& shapedTextMap,
                          const std::vector<Text*>& textOrder) {
  if (document == nullptr) {
    return false;
  }

  std::unordered_map<GlyphKey, GlyphType, GlyphKeyHash> glyphTypes = {};
  std::unordered_map<GlyphKey, float, GlyphKeyHash> maxFontSizes = {};
  VectorFontBuilder vectorBuilder = {};
  std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder> bitmapBuilders = {};
  std::vector<const tgfx::Typeface*> bitmapTypefaces = {};

  // First pass: classify all glyphs and collect vector/bitmap/spacing glyph data in one iteration.
  // For vector glyphs, CollectVectorGlyph internally tracks the maximum font size and re-collects
  // the path when a larger size is found, so no separate max-size pass is needed.
  for (auto* text : textOrder) {
    auto shapedText = FindShapedText(shapedTextMap, text);
    if (shapedText == nullptr || shapedText->textBlob == nullptr) {
      continue;
    }
    for (const auto& run : *shapedText->textBlob) {
      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        GlyphKey key = {run.font.getTypeface().get(), glyphID};
        auto& type = glyphTypes[key];
        if (type == GlyphType::Unknown) {
          type = ClassifyGlyph(run.font, glyphID);
        }
        switch (type) {
          case GlyphType::Vector:
            CollectVectorGlyph(document, run.font, glyphID, maxFontSizes, vectorBuilder);
            break;
          case GlyphType::Bitmap:
            CollectBitmapGlyph(document, run.font, glyphID, bitmapBuilders, &bitmapTypefaces);
            break;
          case GlyphType::Spacing:
            CollectSpacingGlyph(document, run.font, glyphID, bitmapBuilders, vectorBuilder);
            break;
          default:
            break;
        }
      }
    }
  }

  // Assign sequential IDs to all fonts
  int fontIndex = 1;
  if (vectorBuilder.font != nullptr) {
    vectorBuilder.font->id = "font" + std::to_string(fontIndex++);
  }
  for (auto* typeface : bitmapTypefaces) {
    if (typeface == nullptr) {
      continue;
    }
    auto builderIt = bitmapBuilders.find(typeface);
    if (builderIt != bitmapBuilders.end() && builderIt->second.font != nullptr) {
      builderIt->second.font->id = "font" + std::to_string(fontIndex++);
    }
  }

  // Second pass: create GlyphRuns for each Text
  for (auto* text : textOrder) {
    auto shapedText = FindShapedText(shapedTextMap, text);
    if (shapedText == nullptr || shapedText->textBlob == nullptr) {
      continue;
    }

    text->glyphRuns.clear();

    for (const auto& run : *shapedText->textBlob) {
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
