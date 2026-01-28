/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <unordered_map>
#include <unordered_set>
#include "SVGPathParser.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextLayout.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathTypes.h"

namespace pagx {

// Converts a tgfx::Path to SVG path string
static std::string PathToSVGString(const tgfx::Path& path) {
  std::string result = {};
  result.reserve(256);
  char buf[64] = {};

  path.decompose([&](tgfx::PathVerb verb, const tgfx::Point pts[4], void*) {
    switch (verb) {
      case tgfx::PathVerb::Move:
        snprintf(buf, sizeof(buf), "M%g %g", pts[0].x, pts[0].y);
        result += buf;
        break;
      case tgfx::PathVerb::Line:
        snprintf(buf, sizeof(buf), "L%g %g", pts[1].x, pts[1].y);
        result += buf;
        break;
      case tgfx::PathVerb::Quad:
        snprintf(buf, sizeof(buf), "Q%g %g %g %g", pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        result += buf;
        break;
      case tgfx::PathVerb::Cubic:
        snprintf(buf, sizeof(buf), "C%g %g %g %g %g %g", pts[1].x, pts[1].y, pts[2].x, pts[2].y,
                 pts[3].x, pts[3].y);
        result += buf;
        break;
      case tgfx::PathVerb::Close:
        result += "Z";
        break;
    }
  });

  return result;
}

// Key for font registration: fontFamily + fontStyle
struct FontKey {
  std::string family = {};
  std::string style = {};

  bool operator==(const FontKey& other) const {
    return family == other.family && style == other.style;
  }
};

struct FontKeyHash {
  size_t operator()(const FontKey& key) const {
    return std::hash<std::string>()(key.family) ^ (std::hash<std::string>()(key.style) << 1);
  }
};

// Intermediate shaping result for a single Text element
struct ShapedTextInfo {
  Text* text = nullptr;
  std::vector<tgfx::GlyphID> originalGlyphIDs = {};
  std::vector<float> xPositions = {};
  float totalWidth = 0;
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  tgfx::Font font = {};
};

// Private implementation class
class TypesetterImpl : public Typesetter {
 public:
  TypesetterImpl() = default;

  void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) override {
    if (!typeface) {
      return;
    }
    FontKey key;
    key.family = typeface->fontFamily();
    key.style = typeface->fontStyle();
    _registeredTypefaces[key] = std::move(typeface);
  }

  void setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) override {
    _fallbackTypefaces = std::move(typefaces);
  }

  bool typeset(PAGXDocument* document, bool force) override {
    if (!document) {
      return false;
    }

    _document = document;
    _force = force;
    _textTypeset = false;
    _fontResources.clear();
    _glyphMapping.clear();

    // Process all layers
    for (auto& layer : _document->layers) {
      processLayer(layer);
    }

    // Process compositions in nodes
    for (auto& node : _document->nodes) {
      if (node->nodeType() == NodeType::Composition) {
        auto comp = static_cast<Composition*>(node.get());
        for (auto& layer : comp->layers) {
          processLayer(layer);
        }
      }
    }

    return _textTypeset;
  }

 private:
  void processLayer(Layer* layer) {
    if (!layer) {
      return;
    }

    // Process layer contents, looking for Text + TextLayout combinations
    processLayerContents(layer->contents);

    // Process child layers
    for (auto& child : layer->children) {
      processLayer(child);
    }
  }

  void processLayerContents(std::vector<Element*>& contents) {
    // Find TextLayout in layer contents
    const TextLayout* textLayout = nullptr;
    std::vector<Text*> textElements = {};

    for (auto& element : contents) {
      if (element->nodeType() == NodeType::TextLayout) {
        textLayout = static_cast<const TextLayout*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      } else if (element->nodeType() == NodeType::Group) {
        processGroup(static_cast<Group*>(element));
      }
    }

    // If we found Text elements at Layer level (not in Group)
    if (!textElements.empty()) {
      processTextWithLayout(textElements, textLayout);
    }
  }

  void processGroup(Group* group) {
    if (!group) {
      return;
    }

    // Find TextLayout modifier and collect Text elements
    const TextLayout* textLayout = nullptr;
    std::vector<Text*> textElements = {};

    for (auto& element : group->elements) {
      if (element->nodeType() == NodeType::TextLayout) {
        textLayout = static_cast<const TextLayout*>(element);
      } else if (element->nodeType() == NodeType::Text) {
        textElements.push_back(static_cast<Text*>(element));
      } else if (element->nodeType() == NodeType::Group) {
        // Recursively process nested groups
        processGroup(static_cast<Group*>(element));
      }
    }

    // Process Text elements with optional TextLayout
    if (!textElements.empty()) {
      processTextWithLayout(textElements, textLayout);
    }
  }

