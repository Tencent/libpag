/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/opengl/qt/QGLWindow.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::MakeFrom(QQuickItem* quickItem) {
  auto window = tgfx::QGLWindow::MakeFrom(quickItem);
  if (window == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(quickItem, std::move(window)));
}

GPUDrawable::GPUDrawable(QQuickItem* quickItem, std::shared_ptr<tgfx::QGLWindow> window)
    : DoubleBufferedDrawable(std::move(window)), quickItem(quickItem) {
  GPUDrawable::updateSize();
}

void GPUDrawable::updateSize() {
  auto nativeWindow = quickItem->window();
  auto pixelRatio = nativeWindow ? nativeWindow->devicePixelRatio() : 1.0f;
  _width = static_cast<int>(ceil(quickItem->width() * pixelRatio));
  _height = static_cast<int>(ceil(quickItem->height() * pixelRatio));
}

std::shared_ptr<tgfx::QGLWindow> GPUDrawable::qGLWindow() const {
  return std::static_pointer_cast<tgfx::QGLWindow>(window);
}

void GPUDrawable::moveToThread(QThread* targetThread) {
  qGLWindow()->moveToThread(targetThread);
}

QSGTexture* GPUDrawable::getTexture() {
  return qGLWindow()->getTexture();
}
}  // namespace pag
