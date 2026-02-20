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

#include "TextLayout.h"
#include <cmath>
#include "LineBreaker.h"
#include "PunctuationSquash.h"
#include "VerticalTextUtils.h"
#include "utils/Base64.h"
#include "utils/MathUtil.h"
#include "ToTGFX.h"
#include "utils/UTF8.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/TextBlobBuilder.h"
#ifdef PAG_USE_HARFBUZZ
#include "HarfBuzzShaper.h"
#endif
#ifdef PAG_USE_PAGX_LAYOUT
#include "BidiResolver.h"
#endif

namespace pagx {

void TextLayout::registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  FontKey key = {};
  key.family = typeface->fontFamily();
  key.style = typeface->fontStyle();
  registeredTypefaces[key] = std::move(typeface);
}

void TextLayout::setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) {
  fallbackTypefaces = std::move(typefaces);
}

// Build context that maintains state during text layout
class TextLayoutContext {
 public:
  TextLayoutContext(const TextLayout* textLayout, PAGXDocument* document)
      : textLayout(textLayout), document(document) {
  }

  TextLayoutResult run() {
    if (document == nullptr) {
      return {};
    }

    // Process all layers
    for (auto* layer : document->layers) {
      processLayer(layer);
    }

    // Process compositions in nodes
    for (auto& node : document->nodes) {
      if (node->nodeType() == NodeType::Composition) {
        auto* comp = static_cast<Composition*>(node.get());
        for (auto* layer : comp->layers) {
          processLayer(layer);
        }
      }
    }

    TextLayoutResult output = {};
    output.shapedTextMap = std::move(result);
    output.textOrder = std::move(textOrder);
    return output;
  }

 private:
  // A run of glyphs with the same font (for shaping).
  struct ShapedGlyphRun {
    tgfx::Font font = {};
    std::vector<tgfx::GlyphID> glyphIDs = {};
    std::vector<float> xPositions = {};
    float startX = 0;
    bool canUseDefaultMode = true;
  };

  // Single glyph information for multi-line layout.
  struct GlyphInfo {
    tgfx::GlyphID glyphID = 0;
    tgfx::Font font = {};
    float advance = 0;
    float xPosition = 0;
    int32_t unichar = 0;
    float fontSize = 0;
    float ascent = 0;
    float descent = 0;
    float fontLineHeight = 0;
    Text* sourceText = nullptr;
    uint32_t cluster = 0;
    float xOffset = 0;
    float yOffset = 0;
  };

  struct LineInfo {
    std::vector<GlyphInfo> glyphs = {};
    float width = 0;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxLineHeight = 0;
    float metricsHeight = 0;
    float roundingRatio = 1.0f;
  };

  // Vertical glyph group with layout metrics. Usually contains a single glyph, but consecutive
  // rotated-group characters (Latin letters, digits, etc.) are collected into one group and rendered
  // as a horizontal run rotated 90 degrees (tate-chu-yoko).
  struct VerticalGlyphInfo {
    std::vector<GlyphInfo> glyphs = {};
    VerticalOrientation orientation = VerticalOrientation::Upright;
    float height = 0;
    float width = 0;
    float leadingSquash = 0;
    bool canBreakBefore = false;
  };

  // Column of vertical text, containing glyphs arranged top to bottom.
  struct ColumnInfo {
    std::vector<VerticalGlyphInfo> glyphs = {};
    float height = 0;
    float maxColumnWidth = 0;
  };

  // Shaped text information for a single Text element.
  struct ShapedInfo {
    Text* text = nullptr;
    std::vector<ShapedGlyphRun> runs = {};
    float totalWidth = 0;
    std::vector<GlyphInfo> allGlyphs = {};
  };

  using FontKey = TextLayout::FontKey;

  void StoreShapedText(Text* text, ShapedText&& shapedText) {
    if (text == nullptr || shapedText.textBlob == nullptr) {
      return;
    }
    auto [it, inserted] = result.emplace(text, std::move(shapedText));
    if (inserted) {
      textOrder.push_back(text);
    } else {
      it->second = std::move(shapedText);
    }
  }

  static int StylePriority(const std::string& style) {
    if (style == "Regular") {
      return 0;
    }
    if (style == "Medium") {
      return 1;
    }
    if (style == "Normal") {
      return 2;
    }
    return 3;
  }

  static bool IsPreferredFontKey(const FontKey& candidate, const FontKey& current) {
    int candidatePriority = StylePriority(candidate.style);
    int currentPriority = StylePriority(current.style);
    if (candidatePriority != currentPriority) {
      return candidatePriority < currentPriority;
    }
    if (candidate.style != current.style) {
      return candidate.style < current.style;
    }
    return candidate.family < current.family;
  }

  void processLayer(Layer* layer) {
    if (layer == nullptr) {
      return;
    }

    auto remaining = processScope(layer->contents);
    // Layer is the accumulation boundary. Process any remaining Text elements that were not
    // handled by a TextBox with single-line layout.
    for (auto* text : remaining) {
      if (result.find(text) != result.end()) {
        continue;
      }
      if (!text->glyphRuns.empty()) {
        auto shapedText = buildShapedTextFromEmbeddedGlyphRuns(text);
        StoreShapedText(text, std::move(shapedText));
      } else {
        processTextWithoutLayout(text);
      }
    }

    for (auto* child : layer->children) {
      processLayer(child);
    }
  }

  // Processes a scope of elements following the VectorElement accumulate-render model.
  // Text elements accumulate in the scope. Child Group geometry propagates upward after the
  // Group completes. Per spec, TextBox position in the node order does not affect its behavior:
  // the last TextBox in the scope applies typography to all accumulated Text elements. Returns
  // all accumulated Text elements for upward propagation to the parent scope (up to the Layer
  // boundary).
  std::vector<Text*> processScope(const std::vector<Element*>& elements) {
    std::vector<Text*> allText = {};
    const TextBox* textBox = nullptr;
    for (auto* element : elements) {
      if (element->nodeType() == NodeType::Text) {
        allText.push_back(static_cast<Text*>(element));
      } else if (element->nodeType() == NodeType::Group) {
        auto propagated = processScope(static_cast<Group*>(element)->elements);
        allText.insert(allText.end(), propagated.begin(), propagated.end());
      } else if (element->nodeType() == NodeType::TextBox) {
        textBox = static_cast<const TextBox*>(element);
      }
    }
    if (textBox != nullptr && !allText.empty()) {
      processTextWithLayout(allText, textBox);
    }
    return allText;
  }

  // Builds a TextBlob from shaped glyph runs, applying the given x/y offsets to all positions.
  void buildTextBlobFromShapedInfo(ShapedInfo& info, float xOffset, float yOffset) {
    if (info.runs.empty()) {
      return;
    }
    tgfx::TextBlobBuilder builder = {};
    std::vector<float> adjustedPositions = {};
    for (auto& run : info.runs) {
      if (run.glyphIDs.empty()) {
        continue;
      }
      adjustedPositions.clear();
      adjustedPositions.reserve(run.xPositions.size());
      for (float x : run.xPositions) {
        adjustedPositions.push_back(x + xOffset);
      }
      auto& buffer = builder.allocRunPosH(run.font, run.glyphIDs.size(), yOffset);
      memcpy(buffer.glyphs, run.glyphIDs.data(), run.glyphIDs.size() * sizeof(tgfx::GlyphID));
      memcpy(buffer.positions, adjustedPositions.data(),
             adjustedPositions.size() * sizeof(float));
    }
    auto textBlob = builder.build();
    if (textBlob != nullptr) {
      ShapedText shapedText = {};
      shapedText.textBlob = textBlob;
      StoreShapedText(info.text, std::move(shapedText));
    }
  }

  // Checks whether all Text elements have pre-embedded GlyphRun data. If so, stores them directly
  // and returns true. Otherwise returns false, indicating that re-typesetting is needed.
  bool tryUseEmbeddedGlyphRuns(const std::vector<Text*>& textElements) {
    for (auto* text : textElements) {
      if (text->glyphRuns.empty()) {
        return false;
      }
    }
    for (auto* text : textElements) {
      auto shapedText = buildShapedTextFromEmbeddedGlyphRuns(text);
      StoreShapedText(text, std::move(shapedText));
    }
    return true;
  }

