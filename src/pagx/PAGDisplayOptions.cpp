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

#include "pagx/PAGDisplayOptions.h"
#include <utility>
#include "renderer/ToTGFX.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {
namespace {

tgfx::RenderMode ToTGFX(PAGRenderMode renderMode) {
  switch (renderMode) {
    case PAGRenderMode::Direct:
      return tgfx::RenderMode::Direct;
    case PAGRenderMode::Partial:
      return tgfx::RenderMode::Partial;
    case PAGRenderMode::Tiled:
      return tgfx::RenderMode::Tiled;
  }
  return tgfx::RenderMode::Partial;
}

PAGRenderMode ToPAGX(tgfx::RenderMode renderMode) {
  switch (renderMode) {
    case tgfx::RenderMode::Direct:
      return PAGRenderMode::Direct;
    case tgfx::RenderMode::Partial:
      return PAGRenderMode::Partial;
    case tgfx::RenderMode::Tiled:
      return PAGRenderMode::Tiled;
  }
  return PAGRenderMode::Partial;
}

Color ToPAGX(const tgfx::Color& color) {
  return {color.red, color.green, color.blue, color.alpha, ColorSpace::SRGB};
}

}  // namespace

struct PAGDisplayOptions::Impl {
  explicit Impl(void* displayList) : displayList(static_cast<tgfx::DisplayList*>(displayList)) {
  }

  tgfx::DisplayList* displayList = nullptr;
  bool showDirtyRegions = false;
};

std::unique_ptr<PAGDisplayOptions> PAGDisplayOptions::Make(void* displayList) {
  return std::unique_ptr<PAGDisplayOptions>(
      new PAGDisplayOptions(std::make_unique<Impl>(displayList)));
}

PAGDisplayOptions::PAGDisplayOptions(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {
}

PAGDisplayOptions::~PAGDisplayOptions() = default;

float PAGDisplayOptions::getZoomScale() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->zoomScale() : 1.0f;
}

void PAGDisplayOptions::setZoomScale(float zoomScale) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setZoomScale(zoomScale);
  }
}

int PAGDisplayOptions::getZoomScalePrecision() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->zoomScalePrecision()
                                                         : 1000;
}

void PAGDisplayOptions::setZoomScalePrecision(int precision) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setZoomScalePrecision(precision);
  }
}

Point PAGDisplayOptions::getContentOffset() const {
  if (impl == nullptr || impl->displayList == nullptr) {
    return {};
  }
  auto offset = impl->displayList->contentOffset();
  return {offset.x, offset.y};
}

void PAGDisplayOptions::setContentOffset(float offsetX, float offsetY) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setContentOffset(offsetX, offsetY);
  }
}

PAGRenderMode PAGDisplayOptions::getRenderMode() const {
  return impl != nullptr && impl->displayList != nullptr ? ToPAGX(impl->displayList->renderMode())
                                                         : PAGRenderMode::Partial;
}

void PAGDisplayOptions::setRenderMode(PAGRenderMode renderMode) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setRenderMode(ToTGFX(renderMode));
  }
}

int PAGDisplayOptions::getTileSize() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->tileSize() : 256;
}

void PAGDisplayOptions::setTileSize(int tileSize) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setTileSize(tileSize);
  }
}

int PAGDisplayOptions::getMaxTileCount() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->maxTileCount() : 0;
}

void PAGDisplayOptions::setMaxTileCount(int count) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setMaxTileCount(count);
  }
}

bool PAGDisplayOptions::getAllowZoomBlur() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->allowZoomBlur()
                                                         : false;
}

void PAGDisplayOptions::setAllowZoomBlur(bool allow) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setAllowZoomBlur(allow);
  }
}

int PAGDisplayOptions::getMaxTilesRefinedPerFrame() const {
  return impl != nullptr && impl->displayList != nullptr
             ? impl->displayList->maxTilesRefinedPerFrame()
             : 5;
}

void PAGDisplayOptions::setMaxTilesRefinedPerFrame(int count) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setMaxTilesRefinedPerFrame(count);
  }
}

Color PAGDisplayOptions::getBackgroundColor() const {
  return impl != nullptr && impl->displayList != nullptr
             ? ToPAGX(impl->displayList->backgroundColor())
             : Color{0, 0, 0, 0, ColorSpace::SRGB};
}

void PAGDisplayOptions::setBackgroundColor(const Color& color) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setBackgroundColor(ToTGFX(color));
  }
}

int PAGDisplayOptions::getSubtreeCacheMaxSize() const {
  return impl != nullptr && impl->displayList != nullptr ? impl->displayList->subtreeCacheMaxSize()
                                                         : 0;
}

void PAGDisplayOptions::setSubtreeCacheMaxSize(int maxSize) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->setSubtreeCacheMaxSize(maxSize);
  }
}

bool PAGDisplayOptions::getShowDirtyRegions() const {
  return impl != nullptr ? impl->showDirtyRegions : false;
}

void PAGDisplayOptions::setShowDirtyRegions(bool show) {
  if (impl != nullptr && impl->displayList != nullptr) {
    impl->displayList->showDirtyRegions(show);
    impl->showDirtyRegions = show;
  }
}

}  // namespace pagx
