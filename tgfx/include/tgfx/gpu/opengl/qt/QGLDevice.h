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

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QWindow>
#pragma clang diagnostic pop
#include "tgfx/gpu/opengl/GLDevice.h"

namespace tgfx {
class QGLDevice : public GLDevice {
 public:
  /**
   * Creates a offscreen QT device with specified format and shared context. If format is not
   * specified, QSurfaceFormat::defaultFormat() will be used.
   * Note: Due to the fact that QOffscreenSurface is backed by a QWindow on some platforms,
   * cross-platform applications must ensure that this method is only called on the main (GUI)
   * thread. The returned QGLDevice is then safe to be used on other threads after calling
   * moveToThread(), but the initialization and destruction must always happen on the main (GUI)
   * thread.
   */
  static std::shared_ptr<QGLDevice> Make(QOpenGLContext* sharedContext = nullptr,
                                         QSurfaceFormat* format = nullptr);

  /**
   * Creates an QT device with adopted EGLDisplay, EGLSurface and EGLContext.
   */
  static std::shared_ptr<QGLDevice> MakeAdopted(QOpenGLContext* context, QSurface* surface);

  ~QGLDevice() override;

  bool sharableWith(void* nativeHandle) const override;

  /**
   * Returns the native OpenGL context.
   */
  QOpenGLContext* glContext() const {
    return qtContext;
  }

  /**
   * Changes the thread affinity for this object and its children.
   */
  void moveToThread(QThread* targetThread);

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  QThread* ownerThread = nullptr;
  QOpenGLContext* qtContext = nullptr;
  QSurface* qtSurface = nullptr;
  QOpenGLContext* oldContext = nullptr;
  QSurface* oldSurface = nullptr;

  static std::shared_ptr<QGLDevice> Wrap(QOpenGLContext* context, QSurface* surface,
                                         bool isAdopted);

  explicit QGLDevice(void* nativeHandle);

  friend class GLDevice;
  friend class QGLWindow;
};
}  // namespace tgfx
