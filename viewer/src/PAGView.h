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

#include <QOpenGLContext>
#include <QQuickItem>
#include "pag/pag.h"

namespace pag {
class RenderThread;

class GPUDrawable;

class PAGView : public QQuickItem {
  Q_OBJECT
 public:
  explicit PAGView(QQuickItem* parent = nullptr);
  ~PAGView() override;

  void setFile(const std::shared_ptr<PAGFile> pagFile);

  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;

 private:
  int64_t startTime = 0;
  bool isPlaying = true;
  qreal lastPixelRatio = 1;
  qreal lastWidth = 0;
  qreal lastHeight = 0;
  QString filePath = "";
  PAGPlayer* pagPlayer = nullptr;
  RenderThread* renderThread = nullptr;
  std::shared_ptr<GPUDrawable> drawable = nullptr;

  friend class RenderThread;
};
}  // namespace pag
