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

#include "pagx/utils/RasterUtils.h"
#include <cmath>
#include <vector>
#include "pagx/nodes/ImagePattern.h"
#include "pagx/types/TileMode.h"
#include "pagx/utils/ImageFormatUtils.h"
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

// Shared tail for off-screen render helpers: reads the surface contents into a
// tightly packed pixmap and encodes it as PNG. Returns nullptr if the backing
// bitmap could not be allocated or the surface read failed.
static std::shared_ptr<tgfx::Data> EncodeSurfaceToPNG(const std::shared_ptr<tgfx::Surface>& surface,
                                                      int width, int height) {
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

// Common skeleton shared by every off-screen GPU render path in this file.
// Validates the requested size, allocates a Surface at `width * pixelScale` by
// `height * pixelScale` pixels, pre-applies a matching scale on the Canvas so
// that drawer commands can stay in the caller's logical coordinate space, then
// encodes the surface as PNG. The encoded PNG is the scaled pixel size — the
// caller is responsible for placing it at the original logical extent so that
// the consumer stretches the higher-density bitmap back over the same visible
// area. `Drawer` must be callable as `drawer(tgfx::Canvas*)`; named functors
// defined below supply the actual draw logic (the project forbids lambdas).
template <typename Drawer>
static std::shared_ptr<tgfx::Data> RenderToPNG(tgfx::Context* context, int width, int height,
                                               float pixelScale, Drawer drawer) {
  if (width <= 0 || height <= 0 || pixelScale <= 0.0f) {
    return nullptr;
  }
  int pixelWidth = static_cast<int>(std::ceil(static_cast<float>(width) * pixelScale));
  int pixelHeight = static_cast<int>(std::ceil(static_cast<float>(height) * pixelScale));
  if (pixelWidth <= 0 || pixelHeight <= 0) {
    return nullptr;
  }
  auto surface = tgfx::Surface::Make(context, pixelWidth, pixelHeight);
  if (surface == nullptr) {
    return nullptr;
  }
  auto* canvas = surface->getCanvas();
  canvas->scale(pixelScale, pixelScale);
  drawer(canvas);
  return EncodeSurfaceToPNG(surface, pixelWidth, pixelHeight);
}

namespace {

// Draws `targetLayer` (and only that layer) into the off-screen canvas, with
// the global bounds origin mapped to (0,0) so the emitted PNG is tightly
// cropped. Used for mask / scrollRect bakes where the layer's visual result
// does not depend on the backdrop.
//
// Note: tgfx's Layer::draw deliberately does NOT apply the target layer's own
// scrollRect (see Layer.h doc on draw()) — only children's scrollRects are
// honoured during the recursive walk. Without this drawer applying the clip
// itself the baked PNG would contain the target's full unclipped content. The
// scrollRect lives in layer-local coordinates, so we clip after concatenating
// the layer's relative matrix while the canvas is still in layer-local space.
struct MaskedLayerDrawer {
  const std::shared_ptr<tgfx::Layer>& root;
  const std::shared_ptr<tgfx::Layer>& targetLayer;
  const tgfx::Rect& globalBounds;
  void operator()(tgfx::Canvas* canvas) const {
    canvas->translate(-globalBounds.left, -globalBounds.top);
    canvas->concat(targetLayer->getRelativeMatrix(root.get()));
    auto scrollRect = targetLayer->scrollRect();
    if (!scrollRect.isEmpty()) {
      canvas->clipRect(scrollRect, targetLayer->allowsEdgeAntialiasing());
    }
    targetLayer->draw(canvas);
  }
};

// Hides every layer that the BackdropCompositeDrawer must NOT bake into the PNG, restoring the
// original visibility flags on destruction. The drawer needs the target layer's backdrop (i.e.
// the pixels of layers drawn before it under the same root) but must exclude the target's later-
// drawn siblings as well as anything painted on top of the target itself — otherwise the bake
// captures content that the surrounding writeLayer recursion will also emit natively, producing
// duplicated / overlapping output.
//
// The mask is built by walking the chain root -> ... -> targetLayer's parent. At each ancestor,
// every child whose draw order is at-or-after the next link in the chain is hidden. The target
// itself is excluded from the hide set because the drawer still needs to paint it on top of the
// backdrop (that is, after all, the whole point of the backdrop composite).
class BackdropSiblingMask {
 public:
  BackdropSiblingMask(const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer) {
    if (!root || !targetLayer || root.get() == targetLayer.get()) {
      return;
    }
    std::vector<tgfx::Layer*> chain;
    for (auto* current = targetLayer.get(); current != nullptr; current = current->parent()) {
      chain.push_back(current);
      if (current == root.get()) {
        break;
      }
    }
    if (chain.empty() || chain.back() != root.get()) {
      // targetLayer is not a descendant of root; defensive bail-out.
      return;
    }
    for (size_t i = chain.size() - 1; i > 0; --i) {
      auto* ancestor = chain[i];
      auto* nextInChain = chain[i - 1];
      bool seenChainLink = false;
      for (const auto& child : ancestor->children()) {
        if (child.get() == nextInChain) {
          seenChainLink = true;
          continue;
        }
        if (!seenChainLink) {
          continue;
        }
        if (!child->visible()) {
          continue;
        }
        _hidden.push_back(child);
        child->setVisible(false);
      }
    }
  }

  ~BackdropSiblingMask() {
    for (const auto& layer : _hidden) {
      layer->setVisible(true);
    }
  }

  BackdropSiblingMask(const BackdropSiblingMask&) = delete;
  BackdropSiblingMask& operator=(const BackdropSiblingMask&) = delete;

 private:
  std::vector<std::shared_ptr<tgfx::Layer>> _hidden;
};

// Draws the entire scene from `root` downward into the off-screen canvas,
// clipped to the target layer's global bounds. Used when the target layer's
// visual result depends on the backdrop pixels below it — e.g. a non-Normal
// `Layer.blendMode`, which the tgfx renderer composites against whatever has
// already been drawn underneath. Drawing only the target against an empty
// canvas (as MaskedLayerDrawer does) would blend it with transparent-black
// and lose the intended composite; drawing `root` with a clip preserves it.
//
// `targetLayer` is the layer whose composite is being baked. The caller must arrange (via
// BackdropSiblingMask) for any ancestor sibling drawn AFTER the target chain to be invisible
// before this drawer runs, so the PNG only contains the backdrop + target instead of the
// surrounding native content the writer will emit separately.
struct BackdropCompositeDrawer {
  const std::shared_ptr<tgfx::Layer>& root;
  const tgfx::Rect& globalBounds;
  void operator()(tgfx::Canvas* canvas) const {
    canvas->translate(-globalBounds.left, -globalBounds.top);
    canvas->clipRect(globalBounds);
    root->draw(canvas);
  }
};

// Fills the entire canvas with a pre-built image shader to rasterize a tiled
// pattern into a PNG tile.
struct TiledPatternDrawer {
  std::shared_ptr<tgfx::Shader> shader;
  int width;
  int height;
  void operator()(tgfx::Canvas* canvas) const {
    tgfx::Paint paint;
    paint.setShader(shader);
    canvas->drawRect(tgfx::Rect::MakeWH(width, height), paint);
  }
};

}  // namespace

GPUContext::~GPUContext() {
  _device = nullptr;
}

tgfx::Context* GPUContext::lockContext() {
  if (!_device) {
    _device = tgfx::GLDevice::Make();
    if (!_device) {
      return nullptr;
    }
  }
  return _device->lockContext();
}

void GPUContext::unlock() {
  if (_device) {
    _device->unlock();
  }
}

tgfx::Rect ComputeRasterizedLayerBoundsInSpace(const std::shared_ptr<tgfx::Layer>& targetLayer,
                                               const tgfx::Layer* coordinateSpace) {
  auto bounds = targetLayer->getBounds(coordinateSpace, true);
  auto scrollRect = targetLayer->scrollRect();
  if (!scrollRect.isEmpty()) {
    // getRelativeMatrix already folds in scrollRect's preTranslate (see tgfx
    // Layer::getMatrixWithScrollRect), so mapping the scrollRect through it
    // yields the visible window in the target coordinate space regardless of
    // the layer's matrix or scroll offsets. getBounds however does not
    // intersect the layer's own scrollRect with its own bounds (only child
    // scrollRects are intersected during traversal), so we apply the clip here
    // to keep the rasterized extent in sync with what will actually be drawn.
    auto windowInSpace = targetLayer->getRelativeMatrix(coordinateSpace).mapRect(scrollRect);
    if (!bounds.intersect(windowInSpace)) {
      return tgfx::Rect::MakeEmpty();
    }
  }
  return bounds;
}

tgfx::Rect ComputeRasterizedLayerBounds(const std::shared_ptr<tgfx::Layer>& root,
                                        const std::shared_ptr<tgfx::Layer>& targetLayer) {
  return ComputeRasterizedLayerBoundsInSpace(targetLayer, root.get());
}

std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              float pixelScale) {
  auto globalBounds = ComputeRasterizedLayerBounds(root, targetLayer);
  if (globalBounds.isEmpty()) {
    return nullptr;
  }
  auto context = gpu->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  int width = static_cast<int>(ceilf(globalBounds.width()));
  int height = static_cast<int>(ceilf(globalBounds.height()));
  auto result = RenderToPNG(context, width, height, pixelScale,
                            MaskedLayerDrawer{root, targetLayer, globalBounds});
  gpu->unlock();
  return result;
}

