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
#include "Base64.h"
#include "MathUtil.h"
#include "SVGPathParser.h"
#include "TGFXConverter.h"
#include "UTF8.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextLayout.h"
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

  // Shaped text information for a single Text element.
  struct ShapedInfo {
    Text* text = nullptr;
    std::vector<ShapedGlyphRun> runs = {};
    float totalWidth = 0;
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
    const TextLayout* textLayout = nullptr;
    std::vector<Text*> textElements = {};

    // First pass: find TextLayout and collect direct Text elements
    for (auto* element : contents) {
      if (element->nodeType() == NodeType::TextLayout) {
        textLayout = static_cast<const TextLayout*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      }
    }

    // Rich text mode: when TextLayout exists and there are Groups containing Text,
    // collect Text elements from Groups for unified typography.
    if (textLayout != nullptr && textElements.empty()) {
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
        processRichTextWithLayout(textElements, textLayout);
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
      processTextWithLayout(textElements, textLayout);
    }
  }

  void processGroup(Group* group) {
    if (group == nullptr) {
      return;
    }

    const TextLayout* textLayout = nullptr;
    std::vector<Text*> textElements = {};

    for (auto* element : group->elements) {
      if (element->nodeType() == NodeType::TextLayout) {
        textLayout = static_cast<const TextLayout*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      } else if (element->nodeType() == NodeType::Group) {
        processGroup(static_cast<Group*>(element));
      }
    }

    if (!textElements.empty()) {
      processTextWithLayout(textElements, textLayout);
    }
  }

  void processRichTextWithLayout(std::vector<Text*>& textElements,
                                   const TextLayout* textLayout) {
    // If all Text elements have embedded GlyphRun data, use them directly.
    bool allHaveEmbeddedData = true;
    for (auto* text : textElements) {
      if (text->glyphRuns.empty()) {
        allHaveEmbeddedData = false;
        break;
      }
    }
    if (allHaveEmbeddedData) {
      for (auto* text : textElements) {
        auto shapedText = buildShapedTextFromEmbeddedGlyphRuns(text);
        StoreShapedText(text, std::move(shapedText));
      }
      return;
    }

    // Shape each Text element individually, then concatenate them into a continuous paragraph.
    std::vector<ShapedInfo> shapedInfos = {};
    float totalWidth = 0;

    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;
      if (!text->text.empty()) {
        shapeText(text, info);
      }
      // Shift all glyph positions by the accumulated width so far
      for (auto& run : info.runs) {
        run.startX += totalWidth;
        for (auto& x : run.xPositions) {
          x += totalWidth;
        }
        run.canUseDefaultMode = false;
      }
      totalWidth += info.totalWidth;
      shapedInfos.push_back(std::move(info));
    }

    // Calculate alignment offset based on total concatenated width
    float alignOffset = calculateLayoutOffset(textLayout, totalWidth);
    float baseX = textLayout->position.x;
    float baseY = textLayout->position.y;

    // Build TextBlob for each Text element with correct offsets
    for (auto& info : shapedInfos) {
      if (info.runs.empty()) {
        continue;
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
          adjustedPositions.push_back(x + alignOffset + baseX);
        }

        auto& buffer = builder.allocRunPosH(run.font, run.glyphIDs.size(), baseY);
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
  }

  void processTextWithLayout(std::vector<Text*>& textElements, const TextLayout* textLayout) {
    // If TextLayout exists, check if ALL Text elements have embedded GlyphRun data.
    // If any Text is missing embedded data, we must re-typeset all of them together.
    bool allHaveEmbeddedData = true;
    if (textLayout != nullptr) {
      for (auto* text : textElements) {
        if (text->glyphRuns.empty()) {
          allHaveEmbeddedData = false;
          break;
        }
      }
    }

    // If no TextLayout or all have embedded data, process each Text individually
    if (textLayout == nullptr || allHaveEmbeddedData) {
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

    // TextLayout exists but some Text elements need re-typesetting.
    // Must re-typeset all Text elements together to apply layout correctly.
    std::vector<ShapedInfo> shapedInfos = {};

    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;

      if (!text->text.empty()) {
        shapeText(text, info);
      }

      shapedInfos.push_back(std::move(info));
    }

    // Apply TextLayout and create TextBlobs
    for (auto& info : shapedInfos) {
      if (info.runs.empty()) {
        continue;
      }

      float xOffset = calculateLayoutOffset(textLayout, info.totalWidth);
      float yOffset = 0;

      if (textLayout->position.x != 0 || textLayout->position.y != 0) {
        xOffset += textLayout->position.x - info.text->position.x;
        yOffset = textLayout->position.y - info.text->position.y;
      }

      // Build TextBlob with multiple runs
      tgfx::TextBlobBuilder builder = {};
      std::vector<float> adjustedPositions = {};

      for (auto& run : info.runs) {
        if (run.glyphIDs.empty()) {
          continue;
        }

        // Apply offsets to positions
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
            builder.addGlyph(codec, PointToTGFX(glyph->offset), glyph->advance);
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
        // TODO: handle multi-line text
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

      currentX += advance + text->letterSpacing;
    }

    // Remove the extra letterSpacing after the last glyph.
    if (hasLetterSpacing && currentX > 0) {
      currentX -= text->letterSpacing;
    }
    info.totalWidth = currentX;
    if (hasLetterSpacing && !info.runs.empty()) {
      info.totalWidth -= text->letterSpacing;
    }
  }

  float calculateLayoutOffset(const TextLayout* layout, float textWidth) {
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