  void processTextWithLayout(std::vector<Text*>& textElements, const TextBox* textBox) {
    if (tryUseEmbeddedGlyphRuns(textElements)) {
      return;
    }

    // Shape each Text and concatenate all glyphs for unified layout within the TextBox.
    std::vector<GlyphInfo> allGlyphs = {};
    float totalWidth = 0;
    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;
      if (!text->text.empty()) {
        shapeText(text, info, textBox->writingMode == WritingMode::Vertical);
      }
      for (auto& g : info.allGlyphs) {
        if (g.unichar != '\n') {
          g.xPosition += totalWidth;
        }
      }
      allGlyphs.insert(allGlyphs.end(), info.allGlyphs.begin(), info.allGlyphs.end());
      totalWidth += info.totalWidth;
    }
    if (allGlyphs.empty()) {
      return;
    }
    if (textBox->writingMode == WritingMode::Vertical) {
      auto columns = layoutColumns(allGlyphs, textBox);
      buildTextBlobVertical(textBox, columns);
    } else {
      auto lines = layoutLines(allGlyphs, textBox);
      buildTextBlobWithLayout(textBox, lines);
    }
  }

  void processTextWithoutLayout(Text* text) {
    ShapedInfo info = {};
    info.text = text;

    if (!text->text.empty()) {
      shapeText(text, info);
    }

    if (info.runs.empty()) {
      return;
    }

    // Check if the text contains any newlines that require multi-line layout.
    bool hasNewline = false;
    for (auto& glyph : info.allGlyphs) {
      if (glyph.unichar == '\n') {
        hasNewline = true;
        break;
      }
    }

    if (hasNewline) {
      buildTextBlobWithoutLayoutMultiLine(text, info);
    } else {
      buildTextBlobWithoutLayoutSingleLine(text, info);
    }
  }

  void buildTextBlobWithoutLayoutSingleLine(Text* text, const ShapedInfo& info) {
    // Calculate text anchor offset based on total text width.
    float anchorOffset = 0;
    switch (text->textAnchor) {
      case TextAnchor::Start:
        break;
      case TextAnchor::Center:
        anchorOffset = -info.totalWidth / 2;
        break;
      case TextAnchor::End:
        anchorOffset = -info.totalWidth;
        break;
    }

    // Build TextBlob with anchor offset applied to all x positions.
    tgfx::TextBlobBuilder builder = {};

    for (auto& run : info.runs) {
      if (run.glyphIDs.empty()) {
        continue;
      }

      if (run.canUseDefaultMode) {
        auto& buffer =
            builder.allocRun(run.font, run.glyphIDs.size(), run.startX + anchorOffset, 0);
        memcpy(buffer.glyphs, run.glyphIDs.data(), run.glyphIDs.size() * sizeof(tgfx::GlyphID));
      } else {
        auto& buffer = builder.allocRunPosH(run.font, run.glyphIDs.size(), 0);
        memcpy(buffer.glyphs, run.glyphIDs.data(), run.glyphIDs.size() * sizeof(tgfx::GlyphID));
        auto* positions = reinterpret_cast<float*>(buffer.positions);
        for (size_t j = 0; j < run.xPositions.size(); j++) {
          positions[j] = run.xPositions[j] + anchorOffset;
        }
      }
    }

    auto textBlob = builder.build();
    if (textBlob != nullptr) {
      ShapedText shapedText = {};
      shapedText.textBlob = textBlob;
      StoreShapedText(text, std::move(shapedText));
    }
  }

  void buildTextBlobWithoutLayoutMultiLine(Text* text, const ShapedInfo& info) {
    // Split allGlyphs into lines by '\n'.
    struct SimpleLine {
      std::vector<const GlyphInfo*> glyphs = {};
      float width = 0;
      float maxAscent = 0;
      float maxDescent = 0;
    };
    std::vector<SimpleLine> lines = {};
    lines.emplace_back();

    for (auto& glyph : info.allGlyphs) {
      if (glyph.unichar == '\n') {
        lines.emplace_back();
        continue;
      }
      auto& line = lines.back();
      line.glyphs.push_back(&glyph);
      float absAscent = fabsf(glyph.ascent);
      if (absAscent > line.maxAscent) {
        line.maxAscent = absAscent;
      }
      if (glyph.descent > line.maxDescent) {
        line.maxDescent = glyph.descent;
      }
      line.width = glyph.xPosition + glyph.advance;
    }

    // Default line height for text without TextBox: 1.2 × fontSize (per spec line 1091).
    float defaultLineHeight = text->fontSize * 1.2f;

    // Calculate baselines: first line baseline = ascent portion of first line,
    // subsequent lines advance by defaultLineHeight.
    float baselineY = 0;
    tgfx::TextBlobBuilder builder = {};

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
      auto& line = lines[lineIdx];
      if (line.glyphs.empty()) {
        baselineY += defaultLineHeight;
        continue;
      }

      if (lineIdx == 0) {
        // First line: baseline at maxAscent (text starts from the top).
        baselineY = line.maxAscent;
      }

      // Calculate text anchor offset for this line independently.
      float anchorOffset = 0;
      switch (text->textAnchor) {
        case TextAnchor::Start:
          break;
        case TextAnchor::Center:
          anchorOffset = -line.width / 2;
          break;
        case TextAnchor::End:
          anchorOffset = -line.width;
          break;
      }

      // Group consecutive glyphs by font into runs for efficient TextBlob building.
      size_t glyphIdx = 0;
      while (glyphIdx < line.glyphs.size()) {
        auto* startGlyph = line.glyphs[glyphIdx];
        auto currentFont = startGlyph->font;
        auto currentTypefaceID = currentFont.getTypeface()->uniqueID();
        size_t runStart = glyphIdx;

        // Collect consecutive glyphs with the same typeface.
        while (glyphIdx < line.glyphs.size()) {
          auto glyphTypefaceID = line.glyphs[glyphIdx]->font.getTypeface()->uniqueID();
          if (glyphTypefaceID != currentTypefaceID) {
            break;
          }
          glyphIdx++;
        }

        size_t runLength = glyphIdx - runStart;
        auto& buffer = builder.allocRunPos(currentFont, runLength);
        for (size_t j = 0; j < runLength; j++) {
          auto* g = line.glyphs[runStart + j];
          buffer.glyphs[j] = g->glyphID;
          auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
          positions[j] = {g->xPosition + anchorOffset, baselineY};
        }
      }

      baselineY += defaultLineHeight;
    }

    auto textBlob = builder.build();
    if (textBlob != nullptr) {
      ShapedText shapedText = {};
      shapedText.textBlob = textBlob;
      StoreShapedText(text, std::move(shapedText));
    }
  }

  ShapedText buildShapedTextFromEmbeddedGlyphRuns(const Text* text) {
    ShapedText shapedText = {};
    tgfx::TextBlobBuilder builder;

    for (const auto& run : text->glyphRuns) {
      if (run->glyphs.empty()) {
        continue;
      }

      auto typeface = buildTypefaceFromFont(run->font);
      if (typeface == nullptr) {
        continue;
      }

      // Calculate font size based on fontSize and unitsPerEm
      // Rendering scale = fontSize / unitsPerEm
      int unitsPerEm = (run->font != nullptr) ? run->font->unitsPerEm : 1;
      if (unitsPerEm <= 0) {
        unitsPerEm = 1;
      }
      float fontSizeForTypeface = run->fontSize / static_cast<float>(unitsPerEm);
      tgfx::Font font(typeface, fontSizeForTypeface);
      font.setFauxBold(text->fauxBold);
      font.setFauxItalic(text->fauxItalic);
      size_t count = run->glyphs.size();

      // Collect anchors for each glyph in this run
      if (!run->anchors.empty()) {
        for (size_t i = 0; i < count; i++) {
          if (i < run->anchors.size()) {
            shapedText.anchors.push_back(tgfx::Point::Make(run->anchors[i].x, run->anchors[i].y));
          } else {
            shapedText.anchors.push_back(tgfx::Point::Zero());
          }
        }
      }

      bool hasTransforms = !run->scales.empty() || !run->rotations.empty() || !run->skews.empty();

      if (hasTransforms) {
        // Matrix mode: build a full affine transform per glyph.
        // Transform order (per spec): translate(-anchor) → scale → skew → rotate →
        // translate(anchor) → translate(position)
        auto& buffer = builder.allocRunMatrix(font, count);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        auto* matrices = reinterpret_cast<tgfx::Matrix*>(buffer.positions);
        float currentX = run->x;
        for (size_t i = 0; i < count; i++) {
          // Compute position
          float posX = 0;
          float posY = 0;
          if (i < run->positions.size()) {
            posX = run->x + run->positions[i].x;
            posY = run->y + run->positions[i].y;
            if (i < run->xOffsets.size()) {
              posX += run->xOffsets[i];
            }
          } else if (i < run->xOffsets.size()) {
            posX = run->x + run->xOffsets[i];
            posY = run->y;
          } else {
            posX = currentX;
            posY = run->y;
            currentX += font.getAdvance(run->glyphs[i]);
          }

          // Get per-glyph transform values (default to identity values)
          float sx = (i < run->scales.size()) ? run->scales[i].x : 1.0f;
          float sy = (i < run->scales.size()) ? run->scales[i].y : 1.0f;
          float rotation = (i < run->rotations.size()) ? run->rotations[i] : 0.0f;
          float skew = (i < run->skews.size()) ? run->skews[i] : 0.0f;

          // Compute anchor: default anchor is (advance * 0.5, 0), plus offset from anchors array
          float anchorX = font.getAdvance(run->glyphs[i]) * 0.5f;
          float anchorY = 0.0f;
          if (i < run->anchors.size()) {
            anchorX += run->anchors[i].x;
            anchorY += run->anchors[i].y;
          }

          // Build the transform matrix
          auto matrix = tgfx::Matrix::I();

          // 1. translate(-anchor)
          matrix.preTranslate(-anchorX, -anchorY);
          // 2. scale
          if (sx != 1.0f || sy != 1.0f) {
            matrix.preScale(sx, sy);
          }
          // 3. skew (along vertical axis)
          if (skew != 0.0f) {
            float skewRadians = skew * static_cast<float>(M_PI) / 180.0f;
            auto skewMatrix = tgfx::Matrix::I();
            skewMatrix.setSkewX(-std::tan(skewRadians));
            matrix.preConcat(skewMatrix);
          }
          // 4. rotate
          if (rotation != 0.0f) {
            matrix.preRotate(rotation);
          }
          // 5. translate(anchor)
          matrix.preTranslate(anchorX, anchorY);
          // 6. translate(position)
          matrix.preTranslate(posX, posY);

          matrices[i] = matrix;
        }
      } else if (!run->positions.empty() && run->positions.size() >= count) {
        // Point mode: each glyph has (x, y) offset combined with overall x/y
        auto& buffer = builder.allocRunPos(font, count);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
        for (size_t i = 0; i < count; i++) {
          float posX = run->x + run->positions[i].x;
          float posY = run->y + run->positions[i].y;
          if (i < run->xOffsets.size()) {
            posX += run->xOffsets[i];
          }
          positions[i] = tgfx::Point::Make(posX, posY);
        }
      } else if (!run->xOffsets.empty() && run->xOffsets.size() >= count) {
        // Horizontal mode: x offsets + shared y
        auto& buffer = builder.allocRunPosH(font, count, run->y);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        for (size_t i = 0; i < count; i++) {
          buffer.positions[i] = run->x + run->xOffsets[i];
        }
      } else {
        // Default mode: use font's advance values to position glyphs
        auto& buffer = builder.allocRun(font, count, run->x, run->y);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        // No positions to fill - tgfx will compute from font advances
      }
    }

    shapedText.textBlob = builder.build();
    return shapedText;
  }

  std::shared_ptr<tgfx::Typeface> buildTypefaceFromFont(const Font* fontNode) {
    if (fontNode == nullptr || fontNode->glyphs.empty()) {
      return nullptr;
    }

    auto it = fontCache.find(fontNode);
    if (it != fontCache.end()) {
      return it->second;
    }

    // Determine if font is path-based or image-based
    bool hasPath = false;
    bool hasImage = false;
    for (const auto& glyph : fontNode->glyphs) {
      if (glyph->path != nullptr) {
        hasPath = true;
      }
      if (glyph->image != nullptr) {
        hasImage = true;
      }
    }

    std::shared_ptr<tgfx::Typeface> typeface = nullptr;
    if (hasPath && !hasImage) {
      tgfx::PathTypefaceBuilder builder;
      for (const auto& glyph : fontNode->glyphs) {
        if (glyph->path != nullptr) {
          auto path = PathDataToTGFXPath(*glyph->path);
          if (glyph->offset.x != 0 || glyph->offset.y != 0) {
            path.transform(tgfx::Matrix::MakeTrans(glyph->offset.x, glyph->offset.y));
          }
          builder.addGlyph(path, glyph->advance);
        } else {
          // Invisible spacing glyph (e.g. space): add with empty path to preserve advance width.
          builder.addGlyph(tgfx::Path(), glyph->advance);
        }
      }
      typeface = builder.detach();
    } else if (hasImage && !hasPath) {
      tgfx::ImageTypefaceBuilder builder;
      for (const auto& glyph : fontNode->glyphs) {
        if (glyph->image != nullptr) {
          std::shared_ptr<tgfx::ImageCodec> codec = nullptr;
          auto imageNode = glyph->image;
          if (imageNode->data != nullptr) {
            codec = tgfx::ImageCodec::MakeFrom(ToTGFXData(imageNode->data));
          } else if (imageNode->filePath.find("data:") == 0) {
            auto data = DecodeBase64DataURI(imageNode->filePath);
            if (data) {
              codec = tgfx::ImageCodec::MakeFrom(ToTGFXData(data));
            }
          } else if (!imageNode->filePath.empty()) {
            codec = tgfx::ImageCodec::MakeFrom(imageNode->filePath);
          }

          if (codec) {
            builder.addGlyph(codec, ToTGFX(glyph->offset), glyph->advance);
          }
        }
      }
      typeface = builder.detach();
    }

    if (typeface) {
      fontCache[fontNode] = typeface;
    }
    return typeface;
  }

  void shapeText(Text* text, ShapedInfo& info, bool vertical = false) {
    auto primaryTypeface = findTypeface(text->fontFamily, text->fontStyle);
    if (primaryTypeface == nullptr) {
      return;
    }

    tgfx::Font primaryFont(primaryTypeface, text->fontSize);
    primaryFont.setFauxBold(text->fauxBold);
    primaryFont.setFauxItalic(text->fauxItalic);
    float currentX = 0;
    const std::string& content = text->text;
    bool hasLetterSpacing = !FloatNearlyEqual(text->letterSpacing, 0.0f);

#ifdef PAG_USE_HARFBUZZ
    // Build fallback fonts list for HarfBuzz shaping.
    std::vector<tgfx::Font> fallbackFonts = {};
    fallbackFonts.reserve(textLayout->fallbackTypefaces.size());
    for (const auto& fallback : textLayout->fallbackTypefaces) {
      if (fallback != nullptr && fallback != primaryTypeface) {
        tgfx::Font fallbackFont(fallback, text->fontSize);
        fallbackFont.setFauxBold(text->fauxBold);
        fallbackFont.setFauxItalic(text->fauxItalic);
        fallbackFonts.push_back(std::move(fallbackFont));
      }
    }

    // Collect newline and tab positions, then shape non-special segments.
    struct TextSegment {
      size_t start = 0;
      size_t length = 0;
      bool isNewline = false;
      bool isTab = false;
#ifdef PAG_USE_PAGX_LAYOUT
      bool isRTL = false;
#endif
    };
    std::vector<TextSegment> segments = {};

#ifdef PAG_USE_PAGX_LAYOUT
    // Use BiDi resolver to split text into directional runs, then further split by newlines/tabs.
    auto bidiRuns = BidiResolver::Resolve(content);
    for (auto& run : bidiRuns) {
      size_t runEnd = run.start + run.length;
      size_t pos = run.start;
      while (pos < runEnd) {
        int32_t ch = 0;
        size_t charLen = DecodeUTF8Char(content.data() + pos, runEnd - pos, &ch);
        if (charLen == 0) {
          pos++;
          continue;
        }
        if (ch == '\n') {
          TextSegment seg = {};
          seg.start = pos;
          seg.length = charLen;
          seg.isNewline = true;
          segments.push_back(seg);
          pos += charLen;
        } else if (ch == '\t') {
          TextSegment seg = {};
          seg.start = pos;
          seg.length = charLen;
          seg.isTab = true;
          segments.push_back(seg);
          pos += charLen;
        } else {
          // Start a text segment, collecting until newline/tab or end of run.
          size_t segStart = pos;
          pos += charLen;
          while (pos < runEnd) {
            int32_t nextCh = 0;
            size_t nextLen = DecodeUTF8Char(content.data() + pos, runEnd - pos, &nextCh);
            if (nextLen == 0) {
              pos++;
              break;
            }
            if (nextCh == '\n' || nextCh == '\t') {
              break;
            }
            pos += nextLen;
          }
          TextSegment seg = {};
          seg.start = segStart;
          seg.length = pos - segStart;
          seg.isRTL = run.isRTL;
          segments.push_back(seg);
        }
      }
    }
#else
    // Without BiDi: split by newlines and tabs only.
    {
      size_t pos = 0;
      while (pos < content.size()) {
        int32_t ch = 0;
        size_t charLen = DecodeUTF8Char(content.data() + pos, content.size() - pos, &ch);
        if (charLen == 0) {
          pos++;
          continue;
        }
        if (ch == '\n') {
          TextSegment seg = {};
          seg.start = pos;
          seg.length = charLen;
          seg.isNewline = true;
          segments.push_back(seg);
          pos += charLen;
        } else if (ch == '\t') {
          TextSegment seg = {};
          seg.start = pos;
          seg.length = charLen;
          seg.isTab = true;
          segments.push_back(seg);
          pos += charLen;
        } else {
          size_t segStart = pos;
          pos += charLen;
          while (pos < content.size()) {
            int32_t nextCh = 0;
            size_t nextLen = DecodeUTF8Char(content.data() + pos, content.size() - pos, &nextCh);
            if (nextLen == 0) {
              pos++;
              break;
            }
            if (nextCh == '\n' || nextCh == '\t') {
              break;
            }
            pos += nextLen;
          }
          TextSegment seg = {};
          seg.start = segStart;
          seg.length = pos - segStart;
          segments.push_back(seg);
        }
      }
    }
#endif

    ShapedGlyphRun* currentRun = nullptr;
    std::shared_ptr<tgfx::Typeface> currentTypeface = nullptr;
    float tabWidth = text->fontSize * 4;

    for (auto& seg : segments) {
      if (seg.isNewline) {
        auto metrics = primaryFont.getMetrics();
        GlyphInfo gi = {};
        gi.unichar = '\n';
        gi.fontSize = text->fontSize;
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
        gi.sourceText = text;
        info.allGlyphs.push_back(gi);
        currentX = 0;
        currentTypeface = nullptr;
        continue;
      }

      if (seg.isTab) {
        float nextTabStop = 0;
        if (tabWidth > 0) {
          nextTabStop = ceilf((currentX + 1.0f) / tabWidth) * tabWidth;
        }
        float tabAdvance = nextTabStop - currentX;
        if (tabAdvance < 0) {
          tabAdvance = 0;
        }
        GlyphInfo gi = {};
        gi.unichar = '\t';
        gi.advance = tabAdvance;
        gi.xPosition = currentX;
        gi.fontSize = text->fontSize;
        auto metrics = primaryFont.getMetrics();
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
        gi.sourceText = text;
        info.allGlyphs.push_back(gi);
        currentX += tabAdvance;
        currentTypeface = nullptr;
        continue;
      }

      // Shape this text segment with HarfBuzz.
      auto substring = content.substr(seg.start, seg.length);
      bool rtl = false;
#ifdef PAG_USE_PAGX_LAYOUT
      rtl = seg.isRTL;
#endif
      auto shapedGlyphs = HarfBuzzShaper::Shape(substring, primaryFont, fallbackFonts,
                                                 vertical, rtl);

      for (auto& sg : shapedGlyphs) {
        if (sg.glyphID == 0) {
          continue;
        }

        auto glyphTypeface = sg.font.getTypeface();
        if (currentTypeface == nullptr || glyphTypeface != currentTypeface) {
          info.runs.emplace_back();
          currentRun = &info.runs.back();
          currentRun->font = sg.font;
          currentTypeface = glyphTypeface;
          currentRun->startX = currentX;
          currentRun->canUseDefaultMode = false;
          currentRun->glyphIDs.reserve(shapedGlyphs.size());
          currentRun->xPositions.reserve(shapedGlyphs.size());
        }

        currentRun->xPositions.push_back(currentX + sg.xOffset);
        currentRun->glyphIDs.push_back(sg.glyphID);

        auto metrics = sg.font.getMetrics();
        // Decode the unichar at this cluster position for line breaking and other logic.
        int32_t unichar = 0;
        size_t clusterByteOffset = seg.start + sg.cluster;
        if (clusterByteOffset < content.size()) {
          DecodeUTF8Char(content.data() + clusterByteOffset,
                         content.size() - clusterByteOffset, &unichar);
        }

        GlyphInfo gi = {};
        gi.glyphID = sg.glyphID;
        gi.font = sg.font;
        gi.advance = sg.xAdvance;
        gi.xPosition = currentX;
        gi.unichar = unichar;
        gi.fontSize = text->fontSize;
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
        gi.sourceText = text;
        gi.cluster = static_cast<uint32_t>(seg.start) + sg.cluster;
        gi.xOffset = sg.xOffset;
        gi.yOffset = sg.yOffset;
        info.allGlyphs.push_back(gi);

        currentX += sg.xAdvance + text->letterSpacing;
      }
    }

#else
    // Non-HarfBuzz path: original per-character glyph lookup.

    // Pre-build fallback font cache to avoid repeated construction in the inner loop.
    std::unordered_map<tgfx::Typeface*, tgfx::Font> fallbackFontCache = {};
    for (const auto& fallback : textLayout->fallbackTypefaces) {
      if (fallback != nullptr && fallback != primaryTypeface) {
        tgfx::Font fallbackFont(fallback, text->fontSize);
        fallbackFont.setFauxBold(text->fauxBold);
        fallbackFont.setFauxItalic(text->fauxItalic);
        fallbackFontCache.emplace(fallback.get(), std::move(fallbackFont));
      }
    }

    // Current run being built
    ShapedGlyphRun* currentRun = nullptr;
    std::shared_ptr<tgfx::Typeface> currentTypeface = nullptr;

    size_t i = 0;
    while (i < content.size()) {
      int32_t unichar = 0;
      size_t charLen = DecodeUTF8Char(content.data() + i, content.size() - i, &unichar);
      if (charLen == 0) {
        i++;
        continue;
      }
      i += charLen;

      // Handle newline: store font metrics so \n participates in line metrics calculation
      // when it becomes the leading cluster of the next line.
      if (unichar == '\n') {
        auto metrics = primaryFont.getMetrics();
        GlyphInfo gi = {};
        gi.unichar = '\n';
        gi.fontSize = text->fontSize;
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
        gi.sourceText = text;
        info.allGlyphs.push_back(gi);
        currentX = 0;
        currentTypeface = nullptr;
        continue;
      }

      // Handle tab character.
      if (unichar == '\t') {
        float tabWidth = text->fontSize * 4;
        float nextTabStop = 0;
        if (tabWidth > 0) {
          nextTabStop = ceilf((currentX + 1.0f) / tabWidth) * tabWidth;
        }
        float tabAdvance = nextTabStop - currentX;
        if (tabAdvance < 0) {
          tabAdvance = 0;
        }
        GlyphInfo gi = {};
        gi.unichar = '\t';
        gi.advance = tabAdvance;
        gi.xPosition = currentX;
        gi.fontSize = text->fontSize;
        auto metrics = primaryFont.getMetrics();
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
        gi.sourceText = text;
        info.allGlyphs.push_back(gi);
        currentX += tabAdvance;
        currentTypeface = nullptr;
        continue;
      }

      // Try to find glyph in primary font or fallbacks
      tgfx::GlyphID glyphID = primaryFont.getGlyphID(unichar);
      tgfx::Font glyphFont = primaryFont;
      std::shared_ptr<tgfx::Typeface> glyphTypeface = primaryTypeface;

      if (glyphID == 0) {
        for (const auto& fallback : textLayout->fallbackTypefaces) {
          auto it = fallbackFontCache.find(fallback.get());
          if (it == fallbackFontCache.end()) {
            continue;
          }
          glyphID = it->second.getGlyphID(unichar);
          if (glyphID != 0) {
            glyphFont = it->second;
            glyphTypeface = fallback;
            break;
          }
        }
      }

      if (glyphID == 0) {
        continue;
      }

      float advance = glyphFont.getAdvance(glyphID);

      // Start new run if typeface changed
      if (currentTypeface != glyphTypeface) {
        info.runs.emplace_back();
        currentRun = &info.runs.back();
        currentRun->font = glyphFont;
        currentTypeface = glyphTypeface;
        currentRun->startX = currentX;
        currentRun->canUseDefaultMode = !hasLetterSpacing;
        // Reserve using remaining character count estimate (assuming average 2 bytes per char).
        auto remaining = (content.size() - i + charLen) / 2 + 1;
        currentRun->glyphIDs.reserve(remaining);
        currentRun->xPositions.reserve(remaining);
      }

      currentRun->xPositions.push_back(currentX);
      currentRun->glyphIDs.push_back(glyphID);

      auto metrics = glyphFont.getMetrics();
      GlyphInfo gi = {};
      gi.glyphID = glyphID;
      gi.font = glyphFont;
      gi.advance = advance;
      gi.xPosition = currentX;
      gi.unichar = unichar;
      gi.fontSize = text->fontSize;
      gi.ascent = metrics.ascent;
      gi.descent = metrics.descent;
      gi.fontLineHeight = fabsf(metrics.ascent) + metrics.descent + metrics.leading;
      gi.sourceText = text;
      info.allGlyphs.push_back(gi);

      currentX += advance + text->letterSpacing;
    }
#endif

    // Remove the extra letterSpacing after the last glyph.
    if (hasLetterSpacing && currentX > 0) {
      currentX -= text->letterSpacing;
    }
    info.totalWidth = currentX;
  }

  std::vector<LineInfo> layoutLines(const std::vector<GlyphInfo>& allGlyphs,
                                    const TextBox* textBox) {
    std::vector<LineInfo> lines = {};
    lines.emplace_back();
    auto* currentLine = &lines.back();
    float currentLineWidth = 0;
    int lastBreakIndex = -1;
    float boxWidth = textBox->size.width;
    bool doWrap = textBox->wordWrap && boxWidth > 0;

    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];

      if (glyph.unichar == '\n') {
        FinishLine(currentLine, textBox->lineHeight, glyph.fontLineHeight);
        lines.emplace_back();
        currentLine = &lines.back();
        currentLineWidth = 0;
        lastBreakIndex = -1;
        continue;
      }

      float letterSpacing = (glyph.sourceText != nullptr) ? glyph.sourceText->letterSpacing : 0;
      float glyphEndX = currentLineWidth + glyph.advance;

      // Auto-wrap check
      if (doWrap && !currentLine->glyphs.empty() && glyphEndX > boxWidth) {
        if (lastBreakIndex >= 0) {
          // Split at break point: move glyphs after lastBreakIndex to new line
          std::vector<GlyphInfo> overflow(
              currentLine->glyphs.begin() + lastBreakIndex + 1, currentLine->glyphs.end());
          currentLine->glyphs.resize(lastBreakIndex + 1);
          // Trim trailing whitespace from current line
          while (!currentLine->glyphs.empty() &&
                 LineBreaker::isWhitespace(currentLine->glyphs.back().unichar)) {
            currentLine->glyphs.pop_back();
          }
          FinishLine(currentLine, textBox->lineHeight, 0.0f);
          lines.emplace_back();
          currentLine = &lines.back();
          // Skip leading whitespace in overflow
          size_t skipCount = 0;
          while (skipCount < overflow.size() &&
                 LineBreaker::isWhitespace(overflow[skipCount].unichar)) {
            skipCount++;
          }
          // Recalculate positions for overflow glyphs
          currentLineWidth = 0;
          for (size_t j = skipCount; j < overflow.size(); j++) {
            overflow[j].xPosition = currentLineWidth;
            float ls = (overflow[j].sourceText != nullptr) ? overflow[j].sourceText->letterSpacing
                                                           : 0;
            currentLineWidth += overflow[j].advance + ls;
            currentLine->glyphs.push_back(overflow[j]);
          }
          // Add current glyph
          GlyphInfo adjusted = glyph;
          adjusted.xPosition = currentLineWidth;
          currentLineWidth += glyph.advance + letterSpacing;
          currentLine->glyphs.push_back(adjusted);
        } else {
          // No break point found - force break before current glyph
          if (!currentLine->glyphs.empty()) {
            FinishLine(currentLine, textBox->lineHeight, 0.0f);
            lines.emplace_back();
            currentLine = &lines.back();
          }
          GlyphInfo adjusted = glyph;
          adjusted.xPosition = 0;
          currentLine->glyphs.push_back(adjusted);
          currentLineWidth = glyph.advance + letterSpacing;
        }
        lastBreakIndex = -1;
        continue;
      }

      GlyphInfo adjusted = glyph;
      adjusted.xPosition = currentLineWidth;
      currentLine->glyphs.push_back(adjusted);
      currentLineWidth += glyph.advance + letterSpacing;

      // Update break opportunity
      if (i + 1 < allGlyphs.size() && allGlyphs[i + 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
        // Cluster-aware breaking: never break within the same HarfBuzz cluster.
        bool sameCluster = (glyph.cluster != 0 || allGlyphs[i + 1].cluster != 0) &&
                           glyph.cluster == allGlyphs[i + 1].cluster;
        if (!sameCluster &&
            LineBreaker::canBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
        }
#else
        if (LineBreaker::canBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
        }
#endif
      }
    }

    FinishLine(currentLine, textBox->lineHeight, 0.0f);

