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

#include "pagx/utils/ExporterUtils.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/Base64.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Shader.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

static const uint8_t kPNGSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
    if (info.fill && info.stroke && info.textBox) {
      break;
    }
  }
  return info;
}

Matrix BuildLayerMatrix(const Layer* layer) {
  Matrix m = layer->matrix;
  if (layer->x != 0.0f || layer->y != 0.0f) {
    m = Matrix::Translate(layer->x, layer->y) * m;
  }
  return m;
}

Matrix BuildGroupMatrix(const Group* group) {
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(group->position.x) || !FloatNearlyZero(group->position.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    shear.c = std::tan(DegreesToRadians(group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(group->position.x, group->position.y) * m;
  }

  return m;
}

std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphPath> result;
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float currentX = textPosX + run->x;
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (!glyph || !glyph->path || glyph->path->isEmpty()) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      if (i < run->positions.size()) {
        posX = textPosX + run->x + run->positions[i].x;
        posY = textPosY + run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
      } else if (i < run->xOffsets.size()) {
        posX = textPosX + run->x + run->xOffsets[i];
        posY = textPosY + run->y;
      } else {
        posX = currentX;
        posY = textPosY + run->y;
      }
      currentX += glyph->advance * scale;

      Matrix glyphMatrix = Matrix::Translate(posX, posY) * Matrix::Scale(scale, scale);

      bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
      bool hasGlyphScale =
          i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
      bool hasSkew = i < run->skews.size() && run->skews[i] != 0;

      if (hasRotation || hasGlyphScale || hasSkew) {
        float anchorX = glyph->advance * 0.5f;
        float anchorY = 0;
        if (i < run->anchors.size()) {
          anchorX += run->anchors[i].x;
          anchorY += run->anchors[i].y;
        }

        Matrix perGlyph = Matrix::Translate(-anchorX, -anchorY);
        if (hasGlyphScale) {
          perGlyph = Matrix::Scale(run->scales[i].x, run->scales[i].y) * perGlyph;
        }
        if (hasSkew) {
          Matrix shear = {};
          shear.c = std::tan(pag::DegreesToRadians(run->skews[i]));
          perGlyph = shear * perGlyph;
        }
        if (hasRotation) {
          perGlyph = Matrix::Rotate(run->rotations[i]) * perGlyph;
        }
        perGlyph = Matrix::Translate(anchorX, anchorY) * perGlyph;
        glyphMatrix = glyphMatrix * perGlyph;
      }

      result.push_back({glyphMatrix, glyph->path});
    }
  }
  return result;
}

FillRule DetectMaskFillRule(const Layer* maskLayer) {
  for (const auto* element : maskLayer->contents) {
    if (element->nodeType() == NodeType::Fill) {
      auto rule = static_cast<const Fill*>(element)->fillRule;
      if (rule == FillRule::EvenOdd) {
        return FillRule::EvenOdd;
      }
    }
  }
  for (const auto* child : maskLayer->children) {
    if (DetectMaskFillRule(child) == FillRule::EvenOdd) {
      return FillRule::EvenOdd;
    }
  }
  return FillRule::Winding;
}

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 24) {
    return false;
  }
  if (memcmp(data, kPNGSignature, 8) != 0) {
    return false;
  }
  *width = static_cast<int>((data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19]);
  *height = static_cast<int>((data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23]);
  return *width > 0 && *height > 0;
}

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height) {
  if (path.rfind("data:", 0) == 0) {
    auto decoded = DecodeBase64DataURI(path);
    if (!decoded) {
      return false;
    }
    return GetPNGDimensions(decoded->bytes(), decoded->size(), width, height);
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  uint8_t header[24];
  if (!file.read(reinterpret_cast<char*>(header), 24)) {
    return false;
  }
  return GetPNGDimensions(header, 24, width, height);
}

bool GetImagePNGDimensions(const Image* image, int* width, int* height) {
  if (image->data) {
    return GetPNGDimensions(image->data->bytes(), image->data->size(), width, height);
  }
  if (!image->filePath.empty()) {
    return GetPNGDimensionsFromPath(image->filePath, width, height);
  }
  return false;
}

bool GetJPEGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segmentLength = static_cast<size_t>((data[offset + 2] << 8) | data[offset + 3]) + 2;
    // SOF0..SOF3 markers contain image dimensions.
    if (marker >= 0xC0 && marker <= 0xC3 && offset + 9 < size) {
      *height = static_cast<int>((data[offset + 5] << 8) | data[offset + 6]);
      *width = static_cast<int>((data[offset + 7] << 8) | data[offset + 8]);
      return *width > 0 && *height > 0;
    }
    offset += segmentLength;
  }
  return false;
}

