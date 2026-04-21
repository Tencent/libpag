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

#include "pagx/html/HTMLPlusDarkerRenderer.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Layer.h"
#include "pagx/utils/Base64.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

using pag::FloatNearlyZero;

namespace {

// Recursively walks the layer tree, filling parentMap (layer -> its parent, nullptr for roots) and
// collecting every layer whose blendMode is PlusDarker.
void CollectPlusDarker(const std::vector<Layer*>& layers, const Layer* parent,
                       std::unordered_map<const Layer*, const Layer*>& parentMap,
                       std::vector<Layer*>& outList) {
  for (auto* layer : layers) {
    if (!layer) {
      continue;
    }
    parentMap[layer] = parent;
    if (layer->blendMode == BlendMode::PlusDarker) {
      outList.push_back(layer);
    }
    CollectPlusDarker(layer->children, layer, parentMap, outList);
  }
}

// Rejects layers whose semantics the feImage+arithmetic filter cannot express faithfully. These
// fall back to the existing mix-blend-mode: darken approximation.
bool CanHandle(const Layer* layer) {
  if (!layer->filters.empty()) {
    return false;
  }
  if (layer->mask != nullptr) {
    return false;
  }
  if (layer->hasScrollRect) {
    return false;
  }
  if (!layer->matrix3D.isIdentity()) {
    return false;
  }
  // Only pure translation is supported; rotation/scale/skew would require transforming the filter
  // region and we don't do that this turn.
  const auto& m = layer->matrix;
  if (!FloatNearlyZero(m.a - 1.0f) || !FloatNearlyZero(m.d - 1.0f) || !FloatNearlyZero(m.b) ||
      !FloatNearlyZero(m.c)) {
    return false;
  }
  return true;
}

// Walks from `target` up its parent chain (using parentMap), accumulating layoutBounds offsets and
// pure-translation matrix components to produce the target's top-left in screen-space CSS pixels.
// Returns false when any ancestor carries a non-translation matrix (caller should fall back).
bool ComputeScreenBounds(const Layer* target,
                         const std::unordered_map<const Layer*, const Layer*>& parentMap,
                         Rect* outBounds) {
  Rect local = target->layoutBounds();
  if (local.width <= 0 || local.height <= 0) {
    return false;
  }
  float x = local.x;
  float y = local.y;
  // Layer's own matrix contributes pure translation (rotation/scale was already rejected by
  // CanHandle); add it to the screen offset.
  if (!target->matrix.isIdentity()) {
    x += target->matrix.tx;
    y += target->matrix.ty;
  }
  auto it = parentMap.find(target);
  const Layer* cur = it != parentMap.end() ? it->second : nullptr;
  while (cur != nullptr) {
    Rect pb = cur->layoutBounds();
    x += pb.x;
    y += pb.y;
    const auto& m = cur->matrix;
    if (!m.isIdentity()) {
      if (!FloatNearlyZero(m.a - 1.0f) || !FloatNearlyZero(m.d - 1.0f) || !FloatNearlyZero(m.b) ||
          !FloatNearlyZero(m.c)) {
        return false;
      }
      x += m.tx;
      y += m.ty;
    }
    if (!cur->matrix3D.isIdentity()) {
      return false;
    }
    auto pit = parentMap.find(cur);
    cur = pit != parentMap.end() ? pit->second : nullptr;
  }
  outBounds->x = x;
  outBounds->y = y;
  outBounds->width = local.width;
  outBounds->height = local.height;
  return true;
}

// Renders the whole document (with one Layer already flipped to visible=false) into a Surface
// sized to `bw*pixelRatio x bh*pixelRatio`, translated so that screen-space (bx, by) maps to (0,0)
// in the Surface. Writes the result to outputPath (PNG) and returns the raw PNG bytes so the
// caller can also embed them as a base64 data URL in the generated HTML.
bool RenderCroppedBackdrop(const PAGXDocument& doc, tgfx::Context* ctx, float bx, float by,
                           float bw, float bh, float pixelRatio, const std::string& outputPath,
                           std::shared_ptr<tgfx::Data>* outEncoded) {
  if (bw <= 0 || bh <= 0) {
    return false;
  }
  int pxW = static_cast<int>(std::ceil(bw * pixelRatio));
  int pxH = static_cast<int>(std::ceil(bh * pixelRatio));
  if (pxW <= 0 || pxH <= 0) {
    return false;
  }
  auto layer = LayerBuilder::Build(const_cast<PAGXDocument*>(&doc));
  if (!layer) {
    return false;
  }
  // Apply (scale by pixelRatio) composed with (translate by -bx,-by). Order matters: we want the
  // world-space point (bx, by) to land at Surface pixel (0, 0), then scale up.
  tgfx::Matrix transform = tgfx::Matrix::MakeScale(pixelRatio);
  transform.preTranslate(-bx, -by);
  layer->setMatrix(transform);

  auto surface = tgfx::Surface::Make(ctx, pxW, pxH);
  if (!surface) {
    return false;
  }
  tgfx::DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);