#ifdef PAG_USE_PAGX_LAYOUT
    // Apply punctuation squash to all lines.
    ApplyPunctuationSquashToLines(lines);
#endif

    return lines;
  }

#ifdef PAG_USE_PAGX_LAYOUT
  static void ApplyPunctuationSquashToLines(std::vector<LineInfo>& lines) {
    for (auto& line : lines) {
      if (line.glyphs.empty()) {
        continue;
      }

      auto glyphCount = line.glyphs.size();
      // Leading squash removes whitespace before the glyph face (Opening punctuation).
      // Trailing squash removes whitespace after the glyph face (Closing punctuation).
      std::vector<float> leadingSquash(glyphCount, 0);
      std::vector<float> trailingSquash(glyphCount, 0);

      // Line-start squash: remove leading whitespace of the first non-whitespace glyph.
      // Only Opening punctuation has leading whitespace.
      for (size_t i = 0; i < glyphCount; i++) {
        if (LineBreaker::isWhitespace(line.glyphs[i].unichar)) {
          continue;
        }
        float fraction = PunctuationSquash::GetLineStartSquash(line.glyphs[i].unichar);
        leadingSquash[i] = line.glyphs[i].advance * fraction;
        break;
      }

      // Line-end squash: remove trailing whitespace of the last non-whitespace glyph.
      // Only Closing punctuation has trailing whitespace.
      for (int i = static_cast<int>(glyphCount) - 1; i >= 0; i--) {
        if (LineBreaker::isWhitespace(line.glyphs[i].unichar)) {
          continue;
        }
        float fraction = PunctuationSquash::GetLineEndSquash(line.glyphs[i].unichar);
        trailingSquash[i] = line.glyphs[i].advance * fraction;
        break;
      }

      // Adjacent punctuation squash. The squash type depends on where each character's internal
      // whitespace is:
      //   - Opening punctuation: whitespace on the leading (left) side
      //   - Closing punctuation: whitespace on the trailing (right) side
      //   - MiddleDot: whitespace on both sides
      // prevSquash removes the trailing whitespace of prevChar.
      // nextSquash removes the leading whitespace of nextChar.
      for (size_t i = 0; i + 1 < glyphCount; i++) {
        auto result = PunctuationSquash::GetAdjacentSquash(line.glyphs[i].unichar,
                                                           line.glyphs[i + 1].unichar);
        if (result.prevSquash > 0) {
          auto prevCat = PunctuationSquash::GetCategory(line.glyphs[i].unichar);
          float amount = line.glyphs[i].advance * result.prevSquash;
          if (prevCat == PunctuationCategory::Opening) {
            // Opening punctuation has leading whitespace. When the squash table says to remove
            // the "trailing space" of an Opening bracket (e.g. Opening+Opening: 「「), it actually
            // means the glyph face is already at the trailing edge, and we should NOT reduce the
            // advance. Instead this case should not arise in a correct JLREQ table, but if it
            // does we treat it as trailing squash to avoid breaking the glyph.
            if (amount > trailingSquash[i]) {
              trailingSquash[i] = amount;
            }
          } else {
            // Closing and MiddleDot have trailing whitespace, so squash reduces their advance.
            if (amount > trailingSquash[i]) {
              trailingSquash[i] = amount;
            }
          }
        }
        if (result.nextSquash > 0) {
          auto nextCat = PunctuationSquash::GetCategory(line.glyphs[i + 1].unichar);
          float amount = line.glyphs[i + 1].advance * result.nextSquash;
          if (nextCat == PunctuationCategory::Opening) {
            // Opening punctuation: squash removes leading whitespace (before the glyph face).
            if (amount > leadingSquash[i + 1]) {
              leadingSquash[i + 1] = amount;
            }
          } else if (nextCat == PunctuationCategory::MiddleDot) {
            // MiddleDot: nextSquash removes leading whitespace.
            if (amount > leadingSquash[i + 1]) {
              leadingSquash[i + 1] = amount;
            }
          } else {
            // Closing punctuation: nextSquash removes leading whitespace (the space before the
            // glyph face of a Closing bracket).
            if (amount > leadingSquash[i + 1]) {
              leadingSquash[i + 1] = amount;
            }
          }
        }
      }

      // Recalculate positions with squash applied.
      // Leading squash shifts the glyph's xPosition backward (removes space before the glyph).
      // Trailing squash reduces the effective advance (removes space after the glyph).
      float xPos = 0;
      for (size_t i = 0; i < glyphCount; i++) {
        line.glyphs[i].xPosition = xPos - leadingSquash[i];
        float effectiveAdvance = line.glyphs[i].advance - leadingSquash[i] - trailingSquash[i];
        float ls = (line.glyphs[i].sourceText != nullptr)
                       ? line.glyphs[i].sourceText->letterSpacing
                       : 0;
        xPos += effectiveAdvance + ls;
      }

      // Recalculate line width.
      if (!line.glyphs.empty()) {
        auto& lastGlyph = line.glyphs.back();
        float lastEffectiveAdvance =
            lastGlyph.advance - leadingSquash[glyphCount - 1] - trailingSquash[glyphCount - 1];
        line.width = lastGlyph.xPosition + leadingSquash[glyphCount - 1] + lastEffectiveAdvance;
      }
    }
  }
