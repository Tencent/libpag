/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <napi/native_api.h>
#include <native_window/external_window.h>
#include "pag/pag.h"
#include "rendering/PAGAnimator.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace pag {
class JPAGImageView : public PAGAnimator::Listener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static inline std::string ClassName() {
    return "JPAGImageView";
  }

  explicit JPAGImageView(const std::string& id) : id(std::move(id)) {
  }
  virtual ~JPAGImageView() {
    napi_release_threadsafe_function(progressCallback, napi_tsfn_abort);
    napi_release_threadsafe_function(playingStateCallback, napi_tsfn_abort);
  }

  void onAnimationStart(PAGAnimator*) override;

  void onAnimationCancel(PAGAnimator*) override;

  void onAnimationEnd(PAGAnimator*) override;

  void onAnimationRepeat(PAGAnimator*) override;

  void onAnimationUpdate(PAGAnimator* animator) override;

  std::shared_ptr<PAGDecoder> getDecoder();

  void updateDecoder();
  void invalidSize();

  HardwareBufferRef getRenderTarget();

  std::string id;
  std::shared_ptr<PAGAnimator> animator = nullptr;
  OHNativeWindow* window = nullptr;
  std::shared_ptr<PAGComposition> composition = nullptr;

  float cacheScale = 1.0f;
  float frameRate = 30.0f;
  napi_threadsafe_function progressCallback = nullptr;
  napi_threadsafe_function playingStateCallback = nullptr;

  Frame currentFrame = 0;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
  std::shared_ptr<PAGDecoder> _decoder = nullptr;
  OHNativeWindowBuffer* windowBuffer = nullptr;
  HardwareBufferRef buffer = nullptr;
  int windowBufferFd = 0;
};
}  // namespace pag
