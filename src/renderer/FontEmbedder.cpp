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
#include <unordered_set>
#include "ToTGFX.h"
#include "base/utils/Log.h"
#include "base/utils/MathUtil.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"

namespace pagx {

static constexpr int VectorFontUnitsPerEm = 1000;
static constexpr float MinFontSize = 0.001f;

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
  bool fauxBold = false;
  bool fauxItalic = false;

  GlyphKey(const tgfx::Font& font, tgfx::GlyphID glyphID)
      : typeface(font.getTypeface().get()), glyphID(glyphID), fauxBold(font.isFauxBold()),
        fauxItalic(font.isFauxItalic()) {
  }

  bool operator==(const GlyphKey& other) const {
    return typeface == other.typeface && glyphID == other.glyphID && fauxBold == other.fauxBold &&
           fauxItalic == other.fauxItalic;
  }
};

struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const {
    size_t h = std::hash<const void*>()(key.typeface);
    h ^= std::hash<uint16_t>()(key.glyphID) << 1;
    h ^= static_cast<size_t>(key.fauxBold) << 17;
    h ^= static_cast<size_t>(key.fauxItalic) << 18;
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

struct BitmapKey {
  const tgfx::Typeface* typeface;
  float size;
  tgfx::Color foreground;

  BitmapKey(const tgfx::Font& font, const tgfx::Color& color)
      : typeface(font.getTypeface().get()), size(std::ceil(font.getSize())), foreground(color) {
  }

  bool operator==(const BitmapKey& other) const {
    return typeface == other.typeface && size == other.size && foreground == other.foreground;
  }
};

struct BitmapKeyHash {
  size_t operator()(const BitmapKey& key) const {
    size_t h = std::hash<const void*>()(key.typeface);
    for (auto value : {key.size, key.foreground.red, key.foreground.green, key.foreground.blue}) {
      h ^= std::hash<float>()(value) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
  }
};

using BitmapBuilders = std::unordered_map<BitmapKey, BitmapFontBuilder, BitmapKeyHash>;

static void CollectVectorGlyph(PAGXDocument* document, const tgfx::Font& font,
                               tgfx::GlyphID glyphID,
                               std::unordered_map<GlyphKey, float, GlyphKeyHash>& maxSizes,
                               VectorFontBuilder& builder) {
  GlyphKey key = {font, glyphID};
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
  if (fontSize < MinFontSize) {
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

static bool CollectBitmapGlyph(PAGXDocument* document, const tgfx::Font& sourceFont,
                               tgfx::GlyphID glyphID, const tgfx::Color& foreground,
                               BitmapBuilders& builders, std::vector<BitmapKey>& order) {
  BitmapKey bitmapKey(sourceFont, foreground);
  auto font = sourceFont.makeWithSize(bitmapKey.size);
  auto* typeface = font.getTypeface().get();
  auto& builder = builders[bitmapKey];

  GlyphKey key = {font, glyphID};
  if (builder.glyphMapping.count(key)) {
    return true;
  }

  tgfx::Matrix imageMatrix = {};
  auto imageCodec = font.getImage(glyphID, nullptr, &imageMatrix, {foreground, 0});
  if (!imageCodec) {
    return false;
  }

  if (builder.backingSize == 0) {
    float scaleX = std::abs(imageMatrix.getScaleX());
    if (scaleX < MinFontSize) {
      return false;
    }
    builder.backingSize = static_cast<int>(std::round(font.getSize() / scaleX));
    builder.typeface = typeface;
    builder.font = document->makeNode<Font>();
    builder.font->unitsPerEm = builder.backingSize;
    order.push_back(bitmapKey);
  }

  int width = imageCodec->width();
  int height = imageCodec->height();
  if (width <= 0 || height <= 0) {
    return false;
  }

  tgfx::Bitmap bitmap(width, height, false, false);
  if (bitmap.isEmpty()) {
    return false;
  }
  auto* pixels = bitmap.lockPixels();
  if (pixels == nullptr) {
    return false;
  }
  bool success = imageCodec->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  if (!success) {
    return false;
  }

  auto pngData = bitmap.encode(tgfx::EncodedFormat::PNG, 100);
  if (pngData == nullptr) {
    return false;
  }

  float fontSize = font.getSize();
  if (fontSize <= 0) {
    return false;
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
  return true;
}

enum class GlyphType {
  Unknown,
  Vector,
  Bitmap,
  Spacing,
  Unsupported,
};

static GlyphType ClassifyGlyph(const tgfx::Font& font, tgfx::GlyphID glyphID) {
  switch (font.getGlyphRepresentation(glyphID).content) {
    case tgfx::GlyphContent::Outline:
      return GlyphType::Vector;
    case tgfx::GlyphContent::Color:
    case tgfx::GlyphContent::Bitmap:
      return GlyphType::Bitmap;
    case tgfx::GlyphContent::Empty:
      return GlyphType::Spacing;
    default:
      return GlyphType::Unsupported;
  }
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
    PAGXDocument* document, const TextLayoutGlyphRun& tlRun, const tgfx::Font& collectFont,
    const std::vector<size_t>& indices, Font* embeddedFont, float fontSize,
    const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& glyphMapping) {
  auto glyphRun = document->makeNode<GlyphRun>();
  glyphRun->font = embeddedFont;
  glyphRun->fontSize = fontSize;
  glyphRun->glyphs.reserve(indices.size());
  for (auto idx : indices) {
    GlyphKey key = {collectFont, tlRun.glyphs[idx]};
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
    for (size_t i = 0; i < indices.size(); i++) {
      auto idx = indices[i];
      if (idx < tlRun.xforms.size()) {
        auto& xf = tlRun.xforms[idx];
        float scale = std::hypot(xf.scos, xf.ssin);
        float rotation = std::atan2(xf.ssin, xf.scos) * 180.0f / static_cast<float>(M_PI);
        glyphRun->scales.push_back({scale, scale});
        glyphRun->rotations.push_back(rotation);
        // Layout RSXforms already contain the final translation t. GlyphRun rotates around
        // half the embedded glyph advance, so serialize p = t + (L - I) * anchor.
        auto glyphID = glyphRun->glyphs[i];
        float anchor = glyphID > 0 && glyphID <= embeddedFont->glyphs.size()
                           ? embeddedFont->glyphs[glyphID - 1]->advance * fontSize /
                                 embeddedFont->unitsPerEm * 0.5f
                           : 0;
        if (i < glyphRun->positions.size()) {
          glyphRun->positions[i] = {xf.tx + (xf.scos - 1) * anchor, xf.ty + xf.ssin * anchor};
        }
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

static void CollectSpacingGlyph(PAGXDocument* document, const tgfx::Font& font,
                                tgfx::GlyphID glyphID, BitmapBuilders& bitmapBuilders,
                                VectorFontBuilder& vectorBuilder, const tgfx::Color& foreground) {
  float advance = font.getAdvance(glyphID);

  GlyphKey key = {font, glyphID};
  float runFontSize = font.getSize();
  if (runFontSize < MinFontSize) {
    return;
  }
  auto bitmapIt = bitmapBuilders.find(BitmapKey(font, foreground));
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
  } else {
    if (!vectorBuilder.font) {
      vectorBuilder.font = document->makeNode<Font>();
      vectorBuilder.font->unitsPerEm = VectorFontUnitsPerEm;
    }
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

// Generates the next __embed_font_N id that does not collide with an existing user node, so
// setNodeId never silently displaces a user-defined id from the nodeMap. fontIndex is advanced
// across calls so each embedded font gets a distinct sequential id.
static std::string NextEmbedFontId(PAGXDocument* document, int& fontIndex) {
  std::string id;
  do {
    id = "__embed_font_" + std::to_string(fontIndex++);
  } while (document->findNode(id) != nullptr);
  return id;
}

void FontEmbedder::ClearEmbeddedGlyphRuns(PAGXDocument* document) {
  if (document == nullptr) {
    return;
  }
  auto textOrder = CollectAllText(document);
  for (auto* text : textOrder) {
    text->glyphRuns.clear();
  }

  std::unordered_set<Node*> toRemove = {};
  for (auto& node : document->nodes) {
    auto type = node->nodeType();
    if (type == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      for (auto* glyph : font->glyphs) {
        toRemove.insert(glyph);
        if (glyph->path != nullptr) {
          toRemove.insert(glyph->path);
        }
        if (glyph->image != nullptr) {
          toRemove.insert(glyph->image);
        }
      }
      if (!font->file.empty()) {
        font->glyphs.clear();
      } else {
        toRemove.insert(node.get());
      }
    } else if (type == NodeType::GlyphRun) {
      toRemove.insert(node.get());
    }
  }
  document->removeNodes(toRemove);
  document->resetLayoutState();
}

// Each painter captures all preceding geometry, including nested groups. A foreground-dependent
// color glyph can be baked only when all its painters agree on a solid foreground. Reject other
// cases explicitly; one image cannot represent two different currentColor values or a gradient.
struct Foreground {
  tgfx::Color color = tgfx::Color::Black();
  bool seen = false;
  bool supported = true;
};
using Foregrounds = std::unordered_map<Text*, Foreground>;

static std::vector<Text*> CollectForegrounds(const std::vector<Element*>& elements,
                                             Foregrounds& foregrounds) {
  std::vector<Text*> geometry;
  for (auto* element : elements) {
    if (!element) continue;
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      geometry.push_back(static_cast<Text*>(element));
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto children = CollectForegrounds(static_cast<Group*>(element)->elements, foregrounds);
      geometry.insert(geometry.end(), children.begin(), children.end());
    } else if (type == NodeType::Fill || type == NodeType::Stroke) {
      bool supported = type == NodeType::Fill;
      auto color = tgfx::Color::Black();
      if (supported) {
        auto* source = static_cast<Fill*>(element)->color;
        supported = !source || source->nodeType() == NodeType::SolidColor;
        if (source && supported) color = ToTGFX(static_cast<SolidColor*>(source)->color);
        color.alpha = 1;  // The painter retains its own alpha after embedding.
      }
      for (auto* text : geometry) {
        auto& foreground = foregrounds[text];
        foreground.supported &= supported && (!foreground.seen || foreground.color == color);
        foreground.color = color;
        foreground.seen = true;
      }
    }
  }
  return geometry;
}

bool FontEmbedder::embed(PAGXDocument* document) {
  lastError.clear();
  if (!document || !document->isLayoutApplied()) {
    lastError = "applyLayout() must succeed before embedding";
    return false;
  }
  auto textOrder = CollectAllText(document);
  Foregrounds foregrounds;
  for (const auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      CollectForegrounds(static_cast<Layer*>(node.get())->contents, foregrounds);
    }
  }
  std::unordered_map<GlyphKey, GlyphType, GlyphKeyHash> glyphTypes;
  std::unordered_map<GlyphKey, float, GlyphKeyHash> maxFontSizes;
  VectorFontBuilder vectorBuilder;
  BitmapBuilders bitmapBuilders;
  std::vector<BitmapKey> bitmapOrder;
  bool needsRenderShear = false;

  // Validate representation before allocating anything. Unsupported data is never a space.
  for (auto* text : textOrder) {
    for (const auto& run : text->glyphData->layoutRuns) {
      if (!run.font.getTypeface() || run.font.getSize() < MinFontSize) continue;
      for (auto glyphID : run.glyphs) {
        auto key = GlyphKey(run.font, glyphID);
        auto [it, inserted] = glyphTypes.emplace(key, GlyphType::Unknown);
        if (inserted) it->second = ClassifyGlyph(run.font, glyphID);
        if (it->second == GlyphType::Unsupported) {
          lastError = "unsupported glyph " + std::to_string(glyphID) + " in " +
                      run.font.getTypeface()->fontFamily();
          return false;
        }
        if (it->second == GlyphType::Bitmap) {
          needsRenderShear |= run.font.isFauxItalic();
          if (run.font.getGlyphRepresentation(glyphID).usesForeground &&
              !foregrounds[text].supported) {
            lastError = "currentColor glyph requires a single solid foreground: " +
                        run.font.getTypeface()->fontFamily();
            return false;
          }
        }
      }
    }
  }

  // Stage all resources. On failure remove only nodes created by this attempt, retaining the
  // original GlyphRuns and document IDs. Nothing is published until all images decode/encode.
  const auto originalNodeCount = document->nodes.size();
  auto fail = [&](const tgfx::Font& font, tgfx::GlyphID glyphID) {
    if (lastError.empty()) {
      lastError =
          "failed to embed glyph " + std::to_string(glyphID) + " in " +
          (font.getTypeface() ? font.getTypeface()->fontFamily() : std::string("embedded font"));
    }
    std::unordered_set<Node*> staged;
    for (size_t i = originalNodeCount; i < document->nodes.size(); ++i) {
      staged.insert(document->nodes[i].get());
    }
    document->removeNodes(staged);
    return false;
  };
  for (auto* text : textOrder) {
    for (const auto& run : text->glyphData->layoutRuns) {
      if (!run.font.getTypeface() || run.font.getSize() < MinFontSize) continue;
      auto font = run.font;
      if (needsRenderShear) font.setFauxItalic(false);
      for (auto glyphID : run.glyphs) {
        auto type = glyphTypes.at(GlyphKey(run.font, glyphID));
        if (type == GlyphType::Vector) {
          CollectVectorGlyph(document, font, glyphID, maxFontSizes, vectorBuilder);
          if (!vectorBuilder.glyphMapping.count(GlyphKey(font, glyphID)))
            return fail(font, glyphID);
        } else if (type == GlyphType::Bitmap) {
          if (!CollectBitmapGlyph(document, font, glyphID, foregrounds[text].color, bitmapBuilders,
                                  bitmapOrder))
            return fail(font, glyphID);
        }
      }
    }
  }
  // Preserve empty glyph IDs even if they precede the first visible glyph or have zero advance.
  for (auto* text : textOrder) {
    for (const auto& run : text->glyphData->layoutRuns) {
      if (!run.font.getTypeface() || run.font.getSize() < MinFontSize) continue;
      auto font = run.font;
      if (needsRenderShear) font.setFauxItalic(false);
      for (auto glyphID : run.glyphs) {
        if (glyphTypes.at(GlyphKey(run.font, glyphID)) == GlyphType::Spacing) {
          CollectSpacingGlyph(document, font, glyphID, bitmapBuilders, vectorBuilder,
                              foregrounds[text].color);
        }
      }
    }
  }
  if (vectorBuilder.font && vectorBuilder.font->glyphs.size() > 65535) {
    lastError = "too many outline glyphs for one embedded font";
    return fail(tgfx::Font(), 0);
  }
  for (const auto& entry : bitmapBuilders) {
    if (entry.second.font && entry.second.font->glyphs.size() > 65535) {
      lastError = "too many bitmap glyphs for one embedded font";
      return fail(tgfx::Font(), 0);
    }
  }
  int fontIndex = 1;
  if (vectorBuilder.font)
    document->setNodeId(vectorBuilder.font, NextEmbedFontId(document, fontIndex));
  for (const auto& key : bitmapOrder) {
    document->setNodeId(bitmapBuilders.at(key).font, NextEmbedFontId(document, fontIndex));
  }

  for (auto* text : textOrder) {
    std::vector<GlyphRun*> runs;
    for (const auto& run : text->glyphData->layoutRuns) {
      if (!run.font.getTypeface() || run.font.getSize() < MinFontSize) continue;
      auto font = run.font;
      if (needsRenderShear) font.setFauxItalic(false);
      auto bitmapIt = bitmapBuilders.find(BitmapKey(font, foregrounds[text].color));
      auto* bitmap = bitmapIt == bitmapBuilders.end() ? nullptr : &bitmapIt->second;
      // Emit consecutive segments in source order. Grouping all paths before all images changes
      // the paint order of overlapping glyphs (marks, negative spacing, and transformed text).
      size_t start = 0;
      while (start < run.glyphs.size()) {
        auto isBitmap = [&](size_t i) {
          return bitmap && bitmap->glyphMapping.count(GlyphKey(font, run.glyphs[i]));
        };
        bool image = isBitmap(start);
        size_t end = start + 1;
        while (end < run.glyphs.size() && isBitmap(end) == image) ++end;
        std::vector<size_t> indices;
        for (size_t i = start; i < end; ++i) indices.push_back(i);
        auto* embeddedFont = image ? bitmap->font : vectorBuilder.font;
        auto& mapping = image ? bitmap->glyphMapping : vectorBuilder.glyphMapping;
        auto* glyphRun = CreateGlyphRunFromLayoutRun(document, run, font, indices, embeddedFont,
                                                     run.font.getSize(), mapping);
        glyphRun->fauxBold = false;
        glyphRun->fauxItalic = needsRenderShear && run.font.isFauxItalic();
        runs.push_back(glyphRun);
        start = end;
      }
    }
    text->glyphRuns = std::move(runs);
    if (!text->glyphRuns.empty() && (text->textBounds.width > 0 || text->textBounds.height > 0)) {
      text->glyphRuns.front()->bounds = text->textBounds;
    }
  }
  return true;
}

}  // namespace pagx
