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

#include "pagx/FontEmbedder.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "SVGPathParser.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/GlyphRun.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"

namespace pagx {

// Scales RGBA8888 pixels using bilinear interpolation for smooth downscaling.
static void ScalePixelsBilinear(const uint8_t* srcPixels, int srcW, int srcH, size_t srcRowBytes,
                                uint8_t* dstPixels, int dstW, int dstH, size_t dstRowBytes) {
  float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
  float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);

  for (int y = 0; y < dstH; y++) {
    float srcY = (static_cast<float>(y) + 0.5f) * scaleY - 0.5f;
    int y0 = static_cast<int>(std::floor(srcY));
    int y1 = y0 + 1;
    float fy = srcY - static_cast<float>(y0);

    y0 = std::max(0, std::min(y0, srcH - 1));
    y1 = std::max(0, std::min(y1, srcH - 1));

    auto* dstRow = dstPixels + y * dstRowBytes;

    for (int x = 0; x < dstW; x++) {
      float srcX = (static_cast<float>(x) + 0.5f) * scaleX - 0.5f;
      int x0 = static_cast<int>(std::floor(srcX));
      int x1 = x0 + 1;
      float fx = srcX - static_cast<float>(x0);

      x0 = std::max(0, std::min(x0, srcW - 1));
      x1 = std::max(0, std::min(x1, srcW - 1));

      const auto* p00 = srcPixels + y0 * srcRowBytes + x0 * 4;
      const auto* p10 = srcPixels + y0 * srcRowBytes + x1 * 4;
      const auto* p01 = srcPixels + y1 * srcRowBytes + x0 * 4;
      const auto* p11 = srcPixels + y1 * srcRowBytes + x1 * 4;

      for (int c = 0; c < 4; c++) {
        float v00 = static_cast<float>(p00[c]);
        float v10 = static_cast<float>(p10[c]);
        float v01 = static_cast<float>(p01[c]);
        float v11 = static_cast<float>(p11[c]);

        float v0 = v00 + (v10 - v00) * fx;
        float v1 = v01 + (v11 - v01) * fx;
        float v = v0 + (v1 - v0) * fy;

        dstRow[x * 4 + c] = static_cast<uint8_t>(std::round(std::max(0.0f, std::min(255.0f, v))));
      }
    }
  }
}

// Converts a tgfx::Path to SVG path string.
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

// Internal implementation class for FontEmbedder.
class FontEmbedderImpl {
 public:
  explicit FontEmbedderImpl(PAGXDocument* document) : document(document) {
  }

  bool embed(const TextGlyphs& textGlyphs) {
    if (document == nullptr) {
      return false;
    }

    // Process each Text and create GlyphRuns with embedded fonts
    textGlyphs.forEach([this](Text* text, std::shared_ptr<tgfx::TextBlob> textBlob) {
      if (textBlob == nullptr) {
        return;
      }
      createGlyphRuns(text, textBlob);
    });

    return true;
  }

