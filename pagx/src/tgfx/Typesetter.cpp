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

#include "pagx/Typesetter.h"
#include "Base64.h"
#include "MathUtil.h"
#include "SVGPathParser.h"
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

// Helper to convert pagx types to tgfx types.
static tgfx::Path ToTGFXPath(const PathData& pathData) {
  tgfx::Path path = {};
  pathData.forEach([&path](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        path.moveTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Line:
        path.lineTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Quad:
        path.quadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        break;
      case PathVerb::Cubic:
        path.cubicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        break;
      case PathVerb::Close:
        path.close();
        break;
    }
  });
  return path;
}

static tgfx::Point ToTGFXPoint(const Point& p) {
  return tgfx::Point::Make(p.x, p.y);
}

static std::shared_ptr<tgfx::Data> ToTGFXData(const std::shared_ptr<Data>& data) {
  if (data == nullptr) {
    return nullptr;
  }
  return tgfx::Data::MakeWithCopy(data->data(), data->size());
}

// Build context that maintains state during text typesetting
class TypesetterContext {
 public:
  TypesetterContext(const Typesetter* typesetter, PAGXDocument* document)
      : typesetter(typesetter), document(document) {
  }

  ShapedTextMap run() {
    if (document == nullptr) {
      return result;
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

    return std::move(result);
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

    for (auto* element : contents) {
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
          if (shapedText.textBlob != nullptr) {
            result[text] = std::move(shapedText);
          }
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

      for (auto& run : info.runs) {
        if (run.glyphIDs.empty()) {
          continue;
        }

        // Apply offsets to positions
        std::vector<float> adjustedPositions = {};
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
        result[info.text] = std::move(shapedText);
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
      result[text] = std::move(shapedText);
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
          auto path = ToTGFXPath(*glyph->path);
          if (glyph->offset.x != 0 || glyph->offset.y != 0) {
            path.transform(tgfx::Matrix::MakeTrans(glyph->offset.x, glyph->offset.y));
          }
          builder.addGlyph(path, glyph->advance);
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
            auto commaPos = imageNode->filePath.find(',');
            if (commaPos != std::string::npos) {
              auto header = imageNode->filePath.substr(0, commaPos);
              if (header.find(";base64") != std::string::npos) {
                auto base64Data = imageNode->filePath.substr(commaPos + 1);
                auto data = Base64Decode(base64Data);
                if (data) {
                  codec = tgfx::ImageCodec::MakeFrom(ToTGFXData(data));
                }
              }
            }
          } else if (!imageNode->filePath.empty()) {
            codec = tgfx::ImageCodec::MakeFrom(imageNode->filePath);
          }

          if (codec) {
            builder.addGlyph(codec, ToTGFXPoint(glyph->offset), glyph->advance);
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

    // Current run being built
    ShapedGlyphRun* currentRun = nullptr;
    std::shared_ptr<tgfx::Typeface> currentTypeface = nullptr;
    float runStartX = 0;

    size_t i = 0;
    while (i < content.size()) {
      // Decode UTF-8 character
      tgfx::Unichar unichar = 0;
      size_t charLen = 0;
      uint8_t byte = static_cast<uint8_t>(content[i]);

      if ((byte & 0x80) == 0) {
        unichar = byte;
        charLen = 1;
      } else if ((byte & 0xE0) == 0xC0) {
        unichar = byte & 0x1F;
        charLen = 2;
      } else if ((byte & 0xF0) == 0xE0) {
        unichar = byte & 0x0F;
        charLen = 3;
      } else if ((byte & 0xF8) == 0xF0) {
        unichar = byte & 0x07;
        charLen = 4;
      } else {
        i++;
        continue;
      }

      for (size_t j = 1; j < charLen && i + j < content.size(); j++) {
        unichar = (unichar << 6) | (static_cast<uint8_t>(content[i + j]) & 0x3F);
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
          if (fallback == nullptr || fallback == primaryTypeface) {
            continue;
          }
          tgfx::Font fallbackFont(fallback, text->fontSize);
          glyphID = fallbackFont.getGlyphID(unichar);
          if (glyphID != 0) {
            glyphFont = fallbackFont;
            glyphTypeface = fallback;
            break;
          }
        }
      }

      if (glyphID == 0) {
        continue;
      }

      float advance = glyphFont.getAdvance(glyphID);

      // Check if glyph has renderable content
      tgfx::Path testPath = {};
      bool hasOutline = glyphFont.getPath(glyphID, &testPath) && !testPath.isEmpty();
      bool hasImage = glyphFont.getImage(glyphID, nullptr, nullptr) != nullptr;

      if (hasOutline || hasImage) {
        // Start new run if typeface changed
        if (currentTypeface != glyphTypeface) {
          info.runs.emplace_back();
          currentRun = &info.runs.back();
          currentRun->font = glyphFont;
          currentTypeface = glyphTypeface;
          runStartX = currentX;
          currentRun->startX = runStartX;
          // Can use Default mode if no letterSpacing (positions follow advance values)
          currentRun->canUseDefaultMode = !hasLetterSpacing;
        }

        currentRun->xPositions.push_back(currentX);
        currentRun->glyphIDs.push_back(glyphID);
      }

      currentX += advance + text->letterSpacing;
    }

    info.totalWidth = currentX;
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
      for (const auto& pair : typesetter->registeredTypefaces) {
        if (pair.first.family == fontFamily) {
          return pair.second;
        }
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
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> fontCache = {};
};

ShapedTextMap Typesetter::shape(PAGXDocument* document) {
  TypesetterContext context(this, document);
  return context.run();
}

}  // namespace pagx