  void processTextWithLayout(std::vector<Text*>& textElements, const TextLayout* textLayout) {
    // Check if any Text needs typesetting
    bool needsTypesetting = _force;
    if (!needsTypesetting) {
      for (auto* text : textElements) {
        if (text->glyphRuns.empty() && !text->text.empty()) {
          needsTypesetting = true;
          break;
        }
      }
    }

    if (!needsTypesetting) {
      return;
    }

    // Clear all GlyphRuns for re-typesetting
    for (auto* text : textElements) {
      text->glyphRuns.clear();
    }

    // Shape all Text elements first
    std::vector<ShapedTextInfo> shapedInfos = {};
    for (auto* text : textElements) {
      if (text->text.empty()) {
        shapedInfos.push_back({});
        continue;
      }
      shapedInfos.push_back(shapeText(text));
    }

    // Apply TextLayout if present
    for (size_t i = 0; i < textElements.size(); ++i) {
      auto* text = textElements[i];
      auto& shapedInfo = shapedInfos[i];

      if (shapedInfo.originalGlyphIDs.empty()) {
        continue;
      }

      // Calculate alignment offset from TextLayout
      float alignOffset = 0;
      float positionOffsetX = 0;
      float positionOffsetY = 0;

      if (textLayout != nullptr) {
        alignOffset = calculateLayoutOffset(textLayout, shapedInfo.totalWidth);

        // If TextLayout has a non-zero position, it overrides Text.position.
        // Calculate the offset needed to move from Text.position to TextLayout.position.
        if (textLayout->position.x != 0 || textLayout->position.y != 0) {
          positionOffsetX = textLayout->position.x - text->position.x;
          positionOffsetY = textLayout->position.y - text->position.y;
        }
      }

      // GlyphRun positions are relative to Text.position (applied by LayerBuilder).
      // We only need to include alignment offset and any position override from TextLayout.
      float xOffset = alignOffset + positionOffsetX;
      float yOffset = positionOffsetY;

      // Create GlyphRun with calculated offsets
      createGlyphRun(text, shapedInfo, xOffset, yOffset);
      _textTypeset = true;
    }
  }

  float calculateLayoutOffset(const TextLayout* layout, float textWidth) {
    float xOffset = 0;
    switch (layout->textAlign) {
      case TextAlign::Start:
        // No offset needed
        break;
      case TextAlign::Center:
        xOffset = -0.5f * textWidth;
        break;
      case TextAlign::End:
        xOffset = -textWidth;
        break;
      case TextAlign::Justify:
        // Justify requires more complex handling, treat as start for now
        break;
    }
    return xOffset;
  }

  ShapedTextInfo shapeText(Text* text) {
    ShapedTextInfo info = {};
    info.text = text;

    // Find typeface for this text
    auto typeface = findTypeface(text->fontFamily, text->fontStyle);
    if (!typeface) {
      return info;
    }

    info.typeface = typeface;
    info.font = tgfx::Font(typeface, text->fontSize);

    float currentX = 0;
    const std::string& content = text->text;

    // Simple text shaping: iterate through characters
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

      // Get glyph ID
      tgfx::GlyphID glyphID = info.font.getGlyphID(unichar);
      if (glyphID == 0) {
        continue;
      }

      // Get glyph advance first (needed for spacing even if no outline)
      float advance = info.font.getAdvance(glyphID);

      // Check if glyph has an outline (skip space-like characters without outlines)
      tgfx::Path testPath;
      bool hasOutline = info.font.getPath(glyphID, &testPath) && !testPath.isEmpty();
      if (hasOutline) {
        info.xPositions.push_back(currentX);
        info.originalGlyphIDs.push_back(glyphID);
      }

      // Always advance position
      currentX += advance + text->letterSpacing;
    }

