/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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
#include "platform/ohos/XComponentHandler.h"
#include "rendering/PAGAnimator.h"

namespace pag {
class PAGViewEventDispatcher;
class JPAGViewRenderSession;

class JPAGView : public PAGAnimator::Listener, public XComponentListener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static inline std::string ClassName() {
    return "JPAGView";
  }

  JPAGView(const std::string& id, napi_env env);

  virtual ~JPAGView();

  void onAnimationStart(PAGAnimator*) override;

  void onAnimationCancel(PAGAnimator*) override;

  void onAnimationEnd(PAGAnimator*) override;

  void onAnimationRepeat(PAGAnimator*) override;

  void onAnimationUpdate(PAGAnimator* animator) override;

  void onSurfaceCreated(NativeWindow* window) override;

  void onSurfaceDestroyed() override;

  void onSurfaceSizeChanged() override;

  void release(napi_env env = nullptr);

  std::shared_ptr<PAGPlayer> getPlayer();

  std::shared_ptr<PAGAnimator> getAnimator();

  void setComposition(std::shared_ptr<PAGComposition> composition);

  void setVisible(bool visible);

  void setProgressCallback(napi_env env, napi_value callback);

  void setPlayingStateCallback(napi_env env, napi_value callback);

  std::string id;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
  std::shared_ptr<JPAGViewRenderSession> renderSession = nullptr;
  std::shared_ptr<PAGAnimator> animator = nullptr;
  std::shared_ptr<PAGViewEventDispatcher> eventDispatcher = nullptr;
  bool isVisible = false;
  bool released = false;
  std::mutex locker;
};
}  // namespace pag
