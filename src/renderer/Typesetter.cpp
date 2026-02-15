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

#include "Typesetter.h"
#include <cmath>
#include "LineBreaker.h"
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

void Typesetter::registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  FontKey key = {};
  key.family = typeface->fontFamily();
  key.style = typeface->fontStyle();
  registeredTypefaces[key] = std::move(typeface);
}

void Typesetter::setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) {
  fallbackTypefaces = std::move(typefaces);
}

// Build context that maintains state during text typesetting
class TypesetterContext {
 public:
  TypesetterContext(const Typesetter* typesetter, PAGXDocument* document)
      : typesetter(typesetter), document(document) {
  }

  TypesetterResult run() {
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

    TypesetterResult output = {};
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
    Text* sourceText = nullptr;
  };

  struct LineInfo {
    std::vector<GlyphInfo> glyphs = {};
    float width = 0;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxLineHeight = 0;
  };

  // Shaped text information for a single Text element.
  struct ShapedInfo {
    Text* text = nullptr;
    std::vector<ShapedGlyphRun> runs = {};
    float totalWidth = 0;
    std::vector<GlyphInfo> allGlyphs = {};
  };

  using FontKey = Typesetter::FontKey;

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

    auto lines = layoutLines(allGlyphs, textBox);
    buildTextBlobWithLayout(textBox, lines);
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

      auto lines = layoutLines(info.allGlyphs, textBox);
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

      // Note: scales, rotations, skews are NOT processed here.
      // These transform attributes should be handled by tgfx layer (similar to TextModifier).
      // Typesetter only computes position information for TextBlob.

      if (!run->positions.empty() && run->positions.size() >= count) {
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
    float currentX = 0;
    const std::string& content = text->text;
    bool hasLetterSpacing = !FloatNearlyEqual(text->letterSpacing, 0.0f);

    // Pre-build fallback font cache to avoid repeated construction in the inner loop.
    std::unordered_map<tgfx::Typeface*, tgfx::Font> fallbackFontCache = {};
    for (const auto& fallback : typesetter->fallbackTypefaces) {
      if (fallback != nullptr && fallback != primaryTypeface) {
        fallbackFontCache.emplace(fallback.get(), tgfx::Font(fallback, text->fontSize));
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

      // Handle newline
      if (unichar == '\n') {
        GlyphInfo gi = {};
        gi.unichar = '\n';
        gi.fontSize = text->fontSize;
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
        for (const auto& fallback : typesetter->fallbackTypefaces) {
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
    bool doWrap = textBox->wordWrap && boxWidth > 0;

    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];

      if (glyph.unichar == '\n') {
        FinishLine(currentLine, textBox->lineHeight, glyph.sourceText);
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
          FinishLine(currentLine, textBox->lineHeight, nullptr);
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
            FinishLine(currentLine, textBox->lineHeight, nullptr);
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

    FinishLine(currentLine, textBox->lineHeight, nullptr);
    return lines;
  }

  static void FinishLine(LineInfo* line, float lineHeightMultiplier, Text* newlineSource) {
    if (line->glyphs.empty()) {
      // Empty line from consecutive \n: use the newline source's fontSize for height.
      if (newlineSource != nullptr) {
        line->maxLineHeight = newlineSource->fontSize * lineHeightMultiplier;
      }
      return;
    }
    auto& lastGlyph = line->glyphs.back();
    line->width = lastGlyph.xPosition + lastGlyph.advance;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxFontSize = 0;
    for (auto& g : line->glyphs) {
      float absAscent = fabsf(g.ascent);
      if (absAscent > maxAscent) {
        maxAscent = absAscent;
      }
      if (g.descent > maxDescent) {
        maxDescent = g.descent;
      }
      if (g.fontSize > maxFontSize) {
        maxFontSize = g.fontSize;
      }
    }
    line->maxAscent = maxAscent;
    line->maxDescent = maxDescent;
    line->maxLineHeight = maxFontSize * lineHeightMultiplier;
  }

  void buildTextBlobWithLayout(const TextBox* textBox, const std::vector<LineInfo>& lines) {
    if (lines.empty()) {
      return;
    }

    float boxWidth = textBox->size.width;
    float boxHeight = textBox->size.height;

    // Calculate total height: first line uses ascent, subsequent lines use maxLineHeight.
    float totalHeight = 0;
    for (size_t i = 0; i < lines.size(); i++) {
      if (lines[i].glyphs.empty()) {
        totalHeight += lines[i].maxLineHeight;
        continue;
      }
      if (i == 0) {
        totalHeight += lines[i].maxAscent;
      } else {
        totalHeight += lines[i].maxLineHeight;
      }
    }
    // Add last line's descent if lineHeight doesn't cover it.
    auto& lastLine = lines.back();
    if (!lastLine.glyphs.empty()) {
      float lineMetricHeight = lastLine.maxAscent + lastLine.maxDescent;
      if (lastLine.maxLineHeight < lineMetricHeight) {
        totalHeight += lastLine.maxDescent;
      } else {
        totalHeight += lastLine.maxLineHeight - lastLine.maxAscent;
      }
    }

    // Vertical alignment offset.
    float yOffset = 0;
    if (boxHeight > 0) {
      switch (textBox->verticalAlign) {
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

    float baselineY = textBox->position.y + yOffset;

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
      auto& line = lines[lineIdx];

      if (lineIdx == 0) {
        baselineY += line.glyphs.empty() ? line.maxLineHeight : line.maxAscent;
      } else {
        baselineY += line.maxLineHeight;
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
        PositionedGlyph pg = {};
        pg.glyphID = g.glyphID;
        pg.font = g.font;
        pg.x = g.xPosition + xOffset;
        pg.y = baselineY;
        if (g.sourceText != nullptr) {
          pg.y += g.sourceText->baselineShift;
        }
        textGlyphs[g.sourceText].push_back(pg);
      }
    }

    // Build TextBlob for each Text element.
    for (auto& [text, glyphs] : textGlyphs) {
      if (glyphs.empty() || text == nullptr) {
        continue;
      }

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
          positions[j] = tgfx::Point::Make(glyphs[runStart + j].x, glyphs[runStart + j].y);
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
      auto it = typesetter->registeredTypefaces.find(key);
      if (it != typesetter->registeredTypefaces.end()) {
        return it->second;
      }

      // Try matching family only (any style)
      std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
      FontKey bestKey = {};
      bool hasBest = false;
      for (const auto& pair : typesetter->registeredTypefaces) {
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
      for (const auto& tf : typesetter->fallbackTypefaces) {
        if (tf != nullptr && tf->fontFamily() == fontFamily) {
          return tf;
        }
      }
    }

    // Use first fallback typeface
    if (!typesetter->fallbackTypefaces.empty()) {
      return typesetter->fallbackTypefaces[0];
    }

    // Last resort: try system font
    if (!fontFamily.empty()) {
      return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    }

    return nullptr;
  }

  const Typesetter* typesetter = nullptr;
  PAGXDocument* document = nullptr;
  ShapedTextMap result = {};
  std::vector<Text*> textOrder = {};
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> fontCache = {};
};

TypesetterResult Typesetter::shape(PAGXDocument* document) {
  TypesetterContext context(this, document);
  return context.run();
}

}  // namespace pagx
