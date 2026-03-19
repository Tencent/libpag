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

#include <atomic>
#include "rendering/IContentRenderer.h"

namespace pag {

class PAGXView;

/**
 * Renderer implementation for PAGX format content. Executes tgfx DisplayList rendering
 * and collects per-frame timing metrics.
 */
class PAGXRenderer : public IContentRenderer {
 public:
  explicit PAGXRenderer(PAGXView* view);

  RenderMetrics flush() override;
  void updateSize() override;
  bool isReady() const override;

 private:
  PAGXView* view = nullptr;
};

}  // namespace pag
