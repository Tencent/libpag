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
#include "rendering/ContentView.h"
#include "rendering/pagx/PAGXRenderer.h"
#include "rendering/pagx/PAGXViewModel.h"

namespace pag {

/**
 * PAGXView is a view for rendering PAGX files.
 */
class PAGXView : public ContentView {
  Q_OBJECT
 public:
  explicit PAGXView(QQuickItem* parent = nullptr);
  ~PAGXView() override;

  ContentViewModel* getViewModel() const override;

  Q_SLOT void sizeChangedDelayHandle() override;

  void flush() const override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;

 private:
  void initDrawable() override;

  std::unique_ptr<PAGXViewModel> viewModel = nullptr;

  friend class PAGXRenderer;
  friend class PAGXViewModel;
};

}  // namespace pag
