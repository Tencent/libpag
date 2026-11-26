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

ContentFit ComputeContentFit(double contentWidth, double contentHeight, double surfaceWidth,
                             double surfaceHeight) {
  ContentFit fit = {};
  if (contentWidth <= 0 || contentHeight <= 0 || surfaceWidth <= 0 || surfaceHeight <= 0) {
    return fit;
  }
  fit.scale = std::min(surfaceWidth / contentWidth, surfaceHeight / contentHeight);
  fit.offsetX = (surfaceWidth - contentWidth * fit.scale) * 0.5;
  fit.offsetY = (surfaceHeight - contentHeight * fit.scale) * 0.5;
  return fit;
}

int64_t ComputeFrameCount(int64_t periodUs, double frameRate) {
  if (periodUs <= 0 || frameRate <= 0) {
    return 0;
  }
  return static_cast<int64_t>(std::round(static_cast<double>(periodUs) * frameRate / 1000000.0));
}

int64_t ComputeFrameIndex(double progress, int64_t totalFrames) {
  if (totalFrames <= 0) {
    return 0;
  }
  auto frame = static_cast<int64_t>(std::floor(progress * static_cast<double>(totalFrames)));
  return std::clamp(frame, int64_t{0}, totalFrames - 1);
}

double ContentViewModel::zoomScale() const {
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  return viewTransform.zoomScale;
}

ContentViewModel::ViewTransform ContentViewModel::getViewTransform() const {
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  return viewTransform;
}

void ContentViewModel::zoomAt(double factor, double anchorX, double anchorY,
                              const QSizeF& viewportSize) {
  auto contentWidth = static_cast<double>(getWidth());
  auto contentHeight = static_cast<double>(getHeight());
  if (viewportSize.isEmpty() || contentWidth <= 0 || contentHeight <= 0) {
    return;
  }
  double newZoomScale = 1.0;
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    // The surface size is unknown until the drawable exists. Treat the surface as the viewport in
    // that case so gestures keep working instead of being dropped without any feedback.
    auto surface = surfaceSize.isEmpty() ? viewportSize : surfaceSize;
    auto viewportScale =
        ComputeContentFit(contentWidth, contentHeight, viewportSize.width(), viewportSize.height())
            .scale;
    auto surfaceScale =
        ComputeContentFit(contentWidth, contentHeight, surface.width(), surface.height()).scale;
    auto ratio = viewportScale / surfaceScale;
    // Invert the centered surface-to-viewport mapping to keep the zoom anchor under the pointer.
    auto surfaceAnchorX =
        (anchorX - (viewportSize.width() - ratio * surface.width()) * 0.5) / ratio;
    auto surfaceAnchorY =
        (anchorY - (viewportSize.height() - ratio * surface.height()) * 0.5) / ratio;
    newZoomScale = std::clamp(viewTransform.zoomScale * factor, MIN_ZOOM, MAX_ZOOM);
    if (newZoomScale == viewTransform.zoomScale) {
      return;
    }
    auto appliedFactor = newZoomScale / viewTransform.zoomScale;
    viewTransform.offsetX =
        (viewTransform.offsetX - surfaceAnchorX) * appliedFactor + surfaceAnchorX;
    viewTransform.offsetY =
        (viewTransform.offsetY - surfaceAnchorY) * appliedFactor + surfaceAnchorY;
    viewTransform.zoomScale = newZoomScale;
  }
  notifyViewTransformChanged(newZoomScale);
}

void ContentViewModel::panBy(double deltaX, double deltaY, const QSizeF& viewportSize) {
  if (std::abs(deltaX) < TRANSFORM_EPSILON && std::abs(deltaY) < TRANSFORM_EPSILON) {
    return;
  }
  auto contentWidth = static_cast<double>(getWidth());
  auto contentHeight = static_cast<double>(getHeight());
  if (viewportSize.isEmpty() || contentWidth <= 0 || contentHeight <= 0) {
    return;
  }
  double currentZoomScale = 1.0;
  {
    std::lock_guard<std::mutex> lock(viewTransformMutex);
    auto surface = surfaceSize.isEmpty() ? viewportSize : surfaceSize;
    auto viewportScale =
        ComputeContentFit(contentWidth, contentHeight, viewportSize.width(), viewportSize.height())
            .scale;
    auto surfaceScale =
        ComputeContentFit(contentWidth, contentHeight, surface.width(), surface.height()).scale;
    auto ratio = viewportScale / surfaceScale;
    // Undo the fit ratio because viewport deltas may target a differently sized render surface.
    viewTransform.offsetX += deltaX / ratio;
    viewTransform.offsetY += deltaY / ratio;
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

void ContentViewModel::setSurfaceSize(const QSizeF& size) {
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  surfaceSize = size.width() <= 0 || size.height() <= 0 ? QSizeF{} : size;
}

void ContentViewModel::adjustForSurfaceResize(double oldSurfaceWidth, double oldSurfaceHeight,
                                              double newSurfaceWidth, double newSurfaceHeight,
                                              double contentWidth, double contentHeight) {
  if (contentWidth <= 0 || contentHeight <= 0 || oldSurfaceWidth <= 0 || oldSurfaceHeight <= 0 ||
      newSurfaceWidth <= 0 || newSurfaceHeight <= 0) {
    return;
  }
  auto oldContentScale =
      ComputeContentFit(contentWidth, contentHeight, oldSurfaceWidth, oldSurfaceHeight).scale;
  auto newContentScale =
      ComputeContentFit(contentWidth, contentHeight, newSurfaceWidth, newSurfaceHeight).scale;
  std::lock_guard<std::mutex> lock(viewTransformMutex);
  auto ratio = newContentScale / oldContentScale;
  // Scale center-relative pan by the fit ratio to preserve the content at the viewport center.
  auto zoomOffsetScale = (viewTransform.zoomScale - 1) * 0.5;
  viewTransform.offsetX = (viewTransform.offsetX + oldSurfaceWidth * zoomOffsetScale) * ratio -
                          newSurfaceWidth * zoomOffsetScale;
  viewTransform.offsetY = (viewTransform.offsetY + oldSurfaceHeight * zoomOffsetScale) * ratio -
                          newSurfaceHeight * zoomOffsetScale;
}

void ContentViewModel::notifyViewTransformChanged(double newZoomScale) {
  onViewTransformChanged();
  Q_EMIT zoomScaleChanged(newZoomScale);
  Q_EMIT requestFlush();
}

}  // namespace pag
