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

#include "pag/gpu.h"
#include "tgfx/core/Surface.h"

namespace pag {

class Drawable {
 public:
  virtual ~Drawable() = default;

  virtual int width() const = 0;

  virtual int height() const = 0;

  virtual std::shared_ptr<tgfx::Device> getDevice() = 0;

  virtual std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context, bool queryOnly);

  virtual std::shared_ptr<tgfx::Surface> getFrontSurface(tgfx::Context* context, bool queryOnly);

  void freeSurface();

  virtual void setTimeStamp(int64_t timestamp);

  virtual void present(tgfx::Context* context);

  virtual void updateSize();

 protected:
  std::shared_ptr<tgfx::Surface> surface = nullptr;

  virtual std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) = 0;

  virtual void onFreeSurface();
};
}  // namespace pag