 private:
  void createGlyphRuns(Text* text, const std::shared_ptr<tgfx::TextBlob>& textBlob) {
    // Clear existing glyph runs
    text->glyphRuns.clear();

    for (const auto& run : *textBlob) {
      if (run.glyphCount == 0) {
        continue;
      }

      // Get or create Font resource for this typeface + font size combination
      std::string fontId = getOrCreateFontResource(run.font, run.glyphs, run.glyphCount);
      if (fontId.empty()) {
        continue;
      }

      // Create GlyphRun with remapped glyph IDs
      auto glyphRun = document->makeNode<GlyphRun>();
      glyphRun->font = fontResources[fontId];

      // Remap glyph IDs to font-specific indices
      auto& glyphMap = glyphMapping[fontId];
      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        auto it = glyphMap.find(glyphID);
        if (it != glyphMap.end()) {
          glyphRun->glyphs.push_back(it->second);
        } else {
          glyphRun->glyphs.push_back(0);  // Missing glyph
        }
      }

      // Copy positions based on positioning mode
      switch (run.positioning) {
        case tgfx::GlyphPositioning::Horizontal: {
          glyphRun->y = run.offsetY;
          for (size_t i = 0; i < run.glyphCount; ++i) {
            glyphRun->xPositions.push_back(run.positions[i]);
          }
          break;
        }
        case tgfx::GlyphPositioning::Point: {
          auto* points = reinterpret_cast<const tgfx::Point*>(run.positions);
          for (size_t i = 0; i < run.glyphCount; ++i) {
            glyphRun->positions.push_back({points[i].x, points[i].y});
          }
          break;
        }
        case tgfx::GlyphPositioning::RSXform: {
          auto* xforms = reinterpret_cast<const tgfx::RSXform*>(run.positions);
          for (size_t i = 0; i < run.glyphCount; ++i) {
            RSXform xform = {};
            xform.scos = xforms[i].scos;
            xform.ssin = xforms[i].ssin;
            xform.tx = xforms[i].tx;
            xform.ty = xforms[i].ty;
            glyphRun->xforms.push_back(xform);
          }
          break;
        }
        case tgfx::GlyphPositioning::Matrix: {
          auto* matrices = reinterpret_cast<const float*>(run.positions);
          for (size_t i = 0; i < run.glyphCount; ++i) {
            Matrix m = {};
            m.a = matrices[i * 6 + 0];
            m.b = matrices[i * 6 + 1];
            m.c = matrices[i * 6 + 2];
            m.d = matrices[i * 6 + 3];
            m.tx = matrices[i * 6 + 4];
            m.ty = matrices[i * 6 + 5];
            glyphRun->matrices.push_back(m);
          }
          break;
        }
      }

      text->glyphRuns.push_back(glyphRun);
    }
  }

  std::string getOrCreateFontResource(const tgfx::Font& font, const tgfx::GlyphID* glyphs,
                                      size_t glyphCount) {
    auto typeface = font.getTypeface();
    if (typeface == nullptr) {
      return "";
    }

    // Create a key combining typeface pointer and font size for lookup.
    // Different font sizes produce different glyph paths, so they need separate Font resources.
    std::string lookupKey = std::to_string(reinterpret_cast<uintptr_t>(typeface.get())) + "_" +
                            std::to_string(static_cast<int>(font.getSize()));

    // Check if we already have a font ID for this key
    auto keyIt = fontKeyToId.find(lookupKey);
    if (keyIt != fontKeyToId.end()) {
      // Font resource already exists, just add any new glyphs
      std::string fontId = keyIt->second;
      addGlyphsToFont(fontId, font, glyphs, glyphCount);
      return fontId;
    }

    // Create new font ID using incremental counter
    std::string fontId = "font_" + std::to_string(nextFontId++);
    fontKeyToId[lookupKey] = fontId;

    // Create new Font resource with the ID so it gets registered in nodeMap
    auto fontNode = document->makeNode<Font>(fontId);
    fontResources[fontId] = fontNode;
    glyphMapping[fontId] = {};

    // Add glyphs to the new font
    addGlyphsToFont(fontId, font, glyphs, glyphCount);
    return fontId;
  }

  void addGlyphsToFont(const std::string& fontId, const tgfx::Font& font,
                       const tgfx::GlyphID* glyphs, size_t glyphCount) {
    Font* fontNode = fontResources[fontId];
    auto& glyphMap = glyphMapping[fontId];

    for (size_t i = 0; i < glyphCount; ++i) {
      tgfx::GlyphID glyphID = glyphs[i];
      if (glyphMap.find(glyphID) != glyphMap.end()) {
        continue;  // Already added
      }

      // Try to get glyph path first (vector outline)
      tgfx::Path glyphPath = {};
      bool hasPath = font.getPath(glyphID, &glyphPath) && !glyphPath.isEmpty();

      // Try to get glyph image (bitmap, e.g., color emoji)
      tgfx::Matrix imageMatrix = {};
      auto imageCodec = font.getImage(glyphID, nullptr, &imageMatrix);

      if (!hasPath && !imageCodec) {
        continue;  // No renderable content
      }

      // Create Glyph node
      auto glyph = document->makeNode<Glyph>();

      if (hasPath) {
        // Vector glyph
        std::string pathStr = PathToSVGString(glyphPath);
        if (!pathStr.empty()) {
          glyph->path = document->makeNode<PathData>();
          *glyph->path = PathDataFromSVGString(pathStr);
        }
      } else if (imageCodec) {
        // Bitmap glyph (e.g., color emoji)
        int srcW = imageCodec->width();
        int srcH = imageCodec->height();
        float scaleX = imageMatrix.getScaleX();
        float scaleY = imageMatrix.getScaleY();
        int dstW = static_cast<int>(std::round(static_cast<float>(srcW) * scaleX));
        int dstH = static_cast<int>(std::round(static_cast<float>(srcH) * scaleY));
        if (dstW > 0 && dstH > 0) {
          tgfx::Bitmap srcBitmap(srcW, srcH, false, false);
          if (!srcBitmap.isEmpty()) {
            auto* srcPixels = srcBitmap.lockPixels();
            if (srcPixels && imageCodec->readPixels(srcBitmap.info(), srcPixels)) {
              srcBitmap.unlockPixels();
              tgfx::Bitmap dstBitmap(dstW, dstH, false, false);
              if (!dstBitmap.isEmpty()) {
                auto* dstPixels = dstBitmap.lockPixels();
                if (dstPixels) {
                  auto* srcReadPixels = static_cast<const uint8_t*>(srcBitmap.lockPixels());
                  ScalePixelsBilinear(srcReadPixels, srcW, srcH, srcBitmap.info().rowBytes(),
                                      static_cast<uint8_t*>(dstPixels), dstW, dstH,
                                      dstBitmap.info().rowBytes());
                  srcBitmap.unlockPixels();
                  dstBitmap.unlockPixels();
                  auto pngData = dstBitmap.encode(tgfx::EncodedFormat::PNG, 100);
                  if (pngData) {
                    auto image = document->makeNode<Image>();
                    image->data = pagx::Data::MakeWithCopy(pngData->data(), pngData->size());
                    glyph->image = image;
                    glyph->offset.x = imageMatrix.getTranslateX();
                    glyph->offset.y = imageMatrix.getTranslateY();
                  }
                } else {
                  dstBitmap.unlockPixels();
                }
              }
            } else {
              srcBitmap.unlockPixels();
            }
          }
        }
      }

      // Only add glyph if it has content
      if (glyph->path || glyph->image) {
        // Map original glyph ID to new index (1-based, since PathTypefaceBuilder uses 1-based IDs)
        glyphMap[glyphID] = static_cast<tgfx::GlyphID>(fontNode->glyphs.size() + 1);
        fontNode->glyphs.push_back(glyph);
      }
    }
  }

  PAGXDocument* document = nullptr;

  // Font ID -> Font resource
  std::unordered_map<std::string, Font*> fontResources = {};

  // Font ID -> (original GlyphID -> new GlyphID)
  std::unordered_map<std::string, std::unordered_map<tgfx::GlyphID, tgfx::GlyphID>> glyphMapping =
      {};

  // Lookup key (typeface_ptr + font_size) -> Font ID
  std::unordered_map<std::string, std::string> fontKeyToId = {};

  // Counter for generating incremental font IDs
  int nextFontId = 0;
};

bool FontEmbedder::Embed(PAGXDocument* document, const TextGlyphs& textGlyphs) {
  FontEmbedderImpl impl(document);
  return impl.embed(textGlyphs);
}

}  // namespace pagx
