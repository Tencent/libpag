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
#include <cmath>
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextLayout.h"
#include "tgfx/core/Font.h"
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

// Internal implementation class for createTextGlyphs.
class TypesetterContext {
 public:
  TypesetterContext(const Typesetter* typesetter, PAGXDocument* document)
      : typesetter(typesetter), document(document) {
  }

  TextGlyphs run() {
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
  // Shaped text information for a single Text element.
  struct ShapedInfo {
    Text* text = nullptr;
    std::vector<tgfx::GlyphID> glyphIDs = {};
    std::vector<float> xPositions = {};
    tgfx::Font font = {};
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
      if (info.glyphIDs.empty()) {
        continue;
      }

      float xOffset = 0;
      float yOffset = 0;

      if (textLayout != nullptr) {
        xOffset = calculateLayoutOffset(textLayout, info.totalWidth);

        if (textLayout->position.x != 0 || textLayout->position.y != 0) {
          xOffset += textLayout->position.x - info.text->position.x;
          yOffset = textLayout->position.y - info.text->position.y;
        }
      }

      // Apply offsets to positions
      std::vector<float> adjustedPositions = {};
      adjustedPositions.reserve(info.xPositions.size());
      for (float x : info.xPositions) {
        adjustedPositions.push_back(x + xOffset);
      }

      // Build TextBlob
      tgfx::TextBlobBuilder builder = {};
      auto& buffer = builder.allocRunPosH(info.font, info.glyphIDs.size(), yOffset);
      memcpy(buffer.glyphs, info.glyphIDs.data(), info.glyphIDs.size() * sizeof(tgfx::GlyphID));
      memcpy(buffer.positions, adjustedPositions.data(), adjustedPositions.size() * sizeof(float));

      auto textBlob = builder.build();
      if (textBlob != nullptr) {
        result.add(info.text, textBlob);
      }
    }
  }

  void shapeText(Text* text, ShapedInfo& info) {
    auto primaryTypeface = findTypeface(text->fontFamily, text->fontStyle);
    if (primaryTypeface == nullptr) {
      return;
    }

    tgfx::Font primaryFont(primaryTypeface, text->fontSize);
    info.font = primaryFont;
    float currentX = 0;
    const std::string& content = text->text;

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

      if (glyphID == 0) {
        for (const auto& fallback : typesetter->fallbackTypefaces) {
          if (fallback == nullptr || fallback == primaryTypeface) {
            continue;
          }
          tgfx::Font fallbackFont(fallback, text->fontSize);
          glyphID = fallbackFont.getGlyphID(unichar);
          if (glyphID != 0) {
            glyphFont = fallbackFont;
            // Note: for simplicity, we use the primary font for all glyphs.
            // A more complete implementation would track font changes per run.
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
        info.xPositions.push_back(currentX);
        info.glyphIDs.push_back(glyphID);
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
  TextGlyphs result = {};
};

TextGlyphs Typesetter::createTextGlyphs(PAGXDocument* document) {
  TypesetterContext context(this, document);
  return context.run();
}

}  // namespace pagx
