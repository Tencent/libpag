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

#pragma once

#include <QOpenGLContext>
#include <QQuickWindow>
#include <mutex>
#include "rendering/ContentView.h"
#include "rendering/pag/PAGViewModel.h"

namespace pag {

class PAGView : public ContentView {
  Q_OBJECT
 public:
  explicit PAGView(QQuickItem* parent = nullptr);
  ~PAGView() override;

  ContentViewModel* getViewModel() const override;

  Q_SLOT void flush() const override;

  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;

 private:
  void initDrawable() override;

  Q_SLOT void onRequestSizeChanged();
  Q_SLOT void onPreferredSizeChanged();
  Q_SLOT void onAudioTimeChanged(int64_t audioTime);
  Q_SLOT void onIsPlayingChanged(bool isPlaying);

  std::mutex lastPlayTimeMutex = {};
  int64_t lastPlayTime = 0;
  std::unique_ptr<PAGViewModel> viewModel = nullptr;
};

}  // namespace pag
