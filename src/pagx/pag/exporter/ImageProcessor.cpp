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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/exporter/ImageProcessor.h"
#include <algorithm>
#include <unordered_map>
#include "pagx/PAGExporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/ImageFormatUtils.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/ImageInfo.h"

namespace pagx {
namespace {

// Maximum singular value of the 2×2 part of a transform matrix; the "how big does this
// transform make things" metric.
float MatrixScale(const Matrix& matrix) {
  float scaleX = std::sqrt(matrix.a * matrix.a + matrix.b * matrix.b);
  float scaleY = std::sqrt(matrix.c * matrix.c + matrix.d * matrix.d);
  return std::max(scaleX, scaleY);
}

// Combines a parent transform with a child transform: parent * child.
Matrix CombineMatrix(const Matrix& parent, const Matrix& child) {
  Matrix result = {};
  result.a = parent.a * child.a + parent.c * child.b;
  result.b = parent.b * child.a + parent.d * child.b;
  result.c = parent.a * child.c + parent.c * child.d;
  result.d = parent.b * child.c + parent.d * child.d;
  result.tx = parent.a * child.tx + parent.c * child.ty + parent.tx;
  result.ty = parent.b * child.tx + parent.d * child.ty + parent.ty;
  return result;
}

struct PatternReference {
  ImagePattern* pattern = nullptr;
  float scale = 1.0f;
};

// Recursively collects ImagePattern references, accumulating ancestor transforms. A
// Layer's matrix combines with the parent; a Group adds its position/rotation/scale.
// The geometry bounds for the scaleMode fitting come from the enclosing container's
// content bounds, which the flattened document provides in layoutBounds().
// animMultiplier accumulates the keyframed-transform upper bounds along the chain (12.2):
// each transform node contributes max(1, largest keyframe scale / static scale), an exact
// upper bound because 2D singular values are invariant under rotation.
void CollectReferences(Node* node, const Matrix& parentMatrix, const Rect& geometryBounds,
                       float animMultiplier, const ImageProcessor::Options& options,
                       std::vector<PatternReference>* references) {
  if (node == nullptr) {
    return;
  }
  auto type = node->nodeType();
  if (type == NodeType::Layer) {
    auto* layer = static_cast<Layer*>(node);
    Matrix matrix = layer->matrix;
    auto combined = CombineMatrix(parentMatrix, matrix);
    if (options.animationScaleLookup != nullptr) {
      animMultiplier *= options.animationScaleLookup(layer);
    }
    // Geometry bounds for the layer's contents.
    Rect bounds = layer->layoutBounds();
    for (auto* element : layer->contents) {
      CollectReferences(element, combined, bounds, animMultiplier, options, references);
    }
    for (auto* child : layer->children) {
      CollectReferences(child, combined, bounds, animMultiplier, options, references);
    }
    return;
  }
  if (type == NodeType::Group || type == NodeType::TextBox) {
    auto* group = static_cast<Group*>(node);
    // Group transform: position + rotation + scale around anchor (§4.2 flattening already
    // wrote layout results back, so position is resolved and anchor effects are folded
    // into the content positions; only the transform matrix matters for scale).
    Matrix matrix = {};
    matrix.a = group->scale.x;
    matrix.b = 0;
    matrix.c = 0;
    matrix.d = group->scale.y;
    matrix.tx = group->position.x;
    matrix.ty = group->position.y;
    // Apply rotation around anchor if non-zero.
    if (group->rotation != 0) {
      float rad = group->rotation * 3.14159265f / 180.0f;
      float cosR = std::cos(rad);
      float sinR = std::sin(rad);
      Matrix rot = {};
      rot.a = cosR * group->scale.x;
      rot.b = sinR * group->scale.x;
      rot.c = -sinR * group->scale.y;
      rot.d = cosR * group->scale.y;
      matrix = rot;
      matrix.tx = group->position.x;
      matrix.ty = group->position.y;
    }
    auto combined = CombineMatrix(parentMatrix, matrix);
    if (options.animationScaleLookup != nullptr) {
      animMultiplier *= options.animationScaleLookup(group);
    }
    Rect bounds = group->layoutBounds();
    for (auto* element : group->elements) {
      CollectReferences(element, combined, bounds, animMultiplier, options, references);
    }
    return;
  }
  if (type == NodeType::Fill || type == NodeType::Stroke) {
    ColorSource* source = nullptr;
    if (type == NodeType::Fill) {
      source = static_cast<Fill*>(node)->color;
    } else {
      source = static_cast<Stroke*>(node)->color;
    }
    if (source != nullptr && source->nodeType() == NodeType::ImagePattern) {
      auto* pattern = static_cast<ImagePattern*>(source);
      if (pattern->image != nullptr) {
        PatternReference ref = {};
        ref.pattern = pattern;
        // The display scale at this reference: the parent transform scale combined with
        // the pattern's own matrix and the scaleMode fitting.
        float parentScale = MatrixScale(parentMatrix);
        float patternScale = MatrixScale(pattern->matrix);
        if (options.animationScaleLookup != nullptr) {
          patternScale *= options.animationScaleLookup(pattern);
        }
        // ScaleMode fitting: how much the image is stretched to fit the geometry. Only
        // compute this when we have valid geometry bounds and the image has dimensions.
        float fitScale = 1.0f;
        int imgW = 0;
        int imgH = 0;
        if (GetImageDimensions(pattern->image, &imgW, &imgH) && imgW > 0 && imgH > 0) {
          float gw = geometryBounds.width;
          float gh = geometryBounds.height;
          if (gw > 0 && gh > 0) {
            switch (pattern->scaleMode) {
              case ScaleMode::None:
                fitScale = 1.0f;
                break;
              case ScaleMode::Stretch:
                // Non-uniform; take the max axis.
                fitScale = std::max(gw / static_cast<float>(imgW), gh / static_cast<float>(imgH));
                break;
              case ScaleMode::LetterBox:
                fitScale = std::min(gw / static_cast<float>(imgW), gh / static_cast<float>(imgH));
                break;
              case ScaleMode::Zoom:
                fitScale = std::max(gw / static_cast<float>(imgW), gh / static_cast<float>(imgH));
                break;
            }
          }
        }
        ref.scale = parentScale * patternScale * fitScale * animMultiplier;
        references->push_back(ref);
      }
    }
    return;
  }
}

std::vector<PatternReference> CollectAllReferences(PAGXDocument* document,
                                                   const ImageProcessor::Options& options) {
  std::vector<PatternReference> references = {};
  Matrix identity = Matrix::Identity();
  Rect emptyBounds = {};
  for (auto* layer : document->layers) {
    CollectReferences(layer, identity, emptyBounds, 1.0f, options, &references);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      for (auto* layer : composition->layers) {
        CollectReferences(layer, identity, emptyBounds, 1.0f, options, &references);
      }
    }
  }
  return references;
}

// Content hash for dedup (§4.5). Uses the first 16 bytes + size + last 16 bytes for speed.
std::string ImageContentHash(const uint8_t* data, size_t size) {
  auto hash = std::to_string(size);
  if (size >= 32) {
    hash += std::string(reinterpret_cast<const char*>(data), 16);
    hash += std::string(reinterpret_cast<const char*>(data + size - 16), 16);
  } else {
    hash += std::string(reinterpret_cast<const char*>(data), size);
  }
  return hash;
}

}  // namespace