namespace {

// Draws `targetLayer` (and only that layer) into the off-screen canvas using the supplied
// coordinate space rather than the document root. The bounds origin is mapped to (0,0) so the
// PNG is tightly cropped and can be placed at `bounds.left/top` inside any container whose
// transform chain matches `coordinateSpace`. Used by the SVG path where the enclosing `<g>`
// already carries the parent transform.
//
// Note: tgfx's Layer::draw deliberately does NOT apply the target layer's own scrollRect (see
// Layer.h doc on draw()) — only children's scrollRects are honoured during the recursive walk.
// This drawer applies the clip itself so the baked PNG honours the scrollRect window even when
// the visual content overflows it (e.g. a layer with both a scrollRect and SVG-unsupported
// features such as TextPath / TextModifier / Conic / Diamond gradient that triggers the SVG
// rasterize fallback). Without this clip the PNG would contain out-of-window pixels and the
// SVG `<image>` placement at `bounds` would still leak them since `writeLayer` returns early
// after a successful rasterize and skips the enclosing scrollRect `<g>` wrapper. The scrollRect
// lives in layer-local coordinates, so we clip after concatenating the layer's relative matrix
// while the canvas is still in layer-local space.
struct MaskedLayerDrawerInSpace {
  const std::shared_ptr<tgfx::Layer>& targetLayer;
  const tgfx::Layer* coordinateSpace;
  const tgfx::Rect& bounds;
  void operator()(tgfx::Canvas* canvas) const {
    canvas->translate(-bounds.left, -bounds.top);
    canvas->concat(targetLayer->getRelativeMatrix(coordinateSpace));
    auto scrollRect = targetLayer->scrollRect();
    if (!scrollRect.isEmpty()) {
      canvas->clipRect(scrollRect, targetLayer->allowsEdgeAntialiasing());
    }
    targetLayer->draw(canvas);
  }
};

}  // namespace

