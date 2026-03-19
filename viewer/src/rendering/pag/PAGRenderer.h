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
#include <memory>
#include "pag/pag.h"
#include "rendering/IContentRenderer.h"

namespace pag {

class PAGView;

/**
 * Renderer implementation for PAG format content. Executes PAGPlayer flush and collects
 * per-frame timing metrics.
 */
class PAGRenderer : public IContentRenderer {
 public:
  explicit PAGRenderer(PAGView* view);

  RenderMetrics flush() override;
  void updateSize() override;
  bool isReady() const override;

 private:
  PAGView* view = nullptr;
};

}  // namespace pag
