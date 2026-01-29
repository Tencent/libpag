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

// Scales RGBA8888 pixels using area averaging (box filter) for high-quality downscaling.
static void ScalePixelsAreaAverage(const uint8_t* srcPixels, int srcW, int srcH, size_t srcRowBytes,
                                   uint8_t* dstPixels, int dstW, int dstH, size_t dstRowBytes) {
  float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
  float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);

  for (int dstY = 0; dstY < dstH; dstY++) {
    float srcY0 = static_cast<float>(dstY) * scaleY;
    float srcY1 = static_cast<float>(dstY + 1) * scaleY;
    int y0 = static_cast<int>(std::floor(srcY0));
    int y1 = static_cast<int>(std::ceil(srcY1));
    y0 = std::max(0, std::min(y0, srcH - 1));
    y1 = std::max(0, std::min(y1, srcH));

    auto* dstRow = dstPixels + dstY * dstRowBytes;

    for (int dstX = 0; dstX < dstW; dstX++) {
      float srcX0 = static_cast<float>(dstX) * scaleX;
      float srcX1 = static_cast<float>(dstX + 1) * scaleX;
      int x0 = static_cast<int>(std::floor(srcX0));
      int x1 = static_cast<int>(std::ceil(srcX1));
      x0 = std::max(0, std::min(x0, srcW - 1));
      x1 = std::max(0, std::min(x1, srcW));

      float sumR = 0, sumG = 0, sumB = 0, sumA = 0;
      float totalWeight = 0;

      for (int sy = y0; sy < y1; sy++) {
        float wy = 1.0f;
        if (sy == y0) {
          wy = 1.0f - (srcY0 - static_cast<float>(y0));
        }
        if (sy == y1 - 1 && y1 > y0 + 1) {
          wy = srcY1 - static_cast<float>(y1 - 1);
        }

        for (int sx = x0; sx < x1; sx++) {
          float wx = 1.0f;
          if (sx == x0) {
            wx = 1.0f - (srcX0 - static_cast<float>(x0));
          }
          if (sx == x1 - 1 && x1 > x0 + 1) {
            wx = srcX1 - static_cast<float>(x1 - 1);
          }

          float weight = wx * wy;
          const auto* p = srcPixels + sy * srcRowBytes + sx * 4;
          sumR += static_cast<float>(p[0]) * weight;
          sumG += static_cast<float>(p[1]) * weight;
          sumB += static_cast<float>(p[2]) * weight;
          sumA += static_cast<float>(p[3]) * weight;
          totalWeight += weight;
        }
      }

      if (totalWeight > 0) {
        dstRow[dstX * 4 + 0] = static_cast<uint8_t>(std::round(sumR / totalWeight));
        dstRow[dstX * 4 + 1] = static_cast<uint8_t>(std::round(sumG / totalWeight));
        dstRow[dstX * 4 + 2] = static_cast<uint8_t>(std::round(sumB / totalWeight));
        dstRow[dstX * 4 + 3] = static_cast<uint8_t>(std::round(sumA / totalWeight));
      }
    }
  }
}

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

struct GlyphKey {
  const tgfx::Typeface* typeface = nullptr;
  int fontSize = 0;
  tgfx::GlyphID glyphID = 0;

  bool operator==(const GlyphKey& other) const {
    return typeface == other.typeface && fontSize == other.fontSize && glyphID == other.glyphID;
  }
};

struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const {
    size_t h = std::hash<const void*>()(key.typeface);
    h ^= std::hash<int>()(key.fontSize) << 1;
    h ^= std::hash<uint16_t>()(key.glyphID) << 2;
    return h;
  }
};

struct GlyphInfo {
  std::string pathString = {};
  std::shared_ptr<tgfx::ImageCodec> imageCodec = nullptr;
  tgfx::Matrix imageMatrix = {};
};

static void CollectGlyphs(
    const std::shared_ptr<tgfx::TextBlob>& textBlob,
    std::vector<std::pair<GlyphKey, GlyphInfo>>& pendingVectorGlyphs,
    std::vector<std::pair<GlyphKey, GlyphInfo>>& pendingBitmapGlyphs,
    std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& vectorGlyphMapping,
    std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& bitmapGlyphMapping) {
  for (const auto& run : *textBlob) {
    auto* typeface = run.font.getTypeface().get();
    int fontSize = static_cast<int>(run.font.getSize());

    for (size_t i = 0; i < run.glyphCount; ++i) {
      tgfx::GlyphID glyphID = run.glyphs[i];
      GlyphKey key = {typeface, fontSize, glyphID};

      if (vectorGlyphMapping.find(key) != vectorGlyphMapping.end() ||
          bitmapGlyphMapping.find(key) != bitmapGlyphMapping.end()) {
        continue;
      }

      tgfx::Path glyphPath = {};
      bool hasPath = run.font.getPath(glyphID, &glyphPath) && !glyphPath.isEmpty();

      tgfx::Matrix imageMatrix = {};
      auto imageCodec = run.font.getImage(glyphID, nullptr, &imageMatrix);

      if (hasPath) {
        GlyphInfo info = {};
        info.pathString = PathToSVGString(glyphPath);
        if (!info.pathString.empty()) {
          pendingVectorGlyphs.push_back({key, info});
        }
      } else if (imageCodec) {
        GlyphInfo info = {};
        info.imageCodec = imageCodec;
        info.imageMatrix = imageMatrix;
        pendingBitmapGlyphs.push_back({key, info});
      }
    }
  }
}

