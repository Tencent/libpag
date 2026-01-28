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

// Key for identifying unique glyphs: typeface pointer + glyph ID.
struct GlyphKey {
  const tgfx::Typeface* typeface = nullptr;
  tgfx::GlyphID glyphID = 0;

  bool operator==(const GlyphKey& other) const {
    return typeface == other.typeface && glyphID == other.glyphID;
  }
};

struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const {
    return std::hash<const void*>()(key.typeface) ^ (std::hash<uint16_t>()(key.glyphID) << 1);
  }
};

// Information about a glyph to be embedded.
struct GlyphInfo {
  std::string pathString = {};
  std::shared_ptr<tgfx::ImageCodec> imageCodec = nullptr;
  tgfx::Matrix imageMatrix = {};
};

// Internal implementation class for FontEmbedder.
class FontEmbedderImpl {
 public:
  explicit FontEmbedderImpl(PAGXDocument* document) : document(document) {
  }

  bool embed(const TextGlyphs& textGlyphs) {
    if (document == nullptr) {
      return false;
    }

    // First pass: collect all glyphs and create Font nodes
    textGlyphs.forEach([this](Text* text, std::shared_ptr<tgfx::TextBlob> textBlob) {
      if (textBlob == nullptr) {
        return;
      }
      collectGlyphs(textBlob);
    });

    // Create Font nodes if we have glyphs
    createFontNodes();

    // Second pass: create GlyphRuns for each Text
    textGlyphs.forEach([this](Text* text, std::shared_ptr<tgfx::TextBlob> textBlob) {
      if (textBlob == nullptr) {
        return;
      }
      createGlyphRuns(text, textBlob);
    });

    return true;
  }

 private:
  void collectGlyphs(const std::shared_ptr<tgfx::TextBlob>& textBlob) {
    for (const auto& run : *textBlob) {
      auto* typeface = run.font.getTypeface().get();
      for (size_t i = 0; i < run.glyphCount; ++i) {
        tgfx::GlyphID glyphID = run.glyphs[i];
        GlyphKey key = {typeface, glyphID};

        if (vectorGlyphMapping.find(key) != vectorGlyphMapping.end() ||
            bitmapGlyphMapping.find(key) != bitmapGlyphMapping.end()) {
          continue;  // Already processed
        }

        // Try to get glyph path first (vector outline)
        tgfx::Path glyphPath = {};
        bool hasPath = run.font.getPath(glyphID, &glyphPath) && !glyphPath.isEmpty();

        // Try to get glyph image (bitmap, e.g., color emoji)
        tgfx::Matrix imageMatrix = {};
        auto imageCodec = run.font.getImage(glyphID, nullptr, &imageMatrix);

        if (hasPath) {
          // Vector glyph
          GlyphInfo info = {};
          info.pathString = PathToSVGString(glyphPath);
          if (!info.pathString.empty()) {
            pendingVectorGlyphs.push_back({key, info});
          }
        } else if (imageCodec) {
          // Bitmap glyph
          GlyphInfo info = {};
          info.imageCodec = imageCodec;
          info.imageMatrix = imageMatrix;
          pendingBitmapGlyphs.push_back({key, info});
        }
      }
    }
  }

  void createFontNodes() {
    // Create vector font if needed
    if (!pendingVectorGlyphs.empty()) {
      vectorFont = document->makeNode<Font>("vector_font");
      tgfx::GlyphID nextID = 1;
      for (auto& [key, info] : pendingVectorGlyphs) {
        auto glyph = document->makeNode<Glyph>();
        glyph->path = document->makeNode<PathData>();
        *glyph->path = PathDataFromSVGString(info.pathString);
        vectorFont->glyphs.push_back(glyph);
        vectorGlyphMapping[key] = nextID++;
      }
    }

    // Create bitmap font if needed
    if (!pendingBitmapGlyphs.empty()) {
      bitmapFont = document->makeNode<Font>("bitmap_font");
      tgfx::GlyphID nextID = 1;
      for (auto& [key, info] : pendingBitmapGlyphs) {
        auto glyph = createBitmapGlyph(info);
        if (glyph != nullptr) {
          bitmapFont->glyphs.push_back(glyph);
          bitmapGlyphMapping[key] = nextID++;
        }
      }
    }
  }

