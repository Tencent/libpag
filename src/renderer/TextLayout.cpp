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

  // A group of glyphs with the same vertical treatment in vertical text layout.
  struct VerticalGlyphGroup {
    std::vector<GlyphInfo> glyphs = {};
    VerticalOrientation orientation = VerticalOrientation::Upright;
    float height = 0;
    float width = 0;
  };

  // Column of vertical text, containing groups of glyphs arranged top to bottom.
  struct ColumnInfo {
    std::vector<VerticalGlyphGroup> groups = {};
    float height = 0;
    float maxWidth = 0;
    float maxFontSize = 0;
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

    processLayerContents(layer->contents);

    for (auto* child : layer->children) {
      processLayer(child);
    }
  }

  void processLayerContents(const std::vector<Element*>& contents) {
    const TextBox* textBox = nullptr;
    std::vector<Text*> textElements = {};

    // First pass: find TextBox and collect direct Text elements
    for (auto* element : contents) {
      if (element->nodeType() == NodeType::TextBox) {
        textBox = static_cast<const TextBox*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      }
    }

    // Rich text mode: when TextBox exists and there are Groups containing Text,
    // collect Text elements from Groups for unified typography.
    if (textBox != nullptr && textElements.empty()) {
      for (auto* element : contents) {
        if (element->nodeType() == NodeType::Group) {
          auto* group = static_cast<Group*>(element);
          for (auto* child : group->elements) {
            if (child->nodeType() == NodeType::Text) {
              textElements.push_back(static_cast<Text*>(child));
            }
          }
        }
      }
      if (!textElements.empty()) {
        processRichTextWithLayout(textElements, textBox);
        return;
      }
    }

    // Normal mode: process Groups independently
    for (auto* element : contents) {
      if (element->nodeType() == NodeType::Group) {
        processGroup(static_cast<Group*>(element));
      }
    }

    if (!textElements.empty()) {
      processTextWithLayout(textElements, textBox);
    }
  }

  void processGroup(Group* group) {
    if (group == nullptr) {
      return;
    }

    const TextBox* textBox = nullptr;
    std::vector<Text*> textElements = {};

    for (auto* element : group->elements) {
      if (element->nodeType() == NodeType::TextBox) {
        textBox = static_cast<const TextBox*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      } else if (element->nodeType() == NodeType::Group) {
        processGroup(static_cast<Group*>(element));
      }
    }

    if (!textElements.empty()) {
      processTextWithLayout(textElements, textBox);
    }
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

  void processRichTextWithLayout(std::vector<Text*>& textElements,
                                   const TextBox* textBox) {
    if (tryUseEmbeddedGlyphRuns(textElements)) {
      return;
    }

    // Shape each Text element and concatenate allGlyphs for unified layout.
    std::vector<GlyphInfo> allGlyphs = {};
    float totalWidth = 0;

    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;
      if (!text->text.empty()) {
        shapeText(text, info);
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

  void processTextWithLayout(std::vector<Text*>& textElements, const TextBox* textBox) {
    // If no TextBox or all have embedded data, process each Text individually
    if (textBox == nullptr) {
      for (auto* text : textElements) {
        if (!text->glyphRuns.empty()) {
          auto shapedText = buildShapedTextFromEmbeddedGlyphRuns(text);
          StoreShapedText(text, std::move(shapedText));
        } else {
          processTextWithoutLayout(text);
        }
      }
      return;
    }

    if (tryUseEmbeddedGlyphRuns(textElements)) {
      return;
    }

    // With TextBox: shape each Text, then layout with box.
    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;
      if (!text->text.empty()) {
        shapeText(text, info);
      }
      if (info.allGlyphs.empty()) {
        continue;
      }

      if (textBox->writingMode == WritingMode::Vertical) {
        auto columns = layoutColumns(info.allGlyphs, textBox);
        buildTextBlobVertical(textBox, columns);
      } else {
        auto lines = layoutLines(info.allGlyphs, textBox);
        buildTextBlobWithLayout(textBox, lines);
      }
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

    // Build TextBlob without layout offset
    tgfx::TextBlobBuilder builder = {};

    for (auto& run : info.runs) {
      if (run.glyphIDs.empty()) {
        continue;
      }

      if (run.canUseDefaultMode) {
        // Default mode: use font's advance values to position glyphs, starting at run.startX
        auto& buffer = builder.allocRun(run.font, run.glyphIDs.size(), run.startX, 0);
        memcpy(buffer.glyphs, run.glyphIDs.data(), run.glyphIDs.size() * sizeof(tgfx::GlyphID));
      } else {
        auto& buffer = builder.allocRunPosH(run.font, run.glyphIDs.size(), 0);
        memcpy(buffer.glyphs, run.glyphIDs.data(), run.glyphIDs.size() * sizeof(tgfx::GlyphID));
        memcpy(buffer.positions, run.xPositions.data(), run.xPositions.size() * sizeof(float));
      }
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

  void shapeText(Text* text, ShapedInfo& info) {
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
    bool doWrap = textBox->wordWrap;

    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];

      if (glyph.unichar == '\n') {
        FinishLine(currentLine, textBox->lineHeight, glyph.fontLineHeight);
        lines.emplace_back();
        currentLine = &lines.back();
        currentLineWidth = 0;
        lastBreakIndex = -1;
        // Add the \n glyph to the next line so its font metrics participate in line metrics
        // calculation for the next line.
        GlyphInfo newlineGlyph = glyph;
        newlineGlyph.xPosition = 0;
        newlineGlyph.advance = 0;
        currentLine->glyphs.push_back(newlineGlyph);
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
        if (LineBreaker::canBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
        }
      }
    }

    FinishLine(currentLine, textBox->lineHeight, 0.0f);
    return lines;
  }

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
      switch (textBox->verticalAlign) {
        case VerticalAlign::Baseline:
        case VerticalAlign::Top:
          yOffset = 0;
          break;
        case VerticalAlign::Center:
          yOffset = (boxHeight - totalHeight) / 2;
          break;
        case VerticalAlign::Bottom:
          yOffset = boxHeight - totalHeight;
          break;
      }
    } else {
      switch (textBox->verticalAlign) {
        case VerticalAlign::Baseline:
        case VerticalAlign::Top:
          yOffset = 0;
          break;
        case VerticalAlign::Center:
          yOffset = -totalHeight / 2;
          break;
        case VerticalAlign::Bottom:
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

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
      auto& line = lines[lineIdx];

      if (textBox->verticalAlign == VerticalAlign::Baseline && lineIdx == 0) {
        // Baseline mode: position.y is the first line's baseline, no ascent offset.
        if (line.glyphs.empty()) {
          baselineY = textBox->position.y + yOffset + line.maxLineHeight;
        } else {
          baselineY = textBox->position.y + yOffset;
        }
      } else {
        if (line.glyphs.empty()) {
          baselineY = textBox->position.y + roundf(
              (relativeTop + line.maxLineHeight) * line.roundingRatio + yOffset);
        } else {
          // Half-leading line-box model:
          // relativeBaseline = (relativeTop + halfLeading + ascent) * roundingRatio
          // baselineY = position.y + roundf(relativeBaseline + yOffset)
          float halfLeading = (line.maxLineHeight - line.metricsHeight) / 2;
          float relativeBaseline =
              (relativeTop + halfLeading + line.maxAscent) * line.roundingRatio;
          baselineY = textBox->position.y + roundf(relativeBaseline + yOffset);
        }
      }
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
          case TextAlign::Justify:
            break;
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

      for (auto& g : line.glyphs) {
        // Skip newline glyphs: they only participate in metrics, not rendering.
        if (g.unichar == '\n') {
          continue;
        }
        PositionedGlyph pg = {};
        pg.glyphID = g.glyphID;
        pg.font = g.font;
        pg.x = g.xPosition + xOffset;
        pg.y = baselineY;
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
    float maxWidth = 0;
    float maxFontSize = 0;
    for (auto& group : column->groups) {
      height += group.height;
      if (group.width > maxWidth) {
        maxWidth = group.width;
      }
      for (auto& g : group.glyphs) {
        if (g.fontSize > maxFontSize) {
          maxFontSize = g.fontSize;
        }
      }
    }
    column->height = height;
    column->maxWidth = maxWidth;
    column->maxFontSize = maxFontSize;
    column->maxColumnWidth = (lineHeight > 0) ? lineHeight : maxFontSize;
  }

  std::vector<ColumnInfo> layoutColumns(const std::vector<GlyphInfo>& allGlyphs,
                                        const TextBox* textBox) {
    std::vector<ColumnInfo> columns = {};
    columns.emplace_back();
    auto* currentColumn = &columns.back();
    float currentColumnHeight = 0;
    float boxHeight = textBox->size.height;
    bool doWrap = textBox->wordWrap;

    size_t i = 0;
    while (i < allGlyphs.size()) {
      auto& glyph = allGlyphs[i];

      if (glyph.unichar == '\n') {
        FinishColumn(currentColumn, textBox->lineHeight);
        columns.emplace_back();
        currentColumn = &columns.back();
        currentColumnHeight = 0;
        i++;
        continue;
      }

      auto orientation = VerticalTextUtils::getOrientation(glyph.unichar);

      if (orientation == VerticalOrientation::Rotated &&
          VerticalTextUtils::isRotatedGroupChar(glyph.unichar)) {
        // Collect consecutive rotated-group characters into a single group.
        VerticalGlyphGroup group = {};
        group.orientation = VerticalOrientation::Rotated;
        float groupHorizontalWidth = 0;
        float groupMaxFontSize = 0;
        while (i < allGlyphs.size() &&
               allGlyphs[i].unichar != '\n' &&
               VerticalTextUtils::isRotatedGroupChar(allGlyphs[i].unichar)) {
          group.glyphs.push_back(allGlyphs[i]);
          groupHorizontalWidth += allGlyphs[i].advance;
          if (allGlyphs[i].fontSize > groupMaxFontSize) {
            groupMaxFontSize = allGlyphs[i].fontSize;
          }
          i++;
        }
        // Rotated group: height = max font size, width = horizontal width sum.
        group.height = groupMaxFontSize;
        group.width = groupHorizontalWidth;

        // Check wrap before adding.
        if (doWrap && !currentColumn->groups.empty() &&
            currentColumnHeight + group.height > boxHeight) {
          FinishColumn(currentColumn, textBox->lineHeight);
          columns.emplace_back();
          currentColumn = &columns.back();
          currentColumnHeight = 0;
        }
        currentColumnHeight += group.height;
        currentColumn->groups.push_back(std::move(group));
      } else {
        // Single upright glyph (CJK, or non-groupable rotated character).
        float verticalAdvance = glyph.font.getAdvance(glyph.glyphID, true);
        if (verticalAdvance <= 0) {
          verticalAdvance = glyph.fontSize;
        }
        float glyphWidth = glyph.advance;
        if (glyphWidth <= 0) {
          glyphWidth = glyph.fontSize;
        }

        // Check wrap before adding.
        if (doWrap && !currentColumn->groups.empty() &&
            currentColumnHeight + verticalAdvance > boxHeight) {
          FinishColumn(currentColumn, textBox->lineHeight);
          columns.emplace_back();
          currentColumn = &columns.back();
          currentColumnHeight = 0;
        }

        VerticalGlyphGroup group = {};
        group.orientation = orientation;
        group.glyphs.push_back(glyph);
        group.height = verticalAdvance;
        group.width = (orientation == VerticalOrientation::Upright) ? glyph.fontSize : glyphWidth;
        currentColumnHeight += verticalAdvance;
        currentColumn->groups.push_back(std::move(group));
        i++;
      }
    }

    FinishColumn(currentColumn, textBox->lineHeight);
    return columns;
  }

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

    // Calculate max column height for vertical alignment.
    float maxColumnHeight = 0;
    for (auto& col : columns) {
      if (col.height > maxColumnHeight) {
        maxColumnHeight = col.height;
      }
    }

    // Horizontal offset: columns go right-to-left, so Start = right-aligned.
    // xStart is where the right edge of the first column starts.
    float xStart = textBox->position.x;
    if (boxWidth > 0) {
      switch (textBox->textAlign) {
        case TextAlign::Start:
          xStart += boxWidth;
          break;
        case TextAlign::Center:
          xStart += (boxWidth + totalWidth) / 2;
          break;
        case TextAlign::End:
          xStart += totalWidth;
          break;
        case TextAlign::Justify:
          xStart += boxWidth;
          break;
      }
    } else {
      // Anchor mode: position is anchor point.
      switch (textBox->textAlign) {
        case TextAlign::Start:
          xStart += totalWidth;
          break;
        case TextAlign::Center:
          xStart += totalWidth / 2;
          break;
        case TextAlign::End:
          break;
        case TextAlign::Justify:
          xStart += totalWidth;
          break;
      }
    }

    // Vertical alignment offset.
    float yBase = textBox->position.y;
    if (boxHeight > 0) {
      switch (textBox->verticalAlign) {
        case VerticalAlign::Baseline:
        case VerticalAlign::Top:
          break;
        case VerticalAlign::Center:
          yBase += (boxHeight - maxColumnHeight) / 2;
          break;
        case VerticalAlign::Bottom:
          yBase += boxHeight - maxColumnHeight;
          break;
      }
    } else {
      switch (textBox->verticalAlign) {
        case VerticalAlign::Baseline:
        case VerticalAlign::Top:
          break;
        case VerticalAlign::Center:
          yBase -= maxColumnHeight / 2;
          break;
        case VerticalAlign::Bottom:
          yBase -= maxColumnHeight;
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

      float currentY = yBase;

      for (auto& group : column.groups) {
        if (group.glyphs.empty()) {
          currentY += group.height;
          continue;
        }

        if (group.orientation == VerticalOrientation::Upright) {
          // Single upright glyph.
          auto& g = group.glyphs[0];
          auto transform = VerticalTextUtils::getPunctuationTransform(g.unichar);

          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;

          if (transform == PunctuationTransform::Rotate90) {
            // Rotate 90° CW using RSXform: scos=0, ssin=1.
            vpg.useRSXform = true;
            // The glyph is drawn with its horizontal origin, then rotated.
            // After 90° CW rotation around origin: (x,y) -> (y, -x).
            // We translate to place it in the column center at currentY.
            float tx = columnCenterX + g.fontSize / 2;
            float ty = currentY;
            vpg.xform = tgfx::RSXform::Make(0, 1, tx, ty);
          } else if (transform == PunctuationTransform::Offset) {
            vpg.useRSXform = false;
            float dx = 0;
            float dy = 0;
            VerticalTextUtils::getPunctuationOffset(g.unichar, g.fontSize, &dx, &dy);
            // Position upright glyph centered in column + offset.
            float glyphX = columnCenterX - g.advance / 2 + dx;
            float glyphY = currentY + fabsf(g.ascent) + dy;
            vpg.position = tgfx::Point::Make(glyphX, glyphY);
          } else {
            // Normal upright: center horizontally in column, baseline at ascent offset.
            vpg.useRSXform = false;
            auto offset = g.font.getVerticalOffset(g.glyphID);
            float glyphX = columnCenterX + offset.x;
            float glyphY = currentY + group.height / 2 + offset.y;
            vpg.position = tgfx::Point::Make(glyphX, glyphY);
          }

          textGlyphs[g.sourceText].push_back(vpg);
        } else {
          // Rotated group: all glyphs rotated 90° CW as a unit.
          // Horizontal layout within the group, then the whole thing rotates.
          // After rotation, the group's horizontal axis becomes vertical.
          // The group is centered in the column.
          // Position the group centered horizontally in the column.
          // After 90° CW rotation, the horizontal text becomes vertical:
          // - original x-axis → y-axis (downward)
          // - original y-axis → x-axis (rightward, but negated)
          float groupCenterX = columnCenterX;
          float groupTopY = currentY;

          float localX = 0;
          for (auto& g : group.glyphs) {
            VerticalPositionedGlyph vpg = {};
            vpg.glyphID = g.glyphID;
            vpg.font = g.font;
            vpg.useRSXform = true;
            // RSXform for 90° CW: scos=0, ssin=1.
            // Matrix: | 0  -1  tx |
            //         | 1   0  ty |
            // The glyph at local position (localX, baseline=0) maps to:
            //   finalX = -0 + tx = tx
            //   finalY = localX + ty
            // We want the group centered in the column width.
            float tx = groupCenterX + g.fontSize / 2;
            float ty = groupTopY + localX;
            vpg.xform = tgfx::RSXform::Make(0, 1, tx, ty);
            textGlyphs[g.sourceText].push_back(vpg);
            localX += g.advance;
          }
        }
        currentY += group.height;
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

  float calculateLayoutOffset(const TextBox* layout, float textWidth) {
    switch (layout->textAlign) {
      case TextAlign::Start:
        return 0;
      case TextAlign::Center:
        return -0.5f * textWidth;
      case TextAlign::End:
        return -textWidth;
      case TextAlign::Justify:
        // TODO: Justify requires more complex handling
        return 0;
    }
    return 0;
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
