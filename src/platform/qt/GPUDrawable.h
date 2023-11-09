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
#include <QOpenGLContext>
#include <QQuickItem>
#include <QSGTexture>
#pragma clang diagnostic pop
#include "rendering/drawables/DoubleBufferedDrawable.h"

namespace tgfx {
class QGLWindow;
}

namespace pag {
class GPUDrawable : public DoubleBufferedDrawable {
 public:
  static std::shared_ptr<GPUDrawable> MakeFrom(QQuickItem* quickItem,
                                               QOpenGLContext* sharedContext = nullptr);

  void updateSize() override;

  void moveToThread(QThread* targetThread);

  QSGTexture* getTexture();

 private:
  QQuickItem* quickItem = nullptr;

  GPUDrawable(QQuickItem* quickItem, std::shared_ptr<tgfx::QGLWindow> window);

  std::shared_ptr<tgfx::QGLWindow> qGLWindow() const;
};
}  // namespace pag