std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              const tgfx::Layer* targetCoordinateSpace,
                                              float pixelScale) {
  auto coordinateSpace = targetCoordinateSpace != nullptr ? targetCoordinateSpace : root.get();
  auto bounds = ComputeRasterizedLayerBoundsInSpace(targetLayer, coordinateSpace);
  if (bounds.isEmpty() || bounds.width() <= 0 || bounds.height() <= 0) {
    return nullptr;
  }
  auto context = gpu->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  int width = static_cast<int>(ceilf(bounds.width()));
  int height = static_cast<int>(ceilf(bounds.height()));
  auto result = RenderToPNG(context, width, height, pixelScale,
                            MaskedLayerDrawerInSpace{targetLayer, coordinateSpace, bounds});
  gpu->unlock();
  return result;
}

std::shared_ptr<tgfx::Data> RenderLayerCompositeWithBackdrop(
    GPUContext* gpu, const std::shared_ptr<tgfx::Layer>& root,
    const std::shared_ptr<tgfx::Layer>& targetLayer, float pixelScale) {
  auto globalBounds = ComputeRasterizedLayerBounds(root, targetLayer);
  if (globalBounds.isEmpty()) {
    return nullptr;
  }
  auto context = gpu->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  int width = static_cast<int>(ceilf(globalBounds.width()));
  int height = static_cast<int>(ceilf(globalBounds.height()));
  // Hide every layer that is drawn after the target along its ancestor chain so the bake captures
  // only the target plus its true backdrop (pixels drawn before it). Without this guard the
  // surrounding writer would re-emit those siblings on top of the PNG, double-painting them.
  BackdropSiblingMask hideMask(root, targetLayer);
  auto result =
      RenderToPNG(context, width, height, pixelScale, BackdropCompositeDrawer{root, globalBounds});
  gpu->unlock();
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

std::shared_ptr<tgfx::Data> RenderTiledPattern(GPUContext* gpu, const ImagePattern* pattern,
                                               int width, int height, float offsetX, float offsetY,
                                               float pixelScale) {
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
  auto shader = tgfx::Shader::MakeImageShader(
      std::move(tgfxImage), ToTGFXTileMode(pattern->tileModeX), ToTGFXTileMode(pattern->tileModeY));
  if (!shader) {
    return nullptr;
  }
  const auto& M = pattern->matrix;
  tgfx::Matrix shaderMatrix = tgfx::Matrix::MakeAll(M.a, M.c, offsetX, M.b, M.d, offsetY);
  shader = shader->makeWithMatrix(shaderMatrix);
  if (!shader) {
    return nullptr;
  }
  auto context = gpu->lockContext();
  if (!context) {
    return nullptr;
  }
  auto result = RenderToPNG(context, width, height, pixelScale,
                            TiledPatternDrawer{std::move(shader), width, height});
  gpu->unlock();
  return result;
}

}  // namespace pagx
