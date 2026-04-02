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
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/Base64.h"
#include "tgfx/core/Data.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

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
  static const uint8_t kPNGSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
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

bool GetImageDimensions(const Image* image, int* width, int* height) {
  if (GetImagePNGDimensions(image, width, height)) {
    return true;
  }
  auto data = GetImageData(image);
  if (data && data->size() > 0) {
    return GetJPEGDimensions(data->bytes(), data->size(), width, height);
  }
  return false;
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

}  // namespace pagx
