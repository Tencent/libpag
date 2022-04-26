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

#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
/**
 * Window represents a native displayable resource that can be rendered or written to by a Device.
 */
class Window {
 public:
  virtual ~Window() = default;

  /**
   * Returns the Device associated with this Window.
   */
  std::shared_ptr<Device> getDevice() const {
    return device;
  }

  /**
   * Creates a new surface connected to the window.
   */
  std::shared_ptr<Surface> createSurface(Context* context);

  /**
   * Applies all pending graphic changes to the window. The presentationTime will be passed to the
   * SurfaceTexture.getTimestamp() on android platform, ignored by other platforms. It is in
   * microseconds. A system timestamp will be used if presentationTime is not set.
   */
  void present(Context* context, int64_t presentationTime = INT64_MIN);

 protected:
  std::shared_ptr<Device> device = nullptr;

  explicit Window(std::shared_ptr<Device> device);
  virtual std::shared_ptr<Surface> onCreateSurface(Context* context) = 0;
  virtual void onPresent(Context* context, int64_t presentationTime) = 0;

 private:
  bool checkContext(Context* context);
};
}  // namespace tgfx
