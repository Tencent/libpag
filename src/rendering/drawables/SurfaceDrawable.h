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

#include "Drawable.h"
#include "tgfx/gpu/Window.h"

namespace pag {
/** Stages rendering offscreen so window submission only contains accepted pixels. */
class SurfaceDrawable : public Drawable {
 public:
  bool needsPresentationCopy() const override;

  bool preparePresent(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  /** Returns the window whose current frame receives accepted staging pixels. */
  virtual std::shared_ptr<tgfx::Window> getWindow() = 0;
};
}  // namespace pag