bool GetWebPDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 16 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) {
    return false;
  }
  if (size < 20) {
    return false;
  }
  const uint8_t* chunk = data + 12;
  size_t remaining = size - 12;
  if (remaining >= 18 && memcmp(chunk, "VP8 ", 4) == 0) {
    const uint8_t* vp8 = chunk + 8;
    size_t vp8Size = remaining - 8;
    if (vp8Size >= 10 && vp8[3] == 0x9D && vp8[4] == 0x01 && vp8[5] == 0x2A) {
      *width = static_cast<int>((vp8[6] | (vp8[7] << 8)) & 0x3FFF);
      *height = static_cast<int>((vp8[8] | (vp8[9] << 8)) & 0x3FFF);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 13 && memcmp(chunk, "VP8L", 4) == 0) {
    const uint8_t* vp8l = chunk + 8;
    size_t vp8lSize = remaining - 8;
    if (vp8lSize >= 5 && vp8l[0] == 0x2F) {
      uint32_t bits = vp8l[1] | (static_cast<uint32_t>(vp8l[2]) << 8) |
                      (static_cast<uint32_t>(vp8l[3]) << 16) |
                      (static_cast<uint32_t>(vp8l[4]) << 24);
      *width = static_cast<int>((bits & 0x3FFF) + 1);
      *height = static_cast<int>(((bits >> 14) & 0x3FFF) + 1);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 18 && memcmp(chunk, "VP8X", 4) == 0) {
    const uint8_t* vp8x = chunk + 8;
    *width = static_cast<int>((vp8x[4] | (vp8x[5] << 8) | (vp8x[6] << 16)) + 1);
    *height = static_cast<int>((vp8x[7] | (vp8x[8] << 8) | (vp8x[9] << 16)) + 1);
    return *width > 0 && *height > 0;
  }
  return false;
}

bool GetImageDimensions(const Image* image, int* width, int* height) {
  if (GetImagePNGDimensions(image, width, height)) {
    return true;
  }
  auto data = GetImageData(image);
  if (data && data->size() > 0) {
    if (GetJPEGDimensions(data->bytes(), data->size(), width, height)) {
      return true;
    }
    return GetWebPDimensions(data->bytes(), data->size(), width, height);
  }
  return false;
}

static bool GetPNGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 8 || memcmp(data, kPNGSignature, 8) != 0) {
    return false;
  }
  size_t offset = 8;
  while (offset + 12 <= size) {
    auto chunkLen = static_cast<uint32_t>((data[offset] << 24) | (data[offset + 1] << 16) |
                                          (data[offset + 2] << 8) | data[offset + 3]);
    if (memcmp(data + offset + 4, "pHYs", 4) == 0) {
      if (chunkLen == 9 && offset + 8 + 9 <= size) {
        const uint8_t* d = data + offset + 8;
        auto ppuX = static_cast<uint32_t>((d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3]);
        auto ppuY = static_cast<uint32_t>((d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]);
        uint8_t unit = d[8];
        if (unit == 1 && ppuX > 0 && ppuY > 0) {
          *dpiX = static_cast<float>(ppuX) * 0.0254f;
          *dpiY = static_cast<float>(ppuY) * 0.0254f;
          return true;
        }
      }
      return false;
    }
    if (memcmp(data + offset + 4, "IDAT", 4) == 0) {
      break;
    }
    offset += 12 + chunkLen;
  }
  return false;
}

static bool GetJPEGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segLen = static_cast<uint16_t>((data[offset + 2] << 8) | data[offset + 3]);
    if (marker == 0xE0 && segLen >= 16 && offset + 2 + segLen <= size) {
      if (memcmp(data + offset + 4, "JFIF\0", 5) == 0) {
        uint8_t units = data[offset + 11];
        auto xDensity = static_cast<uint16_t>((data[offset + 12] << 8) | data[offset + 13]);
        auto yDensity = static_cast<uint16_t>((data[offset + 14] << 8) | data[offset + 15]);
        if (xDensity > 0 && yDensity > 0) {
          if (units == 1) {
            *dpiX = static_cast<float>(xDensity);
            *dpiY = static_cast<float>(yDensity);
            return true;
          } else if (units == 2) {
            *dpiX = static_cast<float>(xDensity) * 2.54f;
            *dpiY = static_cast<float>(yDensity) * 2.54f;
            return true;
          }
        }
      }
    }
    offset += 2 + segLen;
  }
  return false;
}

bool GetImageDPI(const Image* image, float* dpiX, float* dpiY) {
  auto data = GetImageData(image);
  if (!data || data->size() == 0) {
    return false;
  }
  if (GetPNGDPI(data->bytes(), data->size(), dpiX, dpiY)) {
    return true;
  }
  return GetJPEGDPI(data->bytes(), data->size(), dpiX, dpiY);
}