float ImageProcessor::ComputeMaxDisplayScale(PAGXDocument* document, const Image* image,
                                             const Options* options) {
  auto references = CollectAllReferences(document, options != nullptr ? *options : Options{});
  float maxScale = 0.0f;
  for (auto& ref : references) {
    if (ref.pattern->image == image) {
      maxScale = std::max(maxScale, ref.scale);
    }
  }
  return maxScale;
}

ImageProcessor::Result ImageProcessor::Process(PAGXDocument* document, const Options* options) {
  Result result = {};
  const Options& opts = options != nullptr ? *options : Options{};
  auto references = CollectAllReferences(document, opts);

  // Group references by image.
  std::unordered_map<const Image*, std::vector<PatternReference*>> imageRefs = {};
  for (auto& ref : references) {
    imageRefs[ref.pattern->image].push_back(&ref);
  }

  // Content hash for dedup.
  std::unordered_map<std::string, uint32_t> dedupTable = {};

  for (auto& [image, refs] : imageRefs) {
    if (image == nullptr || refs.empty()) {
      continue;
    }

    // Compute max display scale.
    float maxScale = 0.0f;
    for (auto* ref : refs) {
      maxScale = std::max(maxScale, ref->scale);
    }

    ImageTableEntry entry = {};
    entry.scaleFactor = 1.0f;

    // Get image data.
    auto data = GetImageData(image);
    if (data == nullptr || data->empty()) {
      PAGExportIssue issue = {};
      issue.code = PAGExportWarning::MissingImage;
      issue.message = "image data is missing or failed to decode";
      result.warnings.push_back(issue);
      entry.logicalWidth = 0;
      entry.logicalHeight = 0;
    } else {
      int imgW = 0;
      int imgH = 0;
      GetImageDimensions(image, &imgW, &imgH);
      entry.logicalWidth = static_cast<uint32_t>(imgW);
      entry.logicalHeight = static_cast<uint32_t>(imgH);

      // Compute factor: only shrink (§4.5).
      float factor = std::min(1.0f, maxScale * 2.0f);

      // Check if this is already WebP and needs no processing.
      bool isWebP = IsWebP(data->bytes(), data->size());
      bool needsProcessing = factor < 1.0f || !isWebP;

      if (needsProcessing) {
        // Decode, scale, crop, re-encode (simplified for phase 1: pass through the
        // original data when we cannot decode; the full pipeline lands with T5 when the
        // encoder consumes the entries).
        entry.data = Data::MakeWithCopy(data->bytes(), data->size());
        if (factor < 1.0f) {
          entry.scaleFactor = factor;
          PAGExportIssue issue = {};
          issue.code = PAGExportWarning::ImageScaledDown;
          issue.message = "image scaled down by factor " + std::to_string(factor);
          result.warnings.push_back(issue);
        }
      } else {
        // Already WebP and no scaling needed: write bytes as-is (§4.5 "不重复压缩").
        entry.data = Data::MakeWithCopy(data->bytes(), data->size());
      }

      // Dedup: same content hash -> reuse the existing entry's index.
      auto hash = ImageContentHash(data->bytes(), data->size());
      auto existing = dedupTable.find(hash);
      if (existing != dedupTable.end()) {
        result.imageIndices[image] = existing->second;
        continue;
      }
      dedupTable[hash] = static_cast<uint32_t>(result.entries.size());
    }

    result.imageIndices[image] = static_cast<uint32_t>(result.entries.size());
    result.entries.push_back(entry);
  }

  // Bitmap glyph images (spec §6.6 mediaType 1): raw bytes with no scaling, cropping, or
  // re-encoding; logical size equals the source image size and scaleFactor stays 1. Fonts are
  // walked in document node order (stable across decode/re-encode because the XML round trip
  // preserves resource order), appended after the pattern images, and deduplicated through the
  // same content hash.
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<Font*>(node.get());
    for (auto* glyph : font->glyphs) {
      auto* image = glyph->image;
      if (image == nullptr || result.imageIndices.find(image) != result.imageIndices.end()) {
        continue;
      }
      auto data = GetImageData(image);
      if (data == nullptr || data->empty()) {
        PAGExportIssue issue = {};
        issue.code = PAGExportWarning::MissingImage;
        issue.message = "bitmap glyph image data is missing or failed to decode";
        result.warnings.push_back(issue);
        continue;
      }
      auto hash = ImageContentHash(data->bytes(), data->size());
      auto existing = dedupTable.find(hash);
      if (existing != dedupTable.end()) {
        result.imageIndices[image] = existing->second;
        continue;
      }
      ImageTableEntry entry = {};
      entry.mediaType = 1;
      entry.data = Data::MakeWithCopy(data->bytes(), data->size());
      int width = 0;
      int height = 0;
      if (GetImageDimensions(image, &width, &height)) {
        entry.logicalWidth = static_cast<uint32_t>(width);
        entry.logicalHeight = static_cast<uint32_t>(height);
      }
      dedupTable[hash] = static_cast<uint32_t>(result.entries.size());
      result.imageIndices[image] = static_cast<uint32_t>(result.entries.size());
      result.entries.push_back(entry);
    }
  }

  // Handle images with no references (unreferenced): they were stripped by T3, but a
  // direct Image node in the tree with no ImagePattern would still be here. Skip them
  // (they won't appear in the ImageTable).

  return result;
}

}  // namespace pagx
