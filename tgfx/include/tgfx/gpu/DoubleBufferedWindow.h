/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/gpu/Window.h"

namespace tgfx {
class DoubleBufferedWindow : public Window {
 public:
  static std::shared_ptr<DoubleBufferedWindow> Make(std::shared_ptr<Device> device,
                                                    const BackendTexture& frontBackendTexture,
                                                    const BackendTexture& backBackendTexture);

  static std::shared_ptr<DoubleBufferedWindow> Make(std::shared_ptr<Device> device,
                                                    HardwareBufferRef frontBuffer,
                                                    HardwareBufferRef backBuffer);

  std::shared_ptr<Surface> getBackSurface() const {
    return backSurface;
  }

  virtual bool isFront(const BackendTexture&) const {
    return false;
  }

  virtual bool isFront(HardwareBufferRef) const {
    return false;
  }

  ~DoubleBufferedWindow() override;

 protected:
  explicit DoubleBufferedWindow(std::shared_ptr<Device> device);

  void onPresent(Context* context, int64_t presentationTime) override;

  virtual void onSwapSurfaces(Context*) = 0;

  std::shared_ptr<Surface> frontSurface;
  std::shared_ptr<Surface> backSurface;
};
}  // namespace tgfx