    info.totalWidth = currentX;
    return info;
  }

  void createGlyphRun(Text* text, const ShapedTextInfo& info, float xOffset, float yOffset) {
    if (info.originalGlyphIDs.empty()) {
      return;
    }

    // Get or create Font resource for this typeface
    std::string fontId = getOrCreateFontResource(info.typeface, info.font, info.originalGlyphIDs);

    // Create GlyphRun with remapped glyph IDs
    auto glyphRun = _document->makeNode<GlyphRun>();
    glyphRun->font = _fontResources[fontId];

    // Remap glyph IDs to font-specific indices
    for (tgfx::GlyphID glyphID : info.originalGlyphIDs) {
      auto it = _glyphMapping[fontId].find(glyphID);
      if (it != _glyphMapping[fontId].end()) {
        glyphRun->glyphs.push_back(it->second);
      } else {
        glyphRun->glyphs.push_back(0);  // Missing glyph
      }
    }

    // Apply layout offset to x positions
    glyphRun->xPositions.reserve(info.xPositions.size());
    for (float x : info.xPositions) {
      glyphRun->xPositions.push_back(x + xOffset);
    }

    // Apply y offset (for TextLayout position override)
    glyphRun->y = yOffset;

    text->glyphRuns.push_back(glyphRun);
  }

  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle) {
    // First, try exact match from registered typefaces
    if (!fontFamily.empty()) {
      FontKey key;
      key.family = fontFamily;
      key.style = fontStyle.empty() ? "Regular" : fontStyle;
      auto it = _registeredTypefaces.find(key);
      if (it != _registeredTypefaces.end()) {
        return it->second;
      }

      // Try matching family only (any style)
      for (const auto& pair : _registeredTypefaces) {
        if (pair.first.family == fontFamily) {
          return pair.second;
        }
      }
    }

    // Then, try fallback typefaces
    if (!fontFamily.empty()) {
      for (const auto& tf : _fallbackTypefaces) {
        if (tf && tf->fontFamily() == fontFamily) {
          return tf;
        }
      }
    }

    // Use first fallback typeface
    if (!_fallbackTypefaces.empty()) {
      return _fallbackTypefaces[0];
    }

    // Last resort: try system font
    if (!fontFamily.empty()) {
      return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    }

    return nullptr;
  }

  std::string getOrCreateFontResource(const std::shared_ptr<tgfx::Typeface>& typeface,
                                      const tgfx::Font& font,
                                      const std::vector<tgfx::GlyphID>& glyphIDs) {
    // Create a key combining typeface pointer and font size for lookup.
    // Different font sizes produce different glyph paths, so they need separate Font resources.
    std::string lookupKey = std::to_string(reinterpret_cast<uintptr_t>(typeface.get())) + "_" +
                            std::to_string(static_cast<int>(font.getSize()));

    // Check if we already have a font ID for this key
    auto keyIt = _fontKeyToId.find(lookupKey);
    if (keyIt != _fontKeyToId.end()) {
      // Font resource already exists, just add any new glyphs
      std::string fontId = keyIt->second;
      addGlyphsToFont(fontId, font, glyphIDs);
      return fontId;
    }

    // Create new font ID using incremental counter
    std::string fontId = "font_" + std::to_string(_nextFontId++);
    _fontKeyToId[lookupKey] = fontId;

    // Create new Font resource with the ID so it gets registered in nodeMap
    auto fontNode = _document->makeNode<Font>(fontId);
    _fontResources[fontId] = fontNode;
    _glyphMapping[fontId] = {};

    // Add glyphs to the new font
    addGlyphsToFont(fontId, font, glyphIDs);
    return fontId;
  }

  void addGlyphsToFont(const std::string& fontId, const tgfx::Font& font,
                       const std::vector<tgfx::GlyphID>& glyphIDs) {
    Font* fontNode = _fontResources[fontId];
    auto& glyphMap = _glyphMapping[fontId];

    for (tgfx::GlyphID glyphID : glyphIDs) {
      if (glyphMap.find(glyphID) != glyphMap.end()) {
        continue;  // Already added
      }

      // Get glyph path
      tgfx::Path glyphPath;
      if (!font.getPath(glyphID, &glyphPath)) {
        continue;  // No outline (e.g., color emoji)
      }

      // Convert path to SVG string
      std::string pathStr = PathToSVGString(glyphPath);
      if (pathStr.empty()) {
        continue;
      }

      // Create Glyph node
      auto glyph = _document->makeNode<Glyph>();
      glyph->path = _document->makeNode<PathData>();
      *glyph->path = PathDataFromSVGString(pathStr);

      // Map original glyph ID to new index (1-based, since PathTypefaceBuilder uses 1-based IDs)
      glyphMap[glyphID] = static_cast<tgfx::GlyphID>(fontNode->glyphs.size() + 1);

      fontNode->glyphs.push_back(glyph);
    }
  }

  // Registered typefaces by family + style
  std::unordered_map<FontKey, std::shared_ptr<tgfx::Typeface>, FontKeyHash> _registeredTypefaces =
      {};

  // Fallback typefaces
  std::vector<std::shared_ptr<tgfx::Typeface>> _fallbackTypefaces = {};

  // Current processing state
  PAGXDocument* _document = nullptr;
  bool _force = false;
  bool _textTypeset = false;

  // Font ID -> Font resource
  std::unordered_map<std::string, Font*> _fontResources = {};

  // Font ID -> (original GlyphID -> new GlyphID)
  std::unordered_map<std::string, std::unordered_map<tgfx::GlyphID, tgfx::GlyphID>> _glyphMapping =
      {};

  // Lookup key (typeface_ptr + font_size) -> Font ID
  std::unordered_map<std::string, std::string> _fontKeyToId = {};

  // Counter for generating incremental font IDs
  int _nextFontId = 0;
};

std::shared_ptr<Typesetter> Typesetter::Make() {
  return std::make_shared<TypesetterImpl>();
}

}  // namespace pagx
