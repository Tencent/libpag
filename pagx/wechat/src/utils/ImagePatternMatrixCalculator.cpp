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
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"

namespace pagx {

// customData key used to persist the original paint transform exported from the source file.
// ResolveImagePatternMatrix() overwrites pattern->matrix with the baked result, so we stash the
// original 6-float affine here on first call to make the whole routine idempotent: later calls
// (for example after progressive image upgrades replace the thumbnail with a higher-resolution
// full image in the node's runtimeImage) can re-bake against the correct source matrix instead
// of re-baking the already baked matrix.
static constexpr const char* kPaintTransformKey = "paint-transform";

// Same idempotency stash but for the "new PAGX standard format" path: the exporter already
// bakes pattern->matrix in original-image pixel coordinates so we cache that authored matrix
// before applying the progressive-image post-scale. Distinct key from kPaintTransformKey so a
// document that mixes legacy and new patterns (only theoretical, but cheap to support) keeps
// the two caches independent.
static constexpr const char* kAuthoredMatrixKey = "authored-pattern-matrix";

// Threshold for treating a 2x2 determinant as singular (non-invertible). Values below this
// produce numerically unstable inverses under float32 precision (~7 significant digits).
static constexpr float SINGULAR_DETERMINANT_EPSILON = 1e-6f;

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

// Stretch the image to exactly fill the node by scaling each axis independently. Callers must
// guarantee imageWidth/imageHeight are positive (the public entry point checks this upfront).
static void ApplyStretchToNode(pagx::Matrix* matrix, float imageWidth, float imageHeight,
                               float nodeWidth, float nodeHeight) {
  matrix->a = nodeWidth / imageWidth;
  matrix->d = nodeHeight / imageHeight;
}

pagx::Matrix CalculateImagePatternMatrix(ImageScaleMode scaleMode, float imageWidth, float imageHeight,
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
        if (std::abs(det) > SINGULAR_DETERMINANT_EPSILON) {
          float invDet = 1.0f / det;
          matrix.a  =  s11 * invDet;
          matrix.c  = -s01 * invDet;
          matrix.b  = -s10 * invDet;
          matrix.d  =  s00 * invDet;
          matrix.tx = (s01 * s12 - s11 * s02) * invDet;
          matrix.ty = (s10 * s02 - s00 * s12) * invDet;
        } else {
          // Singular paint transform cannot be inverted; fall back to the same node/image
          // proportional stretch used for the identity-paintTransform path instead of leaving
          // the matrix at its identity initial value.
          ApplyStretchToNode(&matrix, imageWidth, imageHeight, nodeWidth, nodeHeight);
        }
      } else {
        ApplyStretchToNode(&matrix, imageWidth, imageHeight, nodeWidth, nodeHeight);
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

// Returns the actual pixel dimensions of the image currently bound to the pattern, falling
// back through the same source priority used by LayerBuilder::getOrCreateImage() so the matrix
// stays consistent with the asset that will actually sample at draw time:
//   1. overrideWidth/overrideHeight, when positive — supplied by the caller that just uploaded
//      a fresh texture and wants the matrix refreshed before the new image is installed on the
//      node via loadFileData.
//   2. runtimeImage on the Image node (set by PAGXDocument::loadFileData(path, PAGImage)).
//   3. data - encoded bytes; peeked via ImageCodec::MakeFrom without a full decode.
// fallbackWidth/fallbackHeight are returned untouched when none of the sources resolves
// to a positive dimension; callers pass either origImageWidth (legacy path) or 0 (new-format
// path) depending on what they want to fall back to.
static void ReadActualImageDimensions(const pagx::Image* imageNode,
                                      float overrideWidth, float overrideHeight,
                                      float fallbackWidth, float fallbackHeight,
                                      float* outWidth, float* outHeight) {
  *outWidth = fallbackWidth;
  *outHeight = fallbackHeight;
  if (overrideWidth > 0.0f && overrideHeight > 0.0f) {
    *outWidth = overrideWidth;
    *outHeight = overrideHeight;
    return;
  }
  if (!imageNode) {
    return;
  }
  auto runtimeImage = pagx::LayerBuilder::GetNodeRuntimeImage(imageNode);
  auto tgfxImage = LayerBuilder::GetTGFXImage(runtimeImage);
  if (tgfxImage && tgfxImage->width() > 0 && tgfxImage->height() > 0) {
    *outWidth = static_cast<float>(tgfxImage->width());
    *outHeight = static_cast<float>(tgfxImage->height());
    return;
  }
  if (imageNode->data && imageNode->data->size() > 0) {
    auto tgfxData = tgfx::Data::MakeWithoutCopy(imageNode->data->data(), imageNode->data->size());
    auto codec = tgfx::ImageCodec::MakeFrom(tgfxData);
    if (codec && codec->width() > 0 && codec->height() > 0) {
      *outWidth = static_cast<float>(codec->width());
      *outHeight = static_cast<float>(codec->height());
    }
  }
}

// Resolve path for the new PAGX standard format: pattern->matrix is already authored against
// original-image pixel coordinates, so we only need to undo any scaling between origSize and
// the actually attached image's pixel size. Returns true when handled (matrix possibly
// rewritten), false to defer to the legacy customData-driven path.
//
// origSize source priority:
//   1. pattern->customData["orig-image-width"/"orig-image-height"] - written by the new
//      exporter alongside the baked matrix. When present, the SDK is fully self-sufficient
//      and does not need any host-side runtime registration.
//   2. origSizeMap[filePath] - registered by the host via setImageOriginalSize(), used as a
//      fallback for documents exported by older tools that bake the matrix but do not write
//      the orig-image-* fields into customData.
static bool ResolveNewFormatPattern(pagx::ImagePattern* pattern,
                                    const ImageOriginalSizeMap* origSizeMap,
                                    float overrideWidth, float overrideHeight) {
  if (!pattern->image) {
    return false;
  }
  float origWidth = 0.0f;
  float origHeight = 0.0f;
  auto oiwIt = pattern->customData.find("orig-image-width");
  auto oihIt = pattern->customData.find("orig-image-height");
  if (oiwIt != pattern->customData.end() && oihIt != pattern->customData.end()) {
    origWidth = std::strtof(oiwIt->second.c_str(), nullptr);
    origHeight = std::strtof(oihIt->second.c_str(), nullptr);
  }
  if ((origWidth <= 0.0f || origHeight <= 0.0f) && origSizeMap) {
    const auto& filePath = pattern->image->filePath;
    if (!filePath.empty()) {
      auto sizeIt = origSizeMap->find(filePath);
      if (sizeIt != origSizeMap->end()) {
        origWidth = sizeIt->second.first;
        origHeight = sizeIt->second.second;
      }
    }
  }
  if (origWidth <= 0.0f || origHeight <= 0.0f) {
    return false;
  }

  // Cache the authored matrix on first call so re-resolves (post thumbnail->full upgrade) start
  // from the original authored matrix rather than re-scaling an already-scaled result.
  pagx::Matrix authoredMatrix = {};
  auto cacheIt = pattern->customData.find(kAuthoredMatrixKey);
  if (cacheIt != pattern->customData.end() &&
      DeserializePaintTransform(cacheIt->second, &authoredMatrix)) {
    // Already cached; pattern->matrix may hold a previously baked variant, so use the cache.
  } else {
    authoredMatrix = pattern->matrix;
    pattern->customData[kAuthoredMatrixKey] = SerializePaintTransform(authoredMatrix);
  }

  float actualWidth = 0.0f;
  float actualHeight = 0.0f;
  ReadActualImageDimensions(pattern->image, overrideWidth, overrideHeight, origWidth, origHeight,
                            &actualWidth, &actualHeight);

  if (actualWidth <= 0.0f || actualHeight <= 0.0f ||
      (actualWidth == origWidth && actualHeight == origHeight)) {
    // No usable actual size, or attached image already matches origSize. Either way the
    // authored matrix is correct as-is.
    pattern->matrix = authoredMatrix;
    return true;
  }

  // pattern->matrix maps points expressed in original-image pixels to the geometry's local
  // space. The actually attached texture has actualSize pixels along each axis but the same
  // visible content area, so we pre-multiply the matrix by diag(origW/actualW, origH/actualH).
  // After the post-scale, mapping a point p_actual in the attached texture's pixel space goes:
  //   p_geom = authoredMatrix * diag(origW/actualW, origH/actualH) * p_actual
  // which equals what the exporter intended (authoredMatrix applied to the equivalent original-
  // image-pixel coordinate).
  pagx::Matrix scale = {};
  scale.a = origWidth / actualWidth;
  scale.d = origHeight / actualHeight;

  pattern->matrix = authoredMatrix * scale;
  return true;
}

bool ResolveImagePatternMatrix(pagx::ImagePattern* pattern,
                               const ImageOriginalSizeMap* origSizeMap,
                               float overrideWidth, float overrideHeight) {
  if (!pattern || !pattern->image) {
    return false;
  }

  auto scaleModeIt = pattern->customData.find("image-scale-mode");
  if (scaleModeIt == pattern->customData.end()) {
    // No legacy customData: try the new PAGX standard format. When that path is also unable to
    // produce a meaningful result (e.g. origSize hasn't been registered yet), leave the
    // authored matrix unchanged and report unresolved.
    return ResolveNewFormatPattern(pattern, origSizeMap, overrideWidth, overrideHeight);
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

  // Parse actual image dimensions from the best available source; fall back to origImage* when
  // no decoded asset has arrived yet (placeholder layout during progressive loading).
  float actualImageWidth = 0.0f;
  float actualImageHeight = 0.0f;
  ReadActualImageDimensions(pattern->image, overrideWidth, overrideHeight, origImageWidth,
                            origImageHeight, &actualImageWidth, &actualImageHeight);

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

  pattern->matrix = CalculateImagePatternMatrix(
      scaleMode, actualImageWidth, actualImageHeight,
      nodeWidth, nodeHeight, paintTransform, scaleFactor,
      origImageWidth, origImageHeight);

  return true;
}

void ResolveAllImagePatternMatrices(pagx::PAGXDocument* document,
                                    const ImageOriginalSizeMap* origSizeMap) {
  if (!document) {
    return;
  }
  for (const auto& nodePtr : document->nodes) {
    if (!nodePtr || nodePtr->nodeType() != pagx::NodeType::ImagePattern) {
      continue;
    }
    auto* pattern = static_cast<pagx::ImagePattern*>(nodePtr.get());
    ResolveImagePatternMatrix(pattern, origSizeMap);
  }
}

size_t ResolveImagePatternMatricesByFilePath(pagx::PAGXDocument* document,
                                             const std::string& filePath,
                                             const ImageOriginalSizeMap* origSizeMap,
                                             float overrideWidth, float overrideHeight) {
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
    if (ResolveImagePatternMatrix(pattern, origSizeMap, overrideWidth, overrideHeight)) {
      ++updated;
    }
  }
  return updated;
}

}  // namespace pagx
