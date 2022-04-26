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

#include "tgfx/gpu/opengl/qt/QGLDevice.h"
#include <QApplication>
#include <QThread>
#include "QGLProcGetter.h"
#include "core/utils/Log.h"

namespace tgfx {
void* GLDevice::CurrentNativeHandle() {
  return QOpenGLContext::currentContext();
}

std::shared_ptr<GLDevice> GLDevice::Current() {
  auto context = QOpenGLContext::currentContext();
  auto surface = context != nullptr ? context->surface() : nullptr;
  return QGLDevice::Wrap(context, surface, true);
}

std::shared_ptr<GLDevice> GLDevice::Make(void*) {
  return nullptr;
}

std::shared_ptr<QGLDevice> QGLDevice::Make(QOpenGLContext* sharedContext, QSurfaceFormat* format) {
  if (QThread::currentThread() != QApplication::instance()->thread()) {
    LOGE("QGLDevice::Make() can be called on the main (GUI) thread only.");
    return nullptr;
  }
  auto context = new QOpenGLContext();
  if (format) {
    context->setFormat(*format);
  }
  if (sharedContext) {
    context->setShareContext(sharedContext);
  }
  context->create();
  auto surface = new QOffscreenSurface();
  surface->setFormat(context->format());
  surface->create();
  auto device = QGLDevice::Wrap(context, surface, false);
  if (device == nullptr) {
    delete surface;
    delete context;
  }
  return device;
}

std::shared_ptr<QGLDevice> QGLDevice::MakeAdopted(QOpenGLContext* context, QSurface* surface) {
  return QGLDevice::Wrap(context, surface, true);
}

std::shared_ptr<QGLDevice> QGLDevice::Wrap(QOpenGLContext* qtContext, QSurface* qtSurface,
                                           bool isAdopted) {
  auto glContext = GLDevice::Get(qtContext);
  if (glContext) {
    return std::static_pointer_cast<QGLDevice>(glContext);
  }
  if (qtContext == nullptr || qtSurface == nullptr) {
    return nullptr;
  }
  auto oldContext = QOpenGLContext::currentContext();
  auto oldSurface = oldContext != nullptr ? oldContext->surface() : nullptr;
  if (oldContext != qtContext) {
    if (!qtContext->makeCurrent(qtSurface)) {
      return nullptr;
    }
  }
  auto device = std::shared_ptr<QGLDevice>(new QGLDevice(qtContext));
  device->isAdopted = isAdopted;
  device->qtContext = qtContext;
  device->qtSurface = qtSurface;
  device->weakThis = device;
  if (oldContext != qtContext) {
    qtContext->doneCurrent();
    if (oldContext != nullptr) {
      oldContext->makeCurrent(oldSurface);
    }
  }
  return device;
}

QGLDevice::QGLDevice(void* nativeHandle) : GLDevice(nativeHandle) {
}

QGLDevice::~QGLDevice() {
  releaseAll();
  if (isAdopted) {
    return;
  }
  delete qtContext;
  delete qtSurface;
}

bool QGLDevice::sharableWith(void* nativeContext) const {
  auto shareContext = reinterpret_cast<QOpenGLContext*>(nativeContext);
  return QOpenGLContext::areSharing(shareContext, qtContext);
}

void QGLDevice::moveToThread(QThread* targetThread) {
  ownerThread = targetThread;
  qtContext->moveToThread(targetThread);
  if (qtSurface->surfaceClass() == QSurface::SurfaceClass::Offscreen) {
    static_cast<QOffscreenSurface*>(qtSurface)->moveToThread(targetThread);
  }
}

bool QGLDevice::onMakeCurrent() {
  if (ownerThread != nullptr && QThread::currentThread() != ownerThread) {
    LOGE("QGLDevice can not be locked in a different thread.");
    return false;
  }
  oldContext = QOpenGLContext::currentContext();
  if (oldContext) {
    oldSurface = oldContext->surface();
  }
  if (oldContext == qtContext) {
    return true;
  }
  if (!qtContext->makeCurrent(qtSurface)) {
    LOGE("QGLDevice::makeCurrent() failed!");
    return false;
  }
  return true;
}

void QGLDevice::onClearCurrent() {
  if (oldContext == qtContext) {
    oldContext = nullptr;
    oldSurface = nullptr;
    return;
  }
  qtContext->doneCurrent();
  if (oldContext != nullptr) {
    oldContext->makeCurrent(oldSurface);
    oldContext = nullptr;
    oldSurface = nullptr;
  }
}
}  // namespace tgfx