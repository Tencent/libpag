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

#include "ContentViewModel.h"
#include <algorithm>
#include <cmath>

namespace pag {

static constexpr double MIN_ZOOM = 0.001;
static constexpr double MAX_ZOOM = 1000.0;
static constexpr double TRANSFORM_EPSILON = 0.001;

double ContentViewModel::zoomScale() const {
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  return viewTransform.zoomScale;
}

ContentViewModel::ViewTransform ContentViewModel::getViewTransform() const {
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  return viewTransform;
}

void ContentViewModel::zoomAt(double factor, double anchorX, double anchorY) {
  double newZoomScale = 1.0;
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    newZoomScale = std::clamp(viewTransform.zoomScale * factor, MIN_ZOOM, MAX_ZOOM);
    if (newZoomScale == viewTransform.zoomScale) {
      return;
    }
    auto appliedFactor = newZoomScale / viewTransform.zoomScale;
    viewTransform.offsetX = (viewTransform.offsetX - anchorX) * appliedFactor + anchorX;
    viewTransform.offsetY = (viewTransform.offsetY - anchorY) * appliedFactor + anchorY;
    viewTransform.zoomScale = newZoomScale;
  }
  notifyViewTransformChanged(newZoomScale);
}

void ContentViewModel::panBy(double deltaX, double deltaY) {
  if (std::abs(deltaX) < TRANSFORM_EPSILON && std::abs(deltaY) < TRANSFORM_EPSILON) {
    return;
  }
  double currentZoomScale = 1.0;
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    viewTransform.offsetX += deltaX;
    viewTransform.offsetY += deltaY;
    currentZoomScale = viewTransform.zoomScale;
  }
  notifyViewTransformChanged(currentZoomScale);
}

void ContentViewModel::resetView() {
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    viewTransform = {};
  }
  notifyViewTransformChanged(1.0);
}

void ContentViewModel::adjustForSurfaceResize(double oldSurfaceWidth, double oldSurfaceHeight,
                                              double newSurfaceWidth, double newSurfaceHeight,
                                              double contentWidth, double contentHeight) {
  if (contentWidth <= 0 || contentHeight <= 0 || oldSurfaceWidth <= 0 || oldSurfaceHeight <= 0 ||
      newSurfaceWidth <= 0 || newSurfaceHeight <= 0) {
    return;
  }
  auto oldContentScale = std::min(oldSurfaceWidth / contentWidth, oldSurfaceHeight / contentHeight);
  auto newContentScale = std::min(newSurfaceWidth / contentWidth, newSurfaceHeight / contentHeight);
  auto oldContentOffsetX = (oldSurfaceWidth - contentWidth * oldContentScale) * 0.5;
  auto oldContentOffsetY = (oldSurfaceHeight - contentHeight * oldContentScale) * 0.5;
  auto newContentOffsetX = (newSurfaceWidth - contentWidth * newContentScale) * 0.5;
  auto newContentOffsetY = (newSurfaceHeight - contentHeight * newContentScale) * 0.5;
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    if (viewTransform.zoomScale == 1.0 && viewTransform.offsetX == 0.0 &&
        viewTransform.offsetY == 0.0) {
      return;
    }
    viewTransform.offsetX += (oldContentOffsetX - newContentOffsetX) * viewTransform.zoomScale;
    viewTransform.offsetY += (oldContentOffsetY - newContentOffsetY) * viewTransform.zoomScale;
  }
}

void ContentViewModel::notifyViewTransformChanged(double newZoomScale) {
  onViewTransformChanged();
  Q_EMIT zoomScaleChanged(newZoomScale);
  Q_EMIT requestFlush();
}

}  // namespace pag
