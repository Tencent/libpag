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

#include "ImagePatternMatrixCalculator.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "pagx/nodes/Image.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"

namespace pagx {

// customData key used to persist the original paint transform exported from the source file.
// resolveImagePatternMatrix() overwrites pattern->matrix with the baked result, so we stash the
// original 6-float affine here on first call to make the whole routine idempotent: later calls
// (for example after progressive image upgrades replace decodedImage with a higher-resolution
// tgfx::Image) can re-bake against the correct source matrix instead of re-baking the already
// baked matrix.
static constexpr const char* kPaintTransformKey = "paint-transform";

static std::string SerializePaintTransform(const pagx::Matrix& matrix) {
  char buf[128] = {};
  std::snprintf(buf, sizeof(buf), "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g", matrix.a, matrix.b, matrix.c,
                matrix.d, matrix.tx, matrix.ty);
  return buf;
}

static bool DeserializePaintTransform(const std::string& text, pagx::Matrix* out) {
  float values[6] = {};
  const char* cursor = text.c_str();
  for (int i = 0; i < 6; ++i) {
    char* end = nullptr;
    values[i] = std::strtof(cursor, &end);
    if (end == cursor) {
      return false;
    }
    cursor = end;
    // Skip the separator ',' between fields; the last field has no trailing comma.
    if (i < 5) {
      while (*cursor == ' ' || *cursor == ',') {
        ++cursor;
      }
    }
  }
  out->a = values[0];
  out->b = values[1];
  out->c = values[2];
  out->d = values[3];
  out->tx = values[4];
  out->ty = values[5];
  return true;
}

pagx::Matrix calculateImagePatternMatrix(ImageScaleMode scaleMode, float imageWidth, float imageHeight,
                                         float nodeWidth, float nodeHeight, const pagx::Matrix& paintTransform,
                                         float scaleFactor, float origImageWidth, float origImageHeight) {
  pagx::Matrix matrix = {};
  matrix.a = 1.0f;
  matrix.d = 1.0f;

  if (imageWidth <= 0.0f || imageHeight <= 0.0f || nodeWidth <= 0.0f || nodeHeight <= 0.0f) {
    if (!paintTransform.isIdentity()) {
      return paintTransform;
    }
    return matrix;
  }

  float imageRatio = imageWidth / imageHeight;
  float nodeRatio = nodeWidth / nodeHeight;

  switch (scaleMode) {
    case ImageScaleMode::FILL: {
      float ratio = (imageRatio > nodeRatio) ? (nodeHeight / imageHeight) : (nodeWidth / imageWidth);
      float marginX = (nodeWidth - imageWidth * ratio) * 0.5f;
      float marginY = (nodeHeight - imageHeight * ratio) * 0.5f;
      matrix.a = ratio;
      matrix.d = ratio;
      matrix.tx = marginX;
      matrix.ty = marginY;
      break;
    }
    case ImageScaleMode::FIT: {
      float ratio = (nodeRatio > imageRatio) ? (nodeHeight / imageHeight) : (nodeWidth / imageWidth);
      float marginX = (nodeWidth - imageWidth * ratio) * 0.5f;
      float marginY = (nodeHeight - imageHeight * ratio) * 0.5f;
      matrix.a = ratio;
      matrix.d = ratio;
      matrix.tx = marginX;
      matrix.ty = marginY;
      break;
    }
    case ImageScaleMode::STRETCH: {
      // pagx::Matrix layout: | a  c  tx |
      //                        | b  d  ty |
      if (!paintTransform.isIdentity()) {
        float invNW = 1.0f / nodeWidth;
        float invNH = 1.0f / nodeHeight;
        float s00 = paintTransform.a * invNW, s01 = paintTransform.c * invNH, s02 = paintTransform.tx;
        float s10 = paintTransform.b * invNW, s11 = paintTransform.d * invNH, s12 = paintTransform.ty;

        s00 *= imageWidth;  s01 *= imageWidth;  s02 *= imageWidth;
        s10 *= imageHeight; s11 *= imageHeight; s12 *= imageHeight;

        float det = s00 * s11 - s01 * s10;
        if (std::abs(det) > 1e-10f) {
          float invDet = 1.0f / det;
          matrix.a  =  s11 * invDet;
          matrix.c  = -s01 * invDet;
          matrix.b  = -s10 * invDet;
          matrix.d  =  s00 * invDet;
          matrix.tx = (s01 * s12 - s11 * s02) * invDet;
          matrix.ty = (s10 * s02 - s00 * s12) * invDet;
        }
      } else {
        matrix.a = nodeWidth / imageWidth;
        matrix.d = nodeHeight / imageHeight;
      }
      break;
    }
    case ImageScaleMode::TILE: {
      float ratioX = scaleFactor;
      float ratioY = scaleFactor;
      if (origImageWidth > 0.0f) {
        ratioX = scaleFactor * origImageWidth / imageWidth;
      }
      if (origImageHeight > 0.0f) {
        ratioY = scaleFactor * origImageHeight / imageHeight;
      }
      matrix.a = ratioX;
      matrix.d = ratioY;
      break;
    }
    default:
      break;
  }

  return matrix;
}

bool resolveImagePatternMatrix(pagx::ImagePattern* pattern) {
  if (!pattern || !pattern->image) {
    return false;
  }

  auto scaleModeIt = pattern->customData.find("image-scale-mode");
  if (scaleModeIt == pattern->customData.end()) {
    return false;
  }

  char* end = nullptr;
  int scaleModeInt = static_cast<int>(std::strtol(scaleModeIt->second.c_str(), &end, 10));
  if (end == scaleModeIt->second.c_str()) {
    return false;
  }
  auto scaleMode = static_cast<ImageScaleMode>(scaleModeInt);

  float nodeWidth = 0.0f, nodeHeight = 0.0f;
  auto nwIt = pattern->customData.find("node-width");
  auto nhIt = pattern->customData.find("node-height");
  if (nwIt != pattern->customData.end()) {
    nodeWidth = std::strtof(nwIt->second.c_str(), nullptr);
  }
  if (nhIt != pattern->customData.end()) {
    nodeHeight = std::strtof(nhIt->second.c_str(), nullptr);
  }

  float origImageWidth = 0.0f, origImageHeight = 0.0f;
  auto oiwIt = pattern->customData.find("orig-image-width");
  auto oihIt = pattern->customData.find("orig-image-height");
  if (oiwIt != pattern->customData.end()) {
    origImageWidth = std::strtof(oiwIt->second.c_str(), nullptr);
  }
  if (oihIt != pattern->customData.end()) {
    origImageHeight = std::strtof(oihIt->second.c_str(), nullptr);
  }

  float scaleFactor = 0.5f;
  auto sfIt = pattern->customData.find("scale-factor");
  if (sfIt != pattern->customData.end()) {
    scaleFactor = std::strtof(sfIt->second.c_str(), nullptr);
  }

  // Parse actual image dimensions from the best available source. The priority mirrors the
  // fallback chain in LayerBuilder::getOrCreateImage() so the pattern matrix stays consistent
  // with what will actually be rendered:
  //   1. decodedImage: host-decoded full-quality tgfx::Image supplied via loadDecodedImage().
  //      Chosen first so progressive loading (e.g. thumbnail swapped for full-resolution)
  //      recomputes matrices off the highest-quality asset that will actually sample.
  //   2. thumbnailImage: lower-resolution backend texture written via
  //      loadDecodedImageAsThumbnail(). LayerBuilder falls back to this slot when
  //      decodedImage is absent, so the pattern matrix must be sized against thumbnail pixel
  //      dimensions in that window or the fill will sample outside the texture (visible as a
  //      blank rectangle clamped to the edge sample).
  //   3. data: encoded bytes. We peek the header via ImageCodec::MakeFrom without forcing a
  //      full decode.
  //   4. orig-image-* from customData: the dimensions recorded by the exporter. Used both when
  //      no pixels have arrived yet (placeholder layout during progressive loading) and as a
  //      fallback when decodedImage / thumbnailImage / data are all absent.
  float actualImageWidth = origImageWidth;
  float actualImageHeight = origImageHeight;
  auto* imageNode = pattern->image;
  if (imageNode) {
    if (imageNode->decodedImage && imageNode->decodedImage->width() > 0 &&
        imageNode->decodedImage->height() > 0) {
      actualImageWidth = static_cast<float>(imageNode->decodedImage->width());
      actualImageHeight = static_cast<float>(imageNode->decodedImage->height());
    } else if (imageNode->thumbnailImage && imageNode->thumbnailImage->width() > 0 &&
               imageNode->thumbnailImage->height() > 0) {
      actualImageWidth = static_cast<float>(imageNode->thumbnailImage->width());
      actualImageHeight = static_cast<float>(imageNode->thumbnailImage->height());
    } else if (imageNode->data && imageNode->data->size() > 0) {
      auto tgfxData = tgfx::Data::MakeWithoutCopy(imageNode->data->data(), imageNode->data->size());
      auto codec = tgfx::ImageCodec::MakeFrom(tgfxData);
      if (codec && codec->width() > 0 && codec->height() > 0) {
        actualImageWidth = static_cast<float>(codec->width());
        actualImageHeight = static_cast<float>(codec->height());
      }
    }
  }

  // The matrix stored during export is paint->transform() (normalized transform). After the
  // first successful resolve, pattern->matrix is overwritten with the baked result, so we keep
  // the original paint transform in customData to stay idempotent across re-resolves triggered
  // by progressive image upgrades.
  pagx::Matrix paintTransform = {};
  auto ptIt = pattern->customData.find(kPaintTransformKey);
  if (ptIt != pattern->customData.end() && DeserializePaintTransform(ptIt->second, &paintTransform)) {
    // Already cached; use the original paint transform and ignore pattern->matrix, which holds
    // a previously baked result.
  } else {
    paintTransform = pattern->matrix;
    pattern->customData[kPaintTransformKey] = SerializePaintTransform(paintTransform);
  }

  pattern->matrix = calculateImagePatternMatrix(
      scaleMode, actualImageWidth, actualImageHeight,
      nodeWidth, nodeHeight, paintTransform, scaleFactor,
      origImageWidth, origImageHeight);

  return true;
}

void resolveAllImagePatternMatrices(pagx::PAGXDocument* document) {
  if (!document) {
    return;
  }
  for (const auto& nodePtr : document->nodes) {
    if (!nodePtr || nodePtr->nodeType() != pagx::NodeType::ImagePattern) {
      continue;
    }
    auto* pattern = static_cast<pagx::ImagePattern*>(nodePtr.get());
    resolveImagePatternMatrix(pattern);
  }
}

size_t resolveImagePatternMatricesByFilePath(pagx::PAGXDocument* document,
                                             const std::string& filePath) {
  if (!document || filePath.empty()) {
    return 0;
  }
  size_t updated = 0;
  for (const auto& nodePtr : document->nodes) {
    if (!nodePtr || nodePtr->nodeType() != pagx::NodeType::ImagePattern) {
      continue;
    }
    auto* pattern = static_cast<pagx::ImagePattern*>(nodePtr.get());
    if (!pattern->image || pattern->image->filePath != filePath) {
      continue;
    }
    if (resolveImagePatternMatrix(pattern)) {
      ++updated;
    }
  }
  return updated;
}

}  // namespace pagx
