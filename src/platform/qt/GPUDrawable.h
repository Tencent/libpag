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

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#include <QOpenGLContext>
#include <QQuickItem>
#include <QSGTexture>
#pragma clang diagnostic pop
#include "rendering/drawables/Drawable.h"

namespace tgfx {
class QGLWindow;
}

namespace pag {
class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> MakeFrom(QQuickItem* quickItem,
                                               QOpenGLContext* sharedContext = nullptr);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;

  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context, bool queryOnly) override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

  void moveToThread(QThread* targetThread);

  QSGTexture* getTexture();

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  int _width = 0;
  int _height = 0;
  QQuickItem* quickItem = nullptr;
  std::shared_ptr<tgfx::QGLWindow> window = nullptr;

  GPUDrawable(QQuickItem* quickItem, std::shared_ptr<tgfx::QGLWindow> window);
};
}  // namespace pag