#endif

  static void FinishLine(LineInfo* line, float lineHeight, float newlineFontLineHeight) {
    if (line->glyphs.empty()) {
      // Empty line from line breaks: use the newline font metrics for height.
      if (lineHeight > 0) {
        line->maxLineHeight = lineHeight;
        line->roundingRatio = 1.0f;
      } else if (newlineFontLineHeight > 0) {
        line->maxLineHeight = roundf(newlineFontLineHeight);
        line->roundingRatio = line->maxLineHeight / newlineFontLineHeight;
      }
      return;
    }
    auto& lastGlyph = line->glyphs.back();
    line->width = lastGlyph.xPosition + lastGlyph.advance;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxFontLineHeight = 0;
    for (auto& g : line->glyphs) {
      float absAscent = fabsf(g.ascent);
      if (absAscent > maxAscent) {
        maxAscent = absAscent;
      }
      if (g.descent > maxDescent) {
        maxDescent = g.descent;
      }
      if (g.fontLineHeight > maxFontLineHeight) {
        maxFontLineHeight = g.fontLineHeight;
      }
    }
    line->maxAscent = maxAscent;
    line->maxDescent = maxDescent;
    line->metricsHeight = maxFontLineHeight;
    if (lineHeight > 0) {
      line->maxLineHeight = lineHeight;
      line->roundingRatio = 1.0f;
    } else if (line->metricsHeight > 0) {
      line->maxLineHeight = roundf(line->metricsHeight);
      line->roundingRatio = line->maxLineHeight / line->metricsHeight;
    }
  }

  void buildTextBlobWithLayout(const TextBox* textBox, const std::vector<LineInfo>& lines) {
    if (lines.empty()) {
      return;
    }

    float boxWidth = textBox->size.width;
    float boxHeight = textBox->size.height;

    // Calculate total height using line-box model: each line contributes its full lineHeight.
    float totalHeight = 0;
    for (size_t i = 0; i < lines.size(); i++) {
      totalHeight += lines[i].maxLineHeight;
    }

    // Vertical alignment offset.
    float yOffset = 0;
    if (boxHeight > 0) {
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Near:
          yOffset = 0;
          break;
        case ParagraphAlign::Middle:
          yOffset = (boxHeight - totalHeight) / 2;
          break;
        case ParagraphAlign::Far:
          yOffset = boxHeight - totalHeight;
          break;
      }
    } else {
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Near:
          yOffset = 0;
          break;
        case ParagraphAlign::Middle:
          yOffset = -totalHeight / 2;
          break;
        case ParagraphAlign::Far:
          yOffset = -totalHeight;
          break;
      }
    }

    // Collect positioned glyphs grouped by source Text element.
    struct PositionedGlyph {
      tgfx::GlyphID glyphID = 0;
      tgfx::Font font = {};
      float x = 0;
      float y = 0;
    };
    std::unordered_map<Text*, std::vector<PositionedGlyph>> textGlyphs = {};

    bool overflowHidden = textBox->overflow == Overflow::Hidden;
    float boxBottom = textBox->position.y + boxHeight;
    // Use relative coordinates for baseline calculation, then add textBox position at the end.
    float relativeTop = 0;
    float baselineY = 0;
    // Track the previous line's relative baseline for the equal-spacing model used by subsequent
    // lines with fixed line height: baseline(n) = baseline(n-1) + lineHeight(n-1).
    float prevRelativeBaseline = 0;
    bool hasPrevBaseline = false;

    // For Bottom and Center alignment, pre-compute baselines from the last line upward so that the
    // last line is anchored at the box bottom and preceding lines are spaced by lineHeight. This
    // ensures correct baseline positions when lines have different heights (mixed font sizes).
    std::vector<float> precomputedBaselines = {};
    if ((textBox->paragraphAlign == ParagraphAlign::Far ||
         textBox->paragraphAlign == ParagraphAlign::Middle) &&
        lines.size() > 1) {
      precomputedBaselines.resize(lines.size(), 0);
      // Start from the last content line.
      auto lastIdx = lines.size() - 1;
      auto& lastLine = lines[lastIdx];
      float lastRelBaseline = 0;
      if (!lastLine.glyphs.empty()) {
        float halfLeading = (lastLine.maxLineHeight - lastLine.metricsHeight) / 2;
        lastRelBaseline =
            (totalHeight - lastLine.maxLineHeight + halfLeading + lastLine.maxAscent) *
            lastLine.roundingRatio;
      } else {
        lastRelBaseline =
            (totalHeight - lastLine.maxLineHeight + lastLine.maxLineHeight) * lastLine.roundingRatio;
      }
      precomputedBaselines[lastIdx] = lastRelBaseline;
      // Walk upward: each preceding line's baseline = next line's baseline - next line's lineHeight
      for (int i = static_cast<int>(lastIdx) - 1; i >= 0; i--) {
        precomputedBaselines[i] = precomputedBaselines[i + 1] - lines[i + 1].maxLineHeight;
      }
    }

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
      auto& line = lines[lineIdx];
      float relativeBaseline = 0;

      if (line.glyphs.empty()) {
        relativeBaseline = (relativeTop + line.maxLineHeight) * line.roundingRatio;
        baselineY = textBox->position.y + roundf(relativeBaseline + yOffset);
      } else if (!precomputedBaselines.empty()) {
        // Bottom/Center alignment: use pre-computed baselines anchored from the last line.
        relativeBaseline = precomputedBaselines[lineIdx];
        baselineY = textBox->position.y + roundf(relativeBaseline + yOffset);
      } else if (hasPrevBaseline && textBox->lineHeight > 0) {
        // Fixed line height, subsequent lines: baseline = prevBaseline + lineHeight.
        // This produces equal baseline-to-baseline spacing matching Figma's behavior where
        // subsequent lines have their leading added above rather than split above and below.
        relativeBaseline = prevRelativeBaseline + lines[lineIdx - 1].maxLineHeight;
        baselineY = textBox->position.y + roundf(relativeBaseline + yOffset);
      } else {
        // Auto line height or first content line: use half-leading model.
        float halfLeading = (line.maxLineHeight - line.metricsHeight) / 2;
        relativeBaseline = (relativeTop + halfLeading + line.maxAscent) * line.roundingRatio;
        baselineY = textBox->position.y + roundf(relativeBaseline + yOffset);
      }
      prevRelativeBaseline = relativeBaseline;
      hasPrevBaseline = !line.glyphs.empty();
      relativeTop += line.maxLineHeight;

      // Skip lines that overflow below the box bottom.
      if (overflowHidden && boxHeight > 0) {
        float lineBottom = baselineY + line.maxDescent;
        if (lineBottom > boxBottom) {
          break;
        }
      }

      // Horizontal alignment.
      float xOffset = textBox->position.x;
      float justifyExtraPerGap = 0;
      if (boxWidth > 0) {
        switch (textBox->textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            xOffset += (boxWidth - line.width) / 2;
            break;
          case TextAlign::End:
            xOffset += boxWidth - line.width;
            break;
          case TextAlign::Justify: {
            // Justify: distribute extra space at word boundaries. Last line uses Start.
            if (lineIdx < lines.size() - 1 && line.glyphs.size() > 1) {
              int gapCount = 0;
              for (size_t i = 0; i + 1 < line.glyphs.size(); i++) {
                if (LineBreaker::canBreakBetween(line.glyphs[i].unichar,
                                                 line.glyphs[i + 1].unichar)) {
                  gapCount++;
                }
              }
              if (gapCount > 0) {
                justifyExtraPerGap =
                    (boxWidth - line.width) / static_cast<float>(gapCount);
              }
            }
            break;
          }
        }
      } else {
        switch (textBox->textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            xOffset -= line.width / 2;
            break;
          case TextAlign::End:
            xOffset -= line.width;
            break;
          case TextAlign::Justify:
            break;
        }
      }

      float justifyOffset = 0;
      for (size_t gi = 0; gi < line.glyphs.size(); gi++) {
        auto& g = line.glyphs[gi];
        // Skip newline and tab glyphs: they only participate in metrics/spacing, not rendering.
        if (g.unichar == '\n' || g.unichar == '\t') {
          continue;
        }
        if (gi > 0 && LineBreaker::canBreakBetween(line.glyphs[gi - 1].unichar, g.unichar)) {
            justifyOffset += justifyExtraPerGap;
        }
        PositionedGlyph pg = {};
        pg.glyphID = g.glyphID;
        pg.font = g.font;
        pg.x = g.xPosition + xOffset + justifyOffset + g.xOffset;
        pg.y = baselineY - g.yOffset;
        textGlyphs[g.sourceText].push_back(pg);
      }
    }

    // Build TextBlob for each Text element with inverse-transform compensation:
    // subtract text->position so that LayerBuilder's setPosition(text.position) restores the
    // correct absolute coordinates.
    for (auto& [text, glyphs] : textGlyphs) {
      if (glyphs.empty() || text == nullptr) {
        continue;
      }

      float compensateX = text->position.x;
      float compensateY = text->position.y;

      tgfx::TextBlobBuilder builder = {};
      // Group consecutive glyphs with the same font into runs.
      size_t runStart = 0;
      while (runStart < glyphs.size()) {
        auto& runFont = glyphs[runStart].font;
        size_t runEnd = runStart + 1;
        while (runEnd < glyphs.size() && glyphs[runEnd].font == runFont) {
          runEnd++;
        }
        size_t count = runEnd - runStart;
        auto& buffer = builder.allocRunPos(runFont, count);
        auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
        for (size_t j = 0; j < count; j++) {
          buffer.glyphs[j] = glyphs[runStart + j].glyphID;
          positions[j] = tgfx::Point::Make(glyphs[runStart + j].x - compensateX,
                                            glyphs[runStart + j].y - compensateY);
        }
        runStart = runEnd;
      }

      auto textBlob = builder.build();
      if (textBlob != nullptr) {
        ShapedText shapedText = {};
        shapedText.textBlob = textBlob;
        StoreShapedText(text, std::move(shapedText));
      }
    }
  }

  static void FinishColumn(ColumnInfo* column, float lineHeight) {
    float height = 0;
    float maxFontLineHeight = 0;
    for (auto& vg : column->glyphs) {
      height += vg.height;
      if (vg.glyphs.front().fontLineHeight > maxFontLineHeight) {
        maxFontLineHeight = vg.glyphs.front().fontLineHeight;
      }
    }
    column->height = height;
    // Auto column width uses font metrics height (ascent + descent + leading) to match
    // horizontal layout behavior where auto lineHeight = metricsHeight. This ensures rotated
    // Latin glyphs fit within the column width.
    column->maxColumnWidth = (lineHeight > 0) ? lineHeight : roundf(maxFontLineHeight);
  }

  std::vector<ColumnInfo> layoutColumns(const std::vector<GlyphInfo>& allGlyphs,
                                        const TextBox* textBox) {
    // Phase 1: Build VerticalGlyphInfo list with orientation, metrics, and break marks.
    // Consecutive rotated-group characters are collected into a single VerticalGlyphInfo.
    std::vector<VerticalGlyphInfo> vgList = {};
    vgList.reserve(allGlyphs.size());
    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];
      if (glyph.unichar == '\n') {
        VerticalGlyphInfo vg = {};
        vg.glyphs.push_back(glyph);
        vgList.push_back(std::move(vg));
        continue;
      }

      float letterSpacing = (glyph.sourceText != nullptr) ? glyph.sourceText->letterSpacing : 0;
      auto orientation = VerticalTextUtils::getOrientation(glyph.unichar);

      if (orientation == VerticalOrientation::Rotated &&
          VerticalTextUtils::isRotatedGroupChar(glyph.unichar)) {
        // Collect consecutive rotated-group characters into a single group.
        VerticalGlyphInfo vg = {};
        vg.orientation = VerticalOrientation::Rotated;
        float groupHorizontalWidth = 0;
        float maxFontSize = 0;
        size_t groupStart = i;
        while (i < allGlyphs.size() && allGlyphs[i].unichar != '\n' &&
               VerticalTextUtils::isRotatedGroupChar(allGlyphs[i].unichar)) {
          vg.glyphs.push_back(allGlyphs[i]);
          // Use horizontal advance from font metrics, not HarfBuzz's xAdvance which may be 0
          // in TTB shaping mode.
          float hAdvance = allGlyphs[i].font.getAdvance(allGlyphs[i].glyphID);
          groupHorizontalWidth += hAdvance;
          if (allGlyphs[i].fontSize > maxFontSize) {
            maxFontSize = allGlyphs[i].fontSize;
          }
          i++;
        }
        i--;  // Compensate for the outer loop's i++.
        vg.height = groupHorizontalWidth + letterSpacing;
        vg.width = maxFontSize;
        if (groupStart > 0 && allGlyphs[groupStart - 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
          auto& prevGlyph = allGlyphs[groupStart - 1];
          auto& firstGlyph = allGlyphs[groupStart];
          bool sameCluster = (firstGlyph.cluster != 0 || prevGlyph.cluster != 0) &&
                             firstGlyph.cluster == prevGlyph.cluster;
          vg.canBreakBefore =
              !sameCluster &&
              LineBreaker::canBreakBetween(prevGlyph.unichar, firstGlyph.unichar);
#else
          vg.canBreakBefore = LineBreaker::canBreakBetween(allGlyphs[groupStart - 1].unichar,
                                                           allGlyphs[groupStart].unichar);
#endif
        }
        vgList.push_back(std::move(vg));
      } else {
        // Single glyph (upright CJK, or non-groupable rotated character).
        VerticalGlyphInfo vg = {};
        vg.glyphs.push_back(glyph);
        vg.orientation = orientation;
        if (orientation == VerticalOrientation::Rotated) {
          // Use horizontal advance from font metrics; HarfBuzz TTB xAdvance may be 0.
          float hAdvance = glyph.font.getAdvance(glyph.glyphID);
          vg.height = hAdvance + letterSpacing;
          vg.width = glyph.fontSize;
        } else {
          // For upright characters, use the vertical advance from font metrics.
          float vertAdvance = glyph.font.getAdvance(glyph.glyphID, true);
          vg.height = vertAdvance + letterSpacing;
          vg.width = glyph.fontSize;
        }

        if (i > 0 && allGlyphs[i - 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
          bool sameCluster = (glyph.cluster != 0 || allGlyphs[i - 1].cluster != 0) &&
                             glyph.cluster == allGlyphs[i - 1].cluster;
          vg.canBreakBefore = !sameCluster &&
                              LineBreaker::canBreakBetween(allGlyphs[i - 1].unichar, glyph.unichar);
#else
          vg.canBreakBefore =
              LineBreaker::canBreakBetween(allGlyphs[i - 1].unichar, glyph.unichar);
#endif
        }
        vgList.push_back(std::move(vg));
      }
    }

    // Phase 2: Split into columns using break marks.
    std::vector<ColumnInfo> columns = {};
    columns.emplace_back();
    auto* currentColumn = &columns.back();
    float currentColumnHeight = 0;
    float boxHeight = textBox->size.height;
    bool doWrap = textBox->wordWrap && boxHeight > 0;
    size_t lastBreakIndex = 0;

    for (size_t i = 0; i < vgList.size(); i++) {
      auto& vg = vgList[i];

      if (vg.glyphs.front().unichar == '\n') {
        // Remove trailing letterSpacing from the last glyph in the column.
        if (!currentColumn->glyphs.empty()) {
          auto& lastVG = currentColumn->glyphs.back();
          float ls = (lastVG.glyphs.front().sourceText != nullptr)
                         ? lastVG.glyphs.front().sourceText->letterSpacing
                         : 0;
          if (ls != 0) {
            lastVG.height -= ls;
          }
        }
        // Trim trailing whitespace from current column.
        while (!currentColumn->glyphs.empty() &&
               LineBreaker::isWhitespace(currentColumn->glyphs.back().glyphs.front().unichar)) {
          currentColumn->glyphs.pop_back();
        }
        FinishColumn(currentColumn, textBox->lineHeight);
        columns.emplace_back();
        currentColumn = &columns.back();
        currentColumnHeight = 0;
        lastBreakIndex = 0;
        continue;
      }

      if (vg.canBreakBefore) {
        lastBreakIndex = currentColumn->glyphs.size();
      }

      if (doWrap && !currentColumn->glyphs.empty() &&
          currentColumnHeight + vg.height > boxHeight) {
        if (lastBreakIndex > 0) {
          std::vector<VerticalGlyphInfo> overflow(currentColumn->glyphs.begin() +
                                                      static_cast<ptrdiff_t>(lastBreakIndex),
                                                  currentColumn->glyphs.end());
          currentColumn->glyphs.resize(lastBreakIndex);
          // Remove trailing letterSpacing from the last glyph before the break.
          if (!currentColumn->glyphs.empty()) {
            auto& lastVG = currentColumn->glyphs.back();
            float ls = (lastVG.glyphs.front().sourceText != nullptr)
                           ? lastVG.glyphs.front().sourceText->letterSpacing
                           : 0;
            if (ls != 0) {
              lastVG.height -= ls;
            }
          }
          // Trim trailing whitespace from current column.
          while (!currentColumn->glyphs.empty() &&
                 LineBreaker::isWhitespace(currentColumn->glyphs.back().glyphs.front().unichar)) {
            currentColumn->glyphs.pop_back();
          }
          FinishColumn(currentColumn, textBox->lineHeight);
          columns.emplace_back();
          currentColumn = &columns.back();
          // Skip leading whitespace in overflow.
          size_t skipCount = 0;
          while (skipCount < overflow.size() &&
                 LineBreaker::isWhitespace(overflow[skipCount].glyphs.front().unichar)) {
            skipCount++;
          }
          for (size_t j = skipCount; j < overflow.size(); j++) {
            if (j == skipCount) {
              overflow[j].canBreakBefore = false;
            }
            currentColumn->glyphs.push_back(std::move(overflow[j]));
          }
          currentColumnHeight = 0;
          for (auto& g : currentColumn->glyphs) {
            currentColumnHeight += g.height;
          }
        } else {
          // Remove trailing letterSpacing from the last glyph.
          if (!currentColumn->glyphs.empty()) {
            auto& lastVG = currentColumn->glyphs.back();
            float ls = (lastVG.glyphs.front().sourceText != nullptr)
                           ? lastVG.glyphs.front().sourceText->letterSpacing
                           : 0;
            if (ls != 0) {
              lastVG.height -= ls;
            }
          }
          FinishColumn(currentColumn, textBox->lineHeight);
          columns.emplace_back();
          currentColumn = &columns.back();
          currentColumnHeight = 0;
        }
        lastBreakIndex = 0;
      }

      currentColumnHeight += vg.height;
      currentColumn->glyphs.push_back(std::move(vg));
    }

    // Remove trailing letterSpacing from the last glyph of the last column.
    if (!currentColumn->glyphs.empty()) {
      auto& lastVG = currentColumn->glyphs.back();
      float ls =
          (lastVG.glyphs.front().sourceText != nullptr) ? lastVG.glyphs.front().sourceText->letterSpacing : 0;
      if (ls != 0) {
        lastVG.height -= ls;
      }
    }

    FinishColumn(currentColumn, textBox->lineHeight);

#ifdef PAG_USE_PAGX_LAYOUT
    // Apply punctuation squash to all columns.
    ApplyPunctuationSquashToColumns(columns);
#endif

    return columns;
  }