bool IsJPEG(const uint8_t* data, size_t size) {
  return size >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

std::shared_ptr<tgfx::Data> GetImageData(const Image* image) {
  if (image->data) {
    return tgfx::Data::MakeWithoutCopy(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return tgfx::Data::MakeFromFile(image->filePath);
  }
  return nullptr;
}

bool HasNonASCII(const std::string& str) {
  for (unsigned char c : str) {
    if (c > 127) {
      return true;
    }
  }
  return false;
}

std::string UTF8ToUTF16BEHex(const std::string& utf8) {
  std::string hex;
  hex.reserve(utf8.size() * 6);
  size_t i = 0;
  while (i < utf8.size()) {
    uint32_t cp = 0;
    auto c = static_cast<unsigned char>(utf8[i]);
    size_t bytes = 1;
    if (c < 0x80) {
      cp = c;
    } else if (c < 0xE0) {
      cp = c & 0x1Fu;
      bytes = 2;
    } else if (c < 0xF0) {
      cp = c & 0x0Fu;
      bytes = 3;
    } else {
      cp = c & 0x07u;
      bytes = 4;
    }
    for (size_t j = 1; j < bytes && (i + j) < utf8.size(); j++) {
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + j]) & 0x3Fu);
    }
    i += bytes;
    char buf[9];
    if (cp <= 0xFFFF) {
      snprintf(buf, sizeof(buf), "%04X", cp);
    } else {
      cp -= 0x10000;
      snprintf(buf, sizeof(buf), "%04X%04X", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
    }
    hex += buf;
  }
  return hex;
}

static std::shared_ptr<tgfx::Data> DoRenderMaskedLayer(
    tgfx::Context* context, const std::shared_ptr<tgfx::Layer>& root,
    const std::shared_ptr<tgfx::Layer>& targetLayer, const tgfx::Rect& globalBounds) {
  int width = static_cast<int>(ceilf(globalBounds.width()));
  int height = static_cast<int>(ceilf(globalBounds.height()));
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto surface = tgfx::Surface::Make(context, width, height);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  canvas->translate(-globalBounds.left, -globalBounds.top);
  canvas->concat(targetLayer->getRelativeMatrix(root.get()));
  targetLayer->draw(canvas);

  tgfx::Bitmap bitmap(width, height, false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}

std::shared_ptr<tgfx::Data> RenderMaskedLayer(const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer) {
  auto globalBounds = targetLayer->getBounds(root.get(), true);
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto result = DoRenderMaskedLayer(context, root, targetLayer, globalBounds);
  device->unlock();
  return result;
}

static tgfx::TileMode ToTGFXTileMode(TileMode mode) {
  switch (mode) {
    case TileMode::Repeat:
      return tgfx::TileMode::Repeat;
    case TileMode::Mirror:
      return tgfx::TileMode::Mirror;
    case TileMode::Decal:
      return tgfx::TileMode::Decal;
    default:
      return tgfx::TileMode::Clamp;
  }
}

static std::shared_ptr<tgfx::Data> DoRenderTiledPattern(tgfx::Context* context,
                                                        std::shared_ptr<tgfx::Shader> shader,
                                                        int width, int height) {
  auto surface = tgfx::Surface::Make(context, width, height);
  if (!surface) {
    return nullptr;
  }
  tgfx::Paint paint;
  paint.setShader(std::move(shader));
  surface->getCanvas()->drawRect(tgfx::Rect::MakeWH(width, height), paint);
  tgfx::Bitmap bitmap(width, height, false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}

std::shared_ptr<tgfx::Data> RenderTiledPattern(const ImagePattern* pattern, int width, int height,
                                               float offsetX, float offsetY) {
  if (width <= 0 || height <= 0 || !pattern || !pattern->image) {
    return nullptr;
  }
  auto imageData = GetImageData(pattern->image);
  if (!imageData) {
    return nullptr;
  }
  auto tgfxImage = tgfx::Image::MakeFromEncoded(imageData);
  if (!tgfxImage) {
    return nullptr;
  }
  auto shader = tgfx::Shader::MakeImageShader(std::move(tgfxImage),
                                              ToTGFXTileMode(pattern->tileModeX),
                                              ToTGFXTileMode(pattern->tileModeY));
  if (!shader) {
    return nullptr;
  }
  const auto& M = pattern->matrix;
  tgfx::Matrix shaderMatrix = tgfx::Matrix::MakeAll(M.a, M.c, offsetX, M.b, M.d, offsetY);
  shader = shader->makeWithMatrix(shaderMatrix);
  if (!shader) {
    return nullptr;
  }
  auto device = tgfx::GLDevice::Make();
  if (!device) {
    return nullptr;
  }
  auto context = device->lockContext();
  if (!context) {
    return nullptr;
  }
  auto result = DoRenderTiledPattern(context, std::move(shader), width, height);
  device->unlock();
  return result;
}

std::string StripQuotes(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace pagx
