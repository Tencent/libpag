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

#include "pag/gpu.h"
#include "tgfx/gpu/Surface.h"

namespace pag {

class Drawable {
 public:
  virtual ~Drawable() = default;

  virtual int width() const = 0;

  virtual int height() const = 0;

 protected:
  std::shared_ptr<tgfx::Surface> surface = nullptr;

  void freeSurface();

  virtual std::shared_ptr<tgfx::Device> onCreateDevice() = 0;

  virtual std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) = 0;

 private:
  std::shared_ptr<tgfx::Device> device = nullptr;

  virtual void updateSize();

  virtual void present(tgfx::Context* context);

  virtual void setTimeStamp(int64_t timestamp);

  tgfx::Context* lockContext(bool force = false);

  void unlockContext();

  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context, bool force = false);

  void freeDevice();

  friend class PAGSurface;
};
}  // namespace pag
