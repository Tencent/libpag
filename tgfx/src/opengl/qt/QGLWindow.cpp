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

#include "tgfx/opengl/qt/QGLWindow.h"
#include <QApplication>
#include <QQuickWindow>
#include <QThread>
#include "gpu/Texture.h"
#include "opengl/GLRenderTarget.h"
#include "opengl/GLSampler.h"
#include "tgfx/opengl/GLFunctions.h"

namespace tgfx {

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
  surface = nullptr;
  frontTexture = nullptr;
  backTexture = nullptr;
  delete outTexture;
}

void QGLWindow::moveToThread(QThread* targetThread) {
  std::lock_guard<std::mutex> autoLock(locker);
  static_cast<QGLDevice*>(device.get())->moveToThread(targetThread);
}

QSGTexture* QGLWindow::getTexture() {
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
    auto textureID = static_cast<const GLSampler*>(frontTexture->getSampler())->id;
    auto width = static_cast<int>(ceil(quickItem->width()));
    auto height = static_cast<int>(ceil(quickItem->height()));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    outTexture = QNativeInterface::QSGOpenGLTexture::fromNative(
        textureID, nativeWindow, QSize(width, height), QQuickWindow::TextureHasAlphaChannel);

#elif (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    outTexture = nativeWindow->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture,
                                                             &textureID, 0, QSize(width, height),
                                                             QQuickWindow::TextureHasAlphaChannel);
#else
    outTexture = nativeWindow->createTextureFromId(textureID, QSize(width, height),
                                                   QQuickWindow::TextureHasAlphaChannel);
#endif
  }
  return outTexture;
}

void QGLWindow::invalidateTexture() {
  std::lock_guard<std::mutex> autoLock(locker);
  textureInvalid = true;
}

std::shared_ptr<Surface> QGLWindow::onCreateSurface(Context* context) {
  surface = nullptr;
  frontTexture = nullptr;
  backTexture = nullptr;
  auto nativeWindow = quickItem->window();
  auto pixelRatio = nativeWindow ? nativeWindow->devicePixelRatio() : 1.0f;
  auto width = static_cast<int>(ceil(quickItem->width() * pixelRatio));
  auto height = static_cast<int>(ceil(quickItem->height() * pixelRatio));
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  frontTexture = Texture::MakeRGBA(context, width, height);
  backTexture = Texture::MakeRGBA(context, width, height);
  surface = Surface::MakeFrom(backTexture);
  return surface;
}

void QGLWindow::onPresent(Context* context, int64_t) {
  if (surface == nullptr) {
    return;
  }
  auto gl = GLFunctions::Get(context);
  std::swap(frontTexture, backTexture);
  surface->texture = backTexture;
  auto renderTarget = std::static_pointer_cast<GLRenderTarget>(surface->renderTarget);
  gl->bindFramebuffer(GL_FRAMEBUFFER, renderTarget->getFrameBufferID());
  auto textureID = static_cast<const GLSampler*>(backTexture->getSampler())->id;
  gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
  gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
  invalidateTexture();
}
}  // namespace tgfx
