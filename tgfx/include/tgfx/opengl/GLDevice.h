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

#include "tgfx/gpu/Device.h"

namespace tgfx {
/**
 * The OpenGL interface for drawing graphics.
 */
class GLDevice : public Device {
 public:
  /**
   * Returns the native handle of current OpenGL context on the calling thread.
   */
  static void* CurrentNativeHandle();

  /**
   * Returns an GLDevice associated with current OpenGL context. Returns nullptr if there is no
   * current OpenGL context on the calling thread.
   */
  static std::shared_ptr<GLDevice> Current();

  /**
   * Creates an offscreen GLDevice with specified shared OpenGL context.
   */
  static std::shared_ptr<GLDevice> Make(void* sharedContext = nullptr);

  /**
   * Returns the GLDevice associated with the specified OpenGL context.
   */
  static std::shared_ptr<GLDevice> Get(void* nativeHandle);

  ~GLDevice() override;

  /**
   * Returns true if the specified native handle is shared context to this GLDevice.
   */
  virtual bool sharableWith(void* nativeHandle) const = 0;

 protected:
  void* nativeHandle = nullptr;
  bool isAdopted = false;

  explicit GLDevice(void* nativeHandle);
  bool onLockContext() override;
  void onUnlockContext() override;
  virtual bool onMakeCurrent() = 0;
  virtual void onClearCurrent() = 0;

  friend class GLContext;
};
}  // namespace tgfx
