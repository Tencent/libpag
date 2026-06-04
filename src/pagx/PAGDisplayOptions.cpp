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
#include "pagx/PAGScene.h"

// The fallback values used when getDisplayList() returns nullptr must match the default member
// values in tgfx::DisplayList (tgfx/include/tgfx/layers/DisplayList.h). Update this file if the
// tgfx defaults change.
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {
namespace {

PAGRenderMode ToPAGX(tgfx::RenderMode renderMode) {
  switch (renderMode) {
    case tgfx::RenderMode::Direct:
      return PAGRenderMode::Direct;
    case tgfx::RenderMode::Partial:
      return PAGRenderMode::Partial;
    case tgfx::RenderMode::Tiled:
      return PAGRenderMode::Tiled;
  }
}

Color ToPAGX(const tgfx::Color& color) {
  return {color.red, color.green, color.blue, color.alpha, ColorSpace::SRGB};
}

}  // namespace

PAGDisplayOptions::PAGDisplayOptions(PAGScene* scene) : scene(scene) {
}

PAGDisplayOptions::~PAGDisplayOptions() = default;

tgfx::DisplayList* PAGDisplayOptions::getDisplayList() const {
  return scene != nullptr ? scene->getDisplayListForOptions() : nullptr;
}

float PAGDisplayOptions::getZoomScale() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->zoomScale() : 1.0f;
}

void PAGDisplayOptions::setZoomScale(float zoomScale) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setZoomScale(zoomScale);
  }
}

int PAGDisplayOptions::getZoomScalePrecision() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->zoomScalePrecision() : 1000;
}

void PAGDisplayOptions::setZoomScalePrecision(int precision) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setZoomScalePrecision(precision);
  }
}

Point PAGDisplayOptions::getContentOffset() const {
  auto displayList = getDisplayList();
  if (displayList == nullptr) {
    return {};
  }
  auto offset = displayList->contentOffset();
  return {offset.x, offset.y};
}

void PAGDisplayOptions::setContentOffset(float offsetX, float offsetY) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setContentOffset(offsetX, offsetY);
  }
}

PAGRenderMode PAGDisplayOptions::getRenderMode() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? ToPAGX(displayList->renderMode()) : PAGRenderMode::Partial;
}

void PAGDisplayOptions::setRenderMode(PAGRenderMode renderMode) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setRenderMode(ToTGFX(renderMode));
  }
}

int PAGDisplayOptions::getTileSize() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->tileSize() : 256;
}

void PAGDisplayOptions::setTileSize(int tileSize) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setTileSize(tileSize);
  }
}

int PAGDisplayOptions::getMaxTileCount() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->maxTileCount() : 0;
}

void PAGDisplayOptions::setMaxTileCount(int count) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setMaxTileCount(count);
  }
}

bool PAGDisplayOptions::getAllowZoomBlur() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->allowZoomBlur() : false;
}

void PAGDisplayOptions::setAllowZoomBlur(bool allow) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setAllowZoomBlur(allow);
  }
}

int PAGDisplayOptions::getMaxTilesRefinedPerFrame() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->maxTilesRefinedPerFrame() : 5;
}

void PAGDisplayOptions::setMaxTilesRefinedPerFrame(int count) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setMaxTilesRefinedPerFrame(count);
  }
}

Color PAGDisplayOptions::getBackgroundColor() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? ToPAGX(displayList->backgroundColor())
                                : Color{0, 0, 0, 0, ColorSpace::SRGB};
}

void PAGDisplayOptions::setBackgroundColor(const Color& color) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setBackgroundColor(ToTGFX(color));
  }
}

int PAGDisplayOptions::getSubtreeCacheMaxSize() const {
  auto displayList = getDisplayList();
  return displayList != nullptr ? displayList->subtreeCacheMaxSize() : 0;
}

void PAGDisplayOptions::setSubtreeCacheMaxSize(int maxSize) {
  auto displayList = getDisplayList();
  if (displayList != nullptr) {
    displayList->setSubtreeCacheMaxSize(maxSize);
  }
}

}  // namespace pagx