  tgfx::Bitmap bitmap(pxW, pxH, false, false);
  if (bitmap.isEmpty()) {
    return false;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return false;
  }
  auto encoded = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
  if (!encoded) {
    return false;
  }
  std::ofstream out(outputPath, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(encoded->data()),
            static_cast<std::streamsize>(encoded->size()));
  if (!out.good()) {
    return false;
  }
  *outEncoded = encoded;
  return true;
}

}  // namespace

void HTMLPlusDarkerRenderer::RenderAll(const PAGXDocument& doc, const std::string& staticImgDir,
                                       const std::string& /*urlPrefix*/,
                                       const std::string& staticImgNamePrefix, float pixelRatio,
                                       std::unordered_map<const Layer*, PlusDarkerBackdrop>& out) {
  std::unordered_map<const Layer*, const Layer*> parentMap;
  std::vector<Layer*> candidates;
  CollectPlusDarker(doc.layers, nullptr, parentMap, candidates);
  if (candidates.empty()) {
    return;
  }

  auto device = tgfx::GLDevice::Make();
  if (!device) {
    return;
  }
  auto ctx = device->lockContext();
  if (!ctx) {
    return;
  }

  std::filesystem::create_directories(staticImgDir);

  int idx = 0;
  for (Layer* target : candidates) {
    if (!CanHandle(target)) {
      continue;
    }
    Rect screenBounds = {};
    if (!ComputeScreenBounds(target, parentMap, &screenBounds)) {
      continue;
    }
    // Round outward to integer CSS pixels so the cropped PNG fully contains the layer area.
    float bx = std::floor(screenBounds.x);
    float by = std::floor(screenBounds.y);
    float bw = std::ceil(screenBounds.x + screenBounds.width) - bx;
    float bh = std::ceil(screenBounds.y + screenBounds.height) - by;
    if (bw <= 0 || bh <= 0) {
      continue;
    }

    // RAII guard: restore visibility no matter how we exit this iteration.
    struct VisibilityGuard {
      Layer* layer;
      bool previous;
      ~VisibilityGuard() {
        layer->visible = previous;
      }
    } guard{target, target->visible};
    target->visible = false;

    std::string fileName = staticImgNamePrefix + "pd_" + std::to_string(idx) + ".png";
    std::string absPath = staticImgDir + "/" + fileName;
    std::shared_ptr<tgfx::Data> encoded;
    if (!RenderCroppedBackdrop(doc, ctx, bx, by, bw, bh, pixelRatio, absPath, &encoded)) {
      continue;
    }

    PlusDarkerBackdrop entry;
    entry.filterId = "pagx_pd_" + std::to_string(idx);
    entry.backdropDataURL =
        "data:image/png;base64," + Base64Encode(encoded->bytes(), encoded->size());
    entry.cropLeft = bx;
    entry.cropTop = by;
    entry.cropWidth = bw;
    entry.cropHeight = bh;
    out[target] = std::move(entry);
    idx++;
  }

  device->unlock();
}

}  // namespace pagx
