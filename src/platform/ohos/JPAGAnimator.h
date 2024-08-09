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
#include "pag/pag.h"
#include "rendering/PAGAnimator.h"

namespace pag {
class JPAGAnimator : public PAGAnimator::Listener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static std::shared_ptr<PAGAnimator> FromJs(napi_env env, napi_value value);
  static inline std::string ClassName() {
    return "JPAGAnimator";
  }
  explicit JPAGAnimator(const std::string& id) : id(std::move(id)) {
    pagAnimator = PAGAnimator::MakeFrom(std::weak_ptr<JPAGAnimator>());
  }
  virtual ~JPAGAnimator() {
    napi_release_threadsafe_function(progressCallback, napi_tsfn_abort);
    napi_release_threadsafe_function(playingStateCallback, napi_tsfn_abort);
  }
  std::shared_ptr<PAGAnimator> animator() {
    return pagAnimator;
  }

  void onAnimationStart(PAGAnimator*) override;

  void onAnimationCancel(PAGAnimator*) override;

  void onAnimationEnd(PAGAnimator*) override;

  void onAnimationRepeat(PAGAnimator*) override;

  void onAnimationUpdate(PAGAnimator* animator) override;

  std::shared_ptr<PAGAnimator> pagAnimator = nullptr;
  std::string id;
  napi_threadsafe_function progressCallback;
  napi_threadsafe_function playingStateCallback;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
};
}  // namespace pag