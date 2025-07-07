/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "GPUDrawable.h"
#include <QQuickWindow>
#include "tgfx/gpu/opengl/qt/QGLWindow.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::MakeFrom(QQuickItem* quickItem,
                                                   QOpenGLContext* sharedContext) {
  auto window = tgfx::QGLWindow::MakeFrom(quickItem, sharedContext);
  if (window == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(quickItem, std::move(window)));
}

GPUDrawable::GPUDrawable(QQuickItem* quickItem, std::shared_ptr<tgfx::QGLWindow> window)
    : quickItem(quickItem), window(std::move(window)) {
  GPUDrawable::updateSize();
}

void GPUDrawable::updateSize() {
  auto nativeWindow = quickItem->window();
  auto pixelRatio = nativeWindow ? nativeWindow->devicePixelRatio() : 1.0f;
  _width = static_cast<int>(ceil(quickItem->width() * pixelRatio));
  _height = static_cast<int>(ceil(quickItem->height() * pixelRatio));
  window->invalidSize();
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  return window->getDevice();
}

std::shared_ptr<tgfx::Surface> GPUDrawable::getSurface(tgfx::Context* context, bool queryOnly) {
  surface = window->getSurface(context, queryOnly);
  return surface;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::onCreateSurface(tgfx::Context* context) {
  return window->getSurface(context);
}

void GPUDrawable::onFreeSurface() {
  window->freeSurface();
}

void GPUDrawable::present(tgfx::Context* context) {
  window->present(context);
}

void GPUDrawable::moveToThread(QThread* targetThread) {
  window->moveToThread(targetThread);
}

QSGTexture* GPUDrawable::getTexture() {
  return window->getQSGTexture();
}
}  // namespace pag