#ifdef PAG_USE_PAGX_LAYOUT
  static void ApplyPunctuationSquashToColumns(std::vector<ColumnInfo>& columns) {
    for (auto& column : columns) {
      if (column.glyphs.empty()) {
        continue;
      }

      auto glyphCount = column.glyphs.size();
      // In vertical layout, "leading" = top (before glyph face), "trailing" = bottom (after glyph
      // face). Opening punctuation has leading (top) whitespace, Closing has trailing (bottom).
      // Only apply squash to upright glyphs. Rotated glyphs have their whitespace axis changed after
      // rotation, so horizontal punctuation squash rules do not apply.
      std::vector<float> leadingSquash(glyphCount, 0);
      std::vector<float> trailingSquash(glyphCount, 0);

      auto isSquashable = [](const VerticalGlyphInfo& vg) -> bool {
        return vg.orientation == VerticalOrientation::Upright;
      };

      // Column-start squash: remove leading whitespace of the first non-whitespace glyph.
      for (size_t i = 0; i < glyphCount; i++) {
        if (LineBreaker::isWhitespace(column.glyphs[i].glyphs.front().unichar)) {
          continue;
        }
        if (!isSquashable(column.glyphs[i])) {
          break;
        }
        float fraction =
            PunctuationSquash::GetLineStartSquash(column.glyphs[i].glyphs.front().unichar);
        leadingSquash[i] = column.glyphs[i].height * fraction;
        break;
      }

      // Column-end squash: remove trailing whitespace of the last non-whitespace glyph.
      for (int i = static_cast<int>(glyphCount) - 1; i >= 0; i--) {
        if (LineBreaker::isWhitespace(column.glyphs[i].glyphs.front().unichar)) {
          continue;
        }
        if (!isSquashable(column.glyphs[i])) {
          break;
        }
        float fraction =
            PunctuationSquash::GetLineEndSquash(column.glyphs[i].glyphs.front().unichar);
        trailingSquash[i] = column.glyphs[i].height * fraction;
        break;
      }

      // Adjacent punctuation squash (only between upright glyphs).
      for (size_t i = 0; i + 1 < glyphCount; i++) {
        if (!isSquashable(column.glyphs[i]) || !isSquashable(column.glyphs[i + 1])) {
          continue;
        }
        auto result = PunctuationSquash::GetAdjacentSquash(
            column.glyphs[i].glyphs.front().unichar,
            column.glyphs[i + 1].glyphs.front().unichar);
        if (result.prevSquash > 0) {
          float amount = column.glyphs[i].height * result.prevSquash;
          if (amount > trailingSquash[i]) {
            trailingSquash[i] = amount;
          }
        }
        if (result.nextSquash > 0) {
          float amount = column.glyphs[i + 1].height * result.nextSquash;
          if (amount > leadingSquash[i + 1]) {
            leadingSquash[i + 1] = amount;
          }
        }
      }

      // Apply squash to glyph heights and record leadingSquash for position adjustment.
      float totalHeight = 0;
      for (size_t i = 0; i < glyphCount; i++) {
        column.glyphs[i].leadingSquash = leadingSquash[i];
        column.glyphs[i].height -= leadingSquash[i] + trailingSquash[i];
        totalHeight += column.glyphs[i].height;
      }
      column.height = totalHeight;
    }
  }
