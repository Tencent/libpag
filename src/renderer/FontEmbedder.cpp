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
#include "base/utils/Log.h"
#include "base/utils/MathUtil.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Bitmap.h"
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

static void CollectTextFromElements(const std::vector<Element*>& elements,
                                    std::vector<Text*>& outText) {
  for (auto* element : elements) {
    if (element == nullptr) {
      continue;
    }
    switch (element->nodeType()) {
      case NodeType::Text:
        outText.push_back(static_cast<Text*>(element));
        break;
      case NodeType::Group:
        CollectTextFromElements(static_cast<Group*>(element)->elements, outText);
        break;
      case NodeType::TextBox:
        CollectTextFromElements(static_cast<TextBox*>(element)->elements, outText);
        break;
      default:
        break;
    }
  }
}

static void CollectTextFromLayer(Layer* layer, std::vector<Text*>& outText) {
  if (layer == nullptr) {
    return;
  }
  CollectTextFromElements(layer->contents, outText);
  for (auto* child : layer->children) {
    CollectTextFromLayer(child, outText);
  }
}

static std::vector<Text*> CollectAllText(PAGXDocument* document) {
  std::vector<Text*> allText = {};
  for (auto* layer : document->layers) {
    CollectTextFromLayer(layer, allText);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CollectTextFromLayer(layer, allText);
      }
    }
  }
  return allText;
}

static GlyphRun* CreateGlyphRunFromLayoutRun(
    PAGXDocument* document, const TextLayoutGlyphRun& tlRun, const tgfx::Typeface* typeface,
    const std::vector<size_t>& indices, Font* embeddedFont, float fontSize,
    const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& glyphMapping) {
  auto glyphRun = document->makeNode<GlyphRun>();
  glyphRun->font = embeddedFont;
  glyphRun->fontSize = fontSize;
  glyphRun->glyphs.reserve(indices.size());
  for (auto idx : indices) {
    GlyphKey key = {typeface, tlRun.glyphs[idx]};
    auto it = glyphMapping.find(key);
    glyphRun->glyphs.push_back(it != glyphMapping.end() ? it->second : 0);
  }
  bool hasTransforms = !tlRun.xforms.empty();
  glyphRun->positions.reserve(indices.size());
  for (auto idx : indices) {
    if (idx < tlRun.positions.size()) {
      glyphRun->positions.push_back({tlRun.positions[idx].x, tlRun.positions[idx].y});
    }
  }
  if (hasTransforms) {
    glyphRun->scales.reserve(indices.size());
    glyphRun->rotations.reserve(indices.size());
    for (auto idx : indices) {
      if (idx < tlRun.xforms.size()) {
        auto& xf = tlRun.xforms[idx];
        float scale = std::hypot(xf.scos, xf.ssin);
        float rotation = std::atan2(xf.ssin, xf.scos) * 180.0f / static_cast<float>(M_PI);
        glyphRun->scales.push_back({scale, scale});
        glyphRun->rotations.push_back(rotation);
      }
    }
    bool hasNonDefaultScale = false;
    bool hasNonDefaultRotation = false;
    for (size_t i = 0; i < glyphRun->scales.size(); i++) {
      if (!pag::FloatNearlyEqual(glyphRun->scales[i].x, 1.0f) ||
          !pag::FloatNearlyEqual(glyphRun->scales[i].y, 1.0f)) {
        hasNonDefaultScale = true;
      }
      if (!pag::FloatNearlyZero(glyphRun->rotations[i])) {
        hasNonDefaultRotation = true;
      }
    }
    if (!hasNonDefaultScale) {
      glyphRun->scales.clear();
    }
    if (!hasNonDefaultRotation) {
      glyphRun->rotations.clear();
    }
  }
  return glyphRun;
}