static Glyph* CreateBitmapGlyph(PAGXDocument* document, const GlyphInfo& info) {
  auto imageCodec = info.imageCodec;
  auto imageMatrix = info.imageMatrix;

  int srcW = imageCodec->width();
  int srcH = imageCodec->height();
  float scaleX = std::abs(imageMatrix.getScaleX());
  float scaleY = std::abs(imageMatrix.getScaleY());
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

  std::shared_ptr<tgfx::Data> pngData = nullptr;

  if (dstW == srcW && dstH == srcH) {
    pngData = srcBitmap.encode(tgfx::EncodedFormat::PNG, 100);
  } else {
    tgfx::Bitmap dstBitmap(dstW, dstH, false, false);
    if (dstBitmap.isEmpty()) {
      return nullptr;
    }

    auto* dstPixels = dstBitmap.lockPixels();
    if (dstPixels == nullptr) {
      return nullptr;
    }
    auto* srcReadPixels = static_cast<const uint8_t*>(srcBitmap.lockPixels());
    ScalePixelsAreaAverage(srcReadPixels, srcW, srcH, srcBitmap.info().rowBytes(),
                           static_cast<uint8_t*>(dstPixels), dstW, dstH,
                           dstBitmap.info().rowBytes());
    srcBitmap.unlockPixels();
    dstBitmap.unlockPixels();

    pngData = dstBitmap.encode(tgfx::EncodedFormat::PNG, 100);
  }

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

static GlyphRun* CreateGlyphRunForIndices(
    PAGXDocument* document, const tgfx::GlyphRun& run, const std::vector<size_t>& indices,
    Font* font, const std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash>& mapping,
    const tgfx::Typeface* typeface, int fontSize) {
  auto glyphRun = document->makeNode<GlyphRun>();
  glyphRun->font = font;

  for (size_t i : indices) {
    GlyphKey key = {typeface, fontSize, run.glyphs[i]};
    auto it = mapping.find(key);
    if (it != mapping.end()) {
      glyphRun->glyphs.push_back(it->second);
    } else {
      glyphRun->glyphs.push_back(0);
    }
  }

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

bool FontEmbedder::embed(PAGXDocument* document, const TextGlyphs& textGlyphs) {
  if (document == nullptr) {
    return false;
  }

  std::vector<std::pair<GlyphKey, GlyphInfo>> pendingVectorGlyphs = {};
  std::vector<std::pair<GlyphKey, GlyphInfo>> pendingBitmapGlyphs = {};
  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> vectorGlyphMapping = {};
  std::unordered_map<GlyphKey, tgfx::GlyphID, GlyphKeyHash> bitmapGlyphMapping = {};

  // First pass: collect all glyphs from all TextBlobs
  for (const auto& [text, textBlob] : textGlyphs.textBlobs) {
    if (textBlob != nullptr) {
      CollectGlyphs(textBlob, pendingVectorGlyphs, pendingBitmapGlyphs, vectorGlyphMapping,
                    bitmapGlyphMapping);
    }
  }

  // Create Font nodes
  Font* vectorFont = nullptr;
  Font* bitmapFont = nullptr;

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

  if (!pendingBitmapGlyphs.empty()) {
    bitmapFont = document->makeNode<Font>("bitmap_font");
    tgfx::GlyphID nextID = 1;
    for (auto& [key, info] : pendingBitmapGlyphs) {
      auto glyph = CreateBitmapGlyph(document, info);
      if (glyph != nullptr) {
        bitmapFont->glyphs.push_back(glyph);
        bitmapGlyphMapping[key] = nextID++;
      }
    }
  }

  // Second pass: create GlyphRuns for each Text
  for (const auto& [text, textBlob] : textGlyphs.textBlobs) {
    if (textBlob == nullptr) {
      continue;
    }

    text->glyphRuns.clear();

    for (const auto& run : *textBlob) {
      if (run.glyphCount == 0) {
        continue;
      }

      auto* typeface = run.font.getTypeface().get();
      int fontSize = static_cast<int>(run.font.getSize());

      std::vector<size_t> vectorIndices = {};
      std::vector<size_t> bitmapIndices = {};

      for (size_t i = 0; i < run.glyphCount; ++i) {
        GlyphKey key = {typeface, fontSize, run.glyphs[i]};
        if (vectorGlyphMapping.find(key) != vectorGlyphMapping.end()) {
          vectorIndices.push_back(i);
        } else if (bitmapGlyphMapping.find(key) != bitmapGlyphMapping.end()) {
          bitmapIndices.push_back(i);
        }
      }

      if (!vectorIndices.empty() && vectorFont != nullptr) {
        auto glyphRun = CreateGlyphRunForIndices(document, run, vectorIndices, vectorFont,
                                                 vectorGlyphMapping, typeface, fontSize);
        if (glyphRun != nullptr) {
          text->glyphRuns.push_back(glyphRun);
        }
      }

      if (!bitmapIndices.empty() && bitmapFont != nullptr) {
        auto glyphRun = CreateGlyphRunForIndices(document, run, bitmapIndices, bitmapFont,
                                                 bitmapGlyphMapping, typeface, fontSize);
        if (glyphRun != nullptr) {
          text->glyphRuns.push_back(glyphRun);
        }
      }
    }
  }

  return true;
}

}  // namespace pagx
