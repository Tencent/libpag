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

#include "pagx/TextShaper.h"
#include <unordered_map>
#include <unordered_set>
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Text.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathTypes.h"
#include "tgfx/core/TextBlob.h"

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

// Private implementation class
class TextShaperImpl : public TextShaper {
 public:
  explicit TextShaperImpl(std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces)
      : _fallbackTypefaces(std::move(fallbackTypefaces)) {
  }

  bool shape(PAGXDocument* document) override {
    if (!document) {
      return false;
    }

    _document = document;
    _textShaped = false;
    _fontResources.clear();
    _glyphMapping.clear();

    // Process all layers
    for (auto& layer : _document->layers) {
      processLayer(layer.get());
    }

    // Process compositions in resources
    for (auto& res : _document->resources) {
      if (res->nodeType() == NodeType::Composition) {
        auto comp = static_cast<Composition*>(res.get());
        for (auto& layer : comp->layers) {
          processLayer(layer.get());
        }
      }
    }

    // Add collected Font resources to document
    for (auto& fontPair : _fontResources) {
      _document->resources.push_back(std::move(fontPair.second));
    }

    return _textShaped;
  }

 private:
  void processLayer(Layer* layer) {
    if (!layer) {
      return;
    }

    // Process layer contents
    for (auto& element : layer->contents) {
      processElement(element.get());
    }

    // Process child layers
    for (auto& child : layer->children) {
      processLayer(child.get());
    }
  }

  void processElement(Element* element) {
    if (!element) {
      return;
    }

    switch (element->nodeType()) {
      case NodeType::Text: {
        processText(static_cast<Text*>(element));
        break;
      }
      case NodeType::Group: {
        auto group = static_cast<Group*>(element);
        for (auto& child : group->elements) {
          processElement(child.get());
        }
        break;
      }
      default:
        break;
    }
  }

  void processText(Text* text) {
    if (!text) {
      return;
    }

    // Skip if already pre-shaped
    if (!text->glyphRuns.empty()) {
      return;
    }

    // Skip if no text content
    if (text->text.empty()) {
      return;
    }

    // Find typeface for this text
    std::shared_ptr<tgfx::Typeface> typeface = nullptr;
    if (!text->fontFamily.empty() && !_fallbackTypefaces.empty()) {
      for (const auto& tf : _fallbackTypefaces) {
        if (tf && tf->fontFamily() == text->fontFamily) {
          typeface = tf;
          break;
        }
      }
    }
    if (!typeface && !_fallbackTypefaces.empty()) {
      typeface = _fallbackTypefaces[0];
    }
    if (!typeface && !text->fontFamily.empty()) {
      typeface = tgfx::Typeface::MakeFromName(text->fontFamily, text->fontStyle);
    }
    if (!typeface) {
      return;
    }

    // Shape the text
    tgfx::Font font(typeface, text->fontSize);

    // Get glyph IDs and advances
    std::vector<tgfx::GlyphID> glyphIDs = {};
    std::vector<float> xPositions = {};

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
      tgfx::GlyphID glyphID = font.getGlyphID(unichar);
      if (glyphID == 0) {
        continue;
      }

      // Get glyph advance first (needed for spacing even if no outline)
      float advance = font.getAdvance(glyphID);

      // Check if glyph has an outline (skip space-like characters without outlines)
      tgfx::Path testPath;
      bool hasOutline = font.getPath(glyphID, &testPath) && !testPath.isEmpty();
      if (hasOutline) {
        xPositions.push_back(currentX);
        glyphIDs.push_back(glyphID);
      }

      // Always advance position
      currentX += advance + text->letterSpacing;
    }

    if (glyphIDs.empty()) {
      return;
    }

    // Get or create Font resource for this typeface
    std::string fontId = getOrCreateFontResource(typeface, font, glyphIDs);

    // Create GlyphRun with remapped glyph IDs
    auto glyphRun = std::make_unique<GlyphRun>();
    glyphRun->font = "@" + fontId;

    // Remap glyph IDs to font-specific indices
    for (tgfx::GlyphID glyphID : glyphIDs) {
      auto it = _glyphMapping[fontId].find(glyphID);
      if (it != _glyphMapping[fontId].end()) {
        glyphRun->glyphs.push_back(it->second);
      } else {
        glyphRun->glyphs.push_back(0);  // Missing glyph
      }
    }

    // For horizontal text, y should be 0 (relative to baseline).
    // Text.position.y in the document represents the baseline position.
    glyphRun->y = 0;
    glyphRun->xPositions = std::move(xPositions);

    text->glyphRuns.push_back(std::move(glyphRun));
    _textShaped = true;
  }

  std::string getOrCreateFontResource(const std::shared_ptr<tgfx::Typeface>& typeface,
                                      const tgfx::Font& font,
                                      const std::vector<tgfx::GlyphID>& glyphIDs) {
    // Create unique font ID based on typeface identity
    std::string fontId = "font_" + std::to_string(reinterpret_cast<uintptr_t>(typeface.get()));

    // Check if font resource already exists
    auto it = _fontResources.find(fontId);
    if (it == _fontResources.end()) {
      // Create new Font resource
      auto fontNode = std::make_unique<Font>();
      fontNode->id = fontId;
      _fontResources[fontId] = std::move(fontNode);
      _glyphMapping[fontId] = {};
    }

    Font* fontNode = _fontResources[fontId].get();
    auto& glyphMap = _glyphMapping[fontId];

    // Add any new glyphs
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
      auto glyph = std::make_unique<Glyph>();
      glyph->path = pathStr;

      // Map original glyph ID to new index (1-based, since PathTypefaceBuilder uses 1-based IDs)
      glyphMap[glyphID] = static_cast<tgfx::GlyphID>(fontNode->glyphs.size() + 1);

      fontNode->glyphs.push_back(std::move(glyph));
    }

    return fontId;
  }

  std::vector<std::shared_ptr<tgfx::Typeface>> _fallbackTypefaces = {};
  PAGXDocument* _document = nullptr;
  bool _textShaped = false;

  // Font ID -> Font resource
  std::unordered_map<std::string, std::unique_ptr<Font>> _fontResources = {};

  // Font ID -> (original GlyphID -> new GlyphID)
  std::unordered_map<std::string, std::unordered_map<tgfx::GlyphID, tgfx::GlyphID>> _glyphMapping =
      {};
};

std::shared_ptr<TextShaper> TextShaper::Make(
    std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces) {
  if (fallbackTypefaces.empty()) {
    return nullptr;
  }
  return std::make_shared<TextShaperImpl>(std::move(fallbackTypefaces));
}

}  // namespace pagx
