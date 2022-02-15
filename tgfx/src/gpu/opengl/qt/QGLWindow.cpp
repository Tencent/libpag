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

#include "gpu/opengl/qt/QGLWindow.h"
#include <QApplication>
#include <QQuickWindow>
#include <QThread>
#include "gpu/opengl/GLContext.h"
#include "gpu/opengl/GLDefines.h"
#include "gpu/opengl/GLRenderTarget.h"
#include "gpu/opengl/GLSurface.h"
#include "gpu/opengl/GLTexture.h"

namespace pag {

std::shared_ptr<QGLWindow> QGLWindow::MakeFrom(QQuickItem* quickItem,
                                               QOpenGLContext* sharedContext) {
  if (quickItem == nullptr) {
    return nullptr;
  }
  auto device = QGLDevice::Make(sharedContext);
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<QGLWindow>(new QGLWindow(device, quickItem));
}

QGLWindow::QGLWindow(std::shared_ptr<Device> device, QQuickItem* quickItem)
    : Window(std::move(device)), quickItem(quickItem) {
}

QGLWindow::~QGLWindow() {
  renderTarget = nullptr;
  frontTexture = nullptr;
  backTexture = nullptr;
  delete outTexture;
}

void QGLWindow::moveToThread(QThread* targetThread) {
  std::lock_guard<std::mutex> autoLock(locker);
  static_cast<QGLDevice*>(device.get())->moveToThread(targetThread);
}

QSGTexture* QGLWindow::getTexture() {
  if (QThread::currentThread() != QApplication::instance()->thread()) {
    LOGE("QGLWindow::getTexture() can be called on the main (GUI) thread only.");
    return nullptr;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  auto nativeWindow = quickItem->window();
  if (nativeWindow == nullptr) {
    return nullptr;
  }
  if (textureInvalid && frontTexture) {
    textureInvalid = false;
    if (outTexture) {
      delete outTexture;
      outTexture = nullptr;
    }
    auto textureID = frontTexture->getGLInfo().id;
    auto width = static_cast<int>(ceil(quickItem->width()));
    auto height = static_cast<int>(ceil(quickItem->height()));
    outTexture = nativeWindow->createTextureFromId(textureID, QSize(width, height),
                                                   QQuickWindow::TextureHasAlphaChannel);
  }
  return outTexture;
}

void QGLWindow::invalidateTexture() {
  std::lock_guard<std::mutex> autoLock(locker);
  textureInvalid = true;
}

std::shared_ptr<Surface> QGLWindow::onCreateSurface(Context* context) {
  renderTarget = nullptr;
  frontTexture = nullptr;
  backTexture = nullptr;
  auto nativeWindow = quickItem->window();
  auto pixelRatio = nativeWindow ? nativeWindow->devicePixelRatio() : 1.0f;
  auto width = static_cast<int>(ceil(quickItem->width() * pixelRatio));
  auto height = static_cast<int>(ceil(quickItem->height() * pixelRatio));
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  frontTexture = GLTexture::MakeRGBA(context, width, height);
  backTexture = GLTexture::MakeRGBA(context, width, height);
  renderTarget = GLRenderTarget::MakeFrom(context, backTexture.get());
  return GLSurface::MakeFrom(context, renderTarget);
}

void QGLWindow::onPresent(Context* context, int64_t) {
  if (renderTarget == nullptr) {
    return;
  }
  auto gl = GLContext::Unwrap(context);
  std::swap(frontTexture, backTexture);
  gl->flush();
  gl->bindFramebuffer(GL_FRAMEBUFFER, renderTarget->getGLInfo().id);
  gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           backTexture->getGLInfo().id, 0);
  gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
  invalidateTexture();
}
}  // namespace pag