  Glyph* createBitmapGlyph(const GlyphInfo& info) {
    auto imageCodec = info.imageCodec;
    auto imageMatrix = info.imageMatrix;

    int srcW = imageCodec->width();
    int srcH = imageCodec->height();
    float scaleX = imageMatrix.getScaleX();
    float scaleY = imageMatrix.getScaleY();
    int dstW = static_cast<int>(std::round(static_cast<float>(srcW) * scaleX));
    int dstH = static_cast<int>(std::round(static_cast<float>(srcH) * scaleY));

    if (dstW <= 0 || dstH <= 0) {
      return nullptr;
    }

    tgfx::Bitmap srcBitmap(srcW, srcH, false, false);
    if (srcBitmap.isEmpty()) {
      return nullptr;
    }

    auto* srcPixels = srcBitmap.lockPixels();
    if (srcPixels == nullptr || !imageCodec->readPixels(srcBitmap.info(), srcPixels)) {
      srcBitmap.unlockPixels();
      return nullptr;
    }
    srcBitmap.unlockPixels();

    tgfx::Bitmap dstBitmap(dstW, dstH, false, false);
    if (dstBitmap.isEmpty()) {
      return nullptr;
    }

    auto* dstPixels = dstBitmap.lockPixels();
    if (dstPixels == nullptr) {
      return nullptr;
    }

    auto* srcReadPixels = static_cast<const uint8_t*>(srcBitmap.lockPixels());
    ScalePixelsBilinear(srcReadPixels, srcW, srcH, srcBitmap.info().rowBytes(),
                        static_cast<uint8_t*>(dstPixels), dstW, dstH, dstBitmap.info().rowBytes());
    srcBitmap.unlockPixels();
    dstBitmap.unlockPixels();

    auto pngData = dstBitmap.encode(tgfx::EncodedFormat::PNG, 100);
    if (pngData == nullptr) {
      return nullptr;
    }

    auto glyph = document->makeNode<Glyph>();
    auto image = document->makeNode<Image>();
    image->data = pagx::Data::MakeWithCopy(pngData->data(), pngData->size());
    glyph->image = image;
    glyph->offset.x = imageMatrix.getTranslateX();
    glyph->offset.y = imageMatrix.getTranslateY();

    return glyph;
  }

  void createGlyphRuns(Text* text, const std::shared_ptr<tgfx::TextBlob>& textBlob) {
    // Clear existing glyph runs
    text->glyphRuns.clear();

    for (const auto& run : *textBlob) {
      auto* typeface = run.font.getTypeface().get();

      // Separate glyphs into vector and bitmap runs
      std::vector<size_t> vectorIndices = {};
      std::vector<size_t> bitmapIndices = {};

      for (size_t i = 0; i < run.glyphCount; ++i) {
        GlyphKey key = {typeface, run.glyphs[i]};
        if (vectorGlyphMapping.find(key) != vectorGlyphMapping.end()) {
          vectorIndices.push_back(i);
        } else if (bitmapGlyphMapping.find(key) != bitmapGlyphMapping.end()) {
          bitmapIndices.push_back(i);
        }
      }

      // Create vector glyph run
      if (!vectorIndices.empty() && vectorFont != nullptr) {
        auto glyphRun = createGlyphRun(run, vectorIndices, vectorFont, vectorGlyphMapping, typeface);
        if (glyphRun != nullptr) {
          text->glyphRuns.push_back(glyphRun);
        }
      }

      // Create bitmap glyph run
      if (!bitmapIndices.empty() && bitmapFont != nullptr) {
        auto glyphRun = createGlyphRun(run, bitmapIndices, bitmapFont, bitmapGlyphMapping, typeface);
        if (glyphRun != nullptr) {
          text->glyphRuns.push_back(glyphRun);
        }
      }
    }
  }

  GlyphRun* createGlyphRun(const tgfx::GlyphRun& run, const std::vector<size_t>& indices,
                           Font* font,
                           const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& mapping,
                           const tgfx::Typeface* typeface) {
    auto glyphRun = document->makeNode<GlyphRun>();
    glyphRun->font = font;

    for (size_t i : indices) {
      GlyphKey key = {typeface, run.glyphs[i]};
      auto it = mapping.find(key);
      if (it != mapping.end()) {
        glyphRun->glyphs.push_back(it->second);
      } else {
        glyphRun->glyphs.push_back(0);
      }
    }

    // Copy positions based on positioning mode
    switch (run.positioning) {
      case tgfx::GlyphPositioning::Horizontal: {
        glyphRun->y = run.offsetY;
        for (size_t i : indices) {
          glyphRun->xPositions.push_back(run.positions[i]);
        }
        break;
      }
      case tgfx::GlyphPositioning::Point: {
        auto* points = reinterpret_cast<const tgfx::Point*>(run.positions);
        for (size_t i : indices) {
          glyphRun->positions.push_back({points[i].x, points[i].y});
        }
        break;
      }
      case tgfx::GlyphPositioning::RSXform: {
        auto* xforms = reinterpret_cast<const tgfx::RSXform*>(run.positions);
        for (size_t i : indices) {
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
        for (size_t i : indices) {
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

    return glyphRun;
  }

  PAGXDocument* document = nullptr;
  Font* vectorFont = nullptr;
  Font* bitmapFont = nullptr;

  std::vector<std::pair<GlyphKey, GlyphInfo>> pendingVectorGlyphs = {};
  std::vector<std::pair<GlyphKey, GlyphInfo>> pendingBitmapGlyphs = {};

  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> vectorGlyphMapping = {};
  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> bitmapGlyphMapping = {};
};

bool FontEmbedder::Embed(PAGXDocument* document, const TextGlyphs& textGlyphs) {
  FontEmbedderImpl impl(document);
  return impl.embed(textGlyphs);
}

}  // namespace pagx