#endif

  void buildTextBlobVertical(const TextBox* textBox, const std::vector<ColumnInfo>& columns) {
    if (columns.empty()) {
      return;
    }

    float boxWidth = textBox->size.width;
    float boxHeight = textBox->size.height;

    // Calculate total width: all columns use maxColumnWidth (line-box model).
    float totalWidth = 0;
    for (size_t i = 0; i < columns.size(); i++) {
      totalWidth += columns[i].maxColumnWidth;
    }

    // ParagraphAlign controls the block-flow direction (horizontal in vertical mode).
    // Columns go right-to-left, so Near = right-aligned, Far = left-aligned.
    // xStart is where the right edge of the first column starts.
    float xStart = textBox->position.x;
    if (boxWidth > 0) {
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Near:
          xStart += boxWidth;
          break;
        case ParagraphAlign::Middle:
          xStart += (boxWidth + totalWidth) / 2;
          break;
        case ParagraphAlign::Far:
          xStart += totalWidth;
          break;
      }
    } else {
      // boxWidth is 0: no boundary, align relative to position.
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Near:
          xStart += totalWidth;
          break;
        case ParagraphAlign::Middle:
          xStart += totalWidth / 2;
          break;
        case ParagraphAlign::Far:
          break;
      }
    }

    // Positioned glyph for vertical layout that can use either Point or RSXform.
    struct VerticalPositionedGlyph {
      tgfx::GlyphID glyphID = 0;
      tgfx::Font font = {};
      bool useRSXform = false;
      tgfx::Point position = {};
      tgfx::RSXform xform = {};
    };
    std::unordered_map<Text*, std::vector<VerticalPositionedGlyph>> textGlyphs = {};

    bool overflowHidden = textBox->overflow == Overflow::Hidden;
    float boxLeft = textBox->position.x;
    float columnX = xStart;

    for (size_t colIdx = 0; colIdx < columns.size(); colIdx++) {
      auto& column = columns[colIdx];
      // Move left by this column's allocated width.
      float allocatedWidth = column.maxColumnWidth;
      columnX -= allocatedWidth;

      // Skip columns that overflow beyond the left edge of the box.
      if (overflowHidden && boxWidth > 0 && columnX < boxLeft) {
        break;
      }

      // Center of this column for centering upright glyphs.
      float columnCenterX = columnX + allocatedWidth / 2;

      // TextAlign controls the inline direction (vertical in vertical mode).
      // Compute per-column vertical offset based on TextAlign.
      float inlineOffset = 0;
      float justifyGap = 0;
      if (boxHeight > 0) {
        switch (textBox->textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            inlineOffset = (boxHeight - column.height) / 2;
            break;
          case TextAlign::End:
            inlineOffset = boxHeight - column.height;
            break;
          case TextAlign::Justify: {
            // Justify: distribute extra space evenly at word boundaries. Last column uses Start.
            if (colIdx < columns.size() - 1) {
              size_t breakCount = 0;
              for (size_t gi = 0; gi < column.glyphs.size(); gi++) {
                if (column.glyphs[gi].canBreakBefore) {
                  breakCount++;
                }
              }
              if (breakCount > 0) {
                float extraSpace = boxHeight - column.height;
                justifyGap = extraSpace / static_cast<float>(breakCount);
              }
            }
            break;
          }
        }
      } else {
        switch (textBox->textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            inlineOffset = -column.height / 2;
            break;
          case TextAlign::End:
            inlineOffset = -column.height;
            break;
          case TextAlign::Justify:
            break;
        }
      }

      float currentY = textBox->position.y + inlineOffset;

      for (size_t glyphIdx = 0; glyphIdx < column.glyphs.size(); glyphIdx++) {
        auto& vg = column.glyphs[glyphIdx];

        // Skip tab glyphs: they only occupy space, not rendered.
        if (vg.glyphs.front().unichar == '\t') {
          currentY += vg.height;
          continue;
        }

        if (vg.orientation == VerticalOrientation::Rotated && vg.glyphs.size() > 1) {
          // Rotated group (tate-chu-yoko): render all glyphs as a horizontal run, rotated 90° CW.
          float localX = 0;
          float squashOffset = vg.leadingSquash;
          for (auto& g : vg.glyphs) {
            VerticalPositionedGlyph vpg = {};
            vpg.glyphID = g.glyphID;
            vpg.font = g.font;
            vpg.useRSXform = true;
            // RSXform::Make(0, 1, tx, ty) rotates 90° CW.
            // After rotation, the glyph's vertical extent (ascent+descent) becomes horizontal width.
            // Center each glyph horizontally in the column using its own ascent/descent.
            float absAscent = fabsf(g.ascent);
            float tx = columnCenterX - (absAscent - g.descent) / 2;
            float ty = currentY - squashOffset + localX;
            vpg.xform = tgfx::RSXform::Make(0, 1, tx, ty);
            // Use horizontal advance from font metrics for positioning within the group.
            localX += g.font.getAdvance(g.glyphID);
            textGlyphs[g.sourceText].push_back(vpg);
          }
        } else if (vg.orientation == VerticalOrientation::Rotated) {
          // Single rotated glyph: rotate 90 degrees CW using RSXform (scos=0, ssin=1).
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = true;
          float absAscent = fabsf(g.ascent);
          float tx = columnCenterX - (absAscent - g.descent) / 2;
          float ty = currentY - vg.leadingSquash;
          vpg.xform = tgfx::RSXform::Make(0, 1, tx, ty);
          textGlyphs[g.sourceText].push_back(vpg);
#ifdef PAG_USE_HARFBUZZ
        } else {
          // With HarfBuzz vert shaping, use getVerticalOffset() for all upright glyphs.
          // If the font has vert/vrt2 features, HarfBuzz already substituted the glyph.
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          auto offset = g.font.getVerticalOffset(g.glyphID);
          float glyphX = columnCenterX + offset.x;
          float glyphY = currentY - vg.leadingSquash + offset.y;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          textGlyphs[g.sourceText].push_back(vpg);
        }
#else
        } else if (VerticalTextUtils::needsPunctuationOffset(vg.glyphs.front().unichar)) {
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          float dx = 0;
          float dy = 0;
          VerticalTextUtils::getPunctuationOffset(g.fontSize, &dx, &dy);
          float glyphX = columnCenterX - g.advance / 2 + dx;
          float glyphY = currentY + fabsf(g.ascent) + dy;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          textGlyphs[g.sourceText].push_back(vpg);
        } else {
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          auto offset = g.font.getVerticalOffset(g.glyphID);
          float glyphX = columnCenterX + offset.x;
          float glyphY = currentY + offset.y;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          textGlyphs[g.sourceText].push_back(vpg);
        }
#endif

        currentY += vg.height;
        if (justifyGap != 0 && glyphIdx + 1 < column.glyphs.size() &&
            column.glyphs[glyphIdx + 1].canBreakBefore) {
          currentY += justifyGap;
        }
      }
    }

    // Build TextBlob for each Text element with inverse-transform compensation.
    for (auto& [text, glyphs] : textGlyphs) {
      if (glyphs.empty() || text == nullptr) {
        continue;
      }

      float compensateX = text->position.x;
      float compensateY = text->position.y;

      tgfx::TextBlobBuilder builder = {};

      // Separate into Point-positioned and RSXform-positioned runs.
      // We need to batch consecutive glyphs of the same font and positioning mode.
      size_t runStart = 0;
      while (runStart < glyphs.size()) {
        auto& runFont = glyphs[runStart].font;
        bool runUseRSXform = glyphs[runStart].useRSXform;
        size_t runEnd = runStart + 1;
        while (runEnd < glyphs.size() &&
               glyphs[runEnd].font == runFont &&
               glyphs[runEnd].useRSXform == runUseRSXform) {
          runEnd++;
        }
        size_t count = runEnd - runStart;

        if (runUseRSXform) {
          auto& buffer = builder.allocRunRSXform(runFont, count);
          auto* xforms = reinterpret_cast<tgfx::RSXform*>(buffer.positions);
          for (size_t j = 0; j < count; j++) {
            buffer.glyphs[j] = glyphs[runStart + j].glyphID;
            auto xform = glyphs[runStart + j].xform;
            xform.tx -= compensateX;
            xform.ty -= compensateY;
            xforms[j] = xform;
          }
        } else {
          auto& buffer = builder.allocRunPos(runFont, count);
          auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
          for (size_t j = 0; j < count; j++) {
            buffer.glyphs[j] = glyphs[runStart + j].glyphID;
            positions[j] = tgfx::Point::Make(glyphs[runStart + j].position.x - compensateX,
                                              glyphs[runStart + j].position.y - compensateY);
          }
        }
        runStart = runEnd;
      }

      auto textBlob = builder.build();
      if (textBlob != nullptr) {
        ShapedText shapedText = {};
        shapedText.textBlob = textBlob;
        StoreShapedText(text, std::move(shapedText));
      }
    }
  }

  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle) {
    // First, try exact match from registered typefaces
    if (!fontFamily.empty()) {
      FontKey key = {};
      key.family = fontFamily;
      key.style = fontStyle.empty() ? "Regular" : fontStyle;
      auto it = textLayout->registeredTypefaces.find(key);
      if (it != textLayout->registeredTypefaces.end()) {
        return it->second;
      }

      // Try matching family only (any style)
      std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
      FontKey bestKey = {};
      bool hasBest = false;
      for (const auto& pair : textLayout->registeredTypefaces) {
        if (pair.first.family != fontFamily) {
          continue;
        }
        if (!hasBest) {
          bestKey = pair.first;
          bestTypeface = pair.second;
          hasBest = true;
          continue;
        }
        if (IsPreferredFontKey(pair.first, bestKey)) {
          bestKey = pair.first;
          bestTypeface = pair.second;
        }
      }
      if (hasBest) {
        return bestTypeface;
      }
    }

    // Then, try fallback typefaces by family name
    if (!fontFamily.empty()) {
      for (const auto& tf : textLayout->fallbackTypefaces) {
        if (tf != nullptr && tf->fontFamily() == fontFamily) {
          return tf;
        }
      }
    }

    // Use first fallback typeface
    if (!textLayout->fallbackTypefaces.empty()) {
      return textLayout->fallbackTypefaces[0];
    }

    // Last resort: try system font
    if (!fontFamily.empty()) {
      return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    }

    return nullptr;
  }

  const TextLayout* textLayout = nullptr;
  PAGXDocument* document = nullptr;
  ShapedTextMap result = {};
  std::vector<Text*> textOrder = {};
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> fontCache = {};
};

TextLayoutResult TextLayout::layout(PAGXDocument* document) {
  TextLayoutContext context(this, document);
  return context.run();
}

}  // namespace pagx