static void CollectSpacingGlyph(
    PAGXDocument* document, const tgfx::Font& font, tgfx::GlyphID glyphID,
    std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder>& bitmapBuilders,
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

void FontEmbedder::ClearEmbeddedGlyphRuns(PAGXDocument* document) {
  if (document == nullptr) {
    return;
  }
  auto textOrder = CollectAllText(document);
  for (auto* text : textOrder) {
    text->glyphRuns.clear();
  }
}

bool FontEmbedder::embed(PAGXDocument* document) {
  if (document == nullptr) {
    return false;
  }
  if (!document->isLayoutApplied()) {
    LOGE("FontEmbedder::embed() called before applyLayout(). Call document->applyLayout() first.");
    DEBUG_ASSERT(false);
    return false;
  }
  auto textOrder = CollectAllText(document);

  std::unordered_map<GlyphKey, GlyphType, GlyphKeyHash> glyphTypes = {};
  std::unordered_map<GlyphKey, float, GlyphKeyHash> maxFontSizes = {};
  VectorFontBuilder vectorBuilder = {};
  std::unordered_map<const tgfx::Typeface*, BitmapFontBuilder> bitmapBuilders = {};
  std::vector<const tgfx::Typeface*> bitmapTypefaces = {};

  // First pass: classify all glyphs and collect vector/bitmap/spacing glyph data.
  // Uses TextLayoutGlyphRun populated by applyLayout(). Text nodes without layoutRuns are skipped.
  for (auto* text : textOrder) {
    auto& layoutRuns = text->glyphData->layoutRuns;
    if (!layoutRuns.empty()) {
      for (auto& tlRun : layoutRuns) {
        auto* typeface = tlRun.font.getTypeface().get();
        if (typeface == nullptr) {
          continue;
        }
        for (auto glyphID : tlRun.glyphs) {
          GlyphKey key = {typeface, glyphID};
          auto& type = glyphTypes[key];
          if (type == GlyphType::Unknown) {
            type = ClassifyGlyph(tlRun.font, glyphID);
          }
          switch (type) {
            case GlyphType::Vector:
              CollectVectorGlyph(document, tlRun.font, glyphID, maxFontSizes, vectorBuilder);
              break;
            case GlyphType::Bitmap:
              CollectBitmapGlyph(document, tlRun.font, glyphID, bitmapBuilders, &bitmapTypefaces);
              break;
            case GlyphType::Spacing:
              CollectSpacingGlyph(document, tlRun.font, glyphID, bitmapBuilders, vectorBuilder);
              break;
            default:
              break;
          }
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

  // Second pass: create pagx::GlyphRuns for each Text.
  // Uses TextLayoutGlyphRun (direct mapping). Text nodes without layoutRuns produce no GlyphRuns.
  for (auto* text : textOrder) {
    text->glyphRuns.clear();
    auto textBounds = text->textBounds;
    auto& layoutRuns = text->glyphData->layoutRuns;

    if (!layoutRuns.empty()) {
      // New path: create GlyphRuns directly from TextLayoutGlyphRun.
      for (auto& tlRun : layoutRuns) {
        if (tlRun.glyphs.empty()) {
          continue;
        }
        auto* typeface = tlRun.font.getTypeface().get();
        if (typeface == nullptr) {
          continue;
        }
        float fontSize = tlRun.font.getSize();

        // Split glyphs by font type (vector vs bitmap).
        std::vector<size_t> vectorIndices;
        std::vector<size_t> bitmapIndices;
        for (size_t i = 0; i < tlRun.glyphs.size(); i++) {
          GlyphKey key = {typeface, tlRun.glyphs[i]};
          if (vectorBuilder.glyphMapping.count(key) > 0) {
            vectorIndices.push_back(i);
          } else {
            auto builderIt = bitmapBuilders.find(typeface);
            if (builderIt != bitmapBuilders.end() &&
                builderIt->second.glyphMapping.count(key) > 0) {
              bitmapIndices.push_back(i);
            }
          }
        }

        if (!vectorIndices.empty() && vectorBuilder.font != nullptr) {
          auto glyphRun =
              CreateGlyphRunFromLayoutRun(document, tlRun, typeface, vectorIndices,
                                          vectorBuilder.font, fontSize, vectorBuilder.glyphMapping);
          text->glyphRuns.push_back(glyphRun);
        }
        if (!bitmapIndices.empty()) {
          auto builderIt = bitmapBuilders.find(typeface);
          if (builderIt != bitmapBuilders.end() && builderIt->second.font != nullptr) {
            auto glyphRun = CreateGlyphRunFromLayoutRun(document, tlRun, typeface, bitmapIndices,
                                                        builderIt->second.font, fontSize,
                                                        builderIt->second.glyphMapping);
            text->glyphRuns.push_back(glyphRun);
          }
        }
      }
    }

    if (!text->glyphRuns.empty() && (textBounds.width > 0 || textBounds.height > 0)) {
      text->glyphRuns.front()->bounds = textBounds;
    }
  }

  return true;
}

}  // namespace pagx
