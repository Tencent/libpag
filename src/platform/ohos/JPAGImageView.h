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
#include <native_window/external_window.h>
#include "pag/pag.h"
#include "platform/ohos/XComponentHandler.h"
#include "rendering/PAGAnimator.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Window.h"

namespace pag {
class PAGViewEventDispatcher;
class JPAGImageViewRenderSession;

class JPAGImageView : public PAGAnimator::Listener, public XComponentListener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static inline std::string ClassName() {
    return "JPAGImageView";
  }

  JPAGImageView(const std::string& id, napi_env env);

  virtual ~JPAGImageView() {
    release();
  }

  void onAnimationStart(PAGAnimator*) override;

  void onAnimationCancel(PAGAnimator*) override;

  void onAnimationEnd(PAGAnimator*) override;

  void onAnimationRepeat(PAGAnimator*) override;

  void onAnimationUpdate(PAGAnimator* animator) override;

  void onSurfaceCreated(NativeWindow* window) override;

  void onSurfaceSizeChanged() override;

  void onSurfaceDestroyed() override;

  Frame numFrames();

  std::shared_ptr<PAGAnimator> getAnimator();

  void setCurrentFrame(Frame currentFrame);

  Frame currentFrame();

  void setComposition(std::shared_ptr<PAGComposition> composition, float frameRate);

  void setVisible(bool visible);

  void setScaleMode(PAGScaleMode scaleMode);

  PAGScaleMode scaleMode();

  void setMatrix(const Matrix& matrix);

  Matrix matrix();

  void setRenderScale(float renderScale);

  float renderScale();

  void setCacheAllFramesInMemory(bool cacheAllFramesInMemory);

  bool cacheAllFramesInMemory();

  bool flush();

  napi_value getCurrentPixelMap(napi_env env);

  void release(napi_env env = nullptr);

  void setProgressCallback(napi_env env, napi_value callback);

  void setPlayingStateCallback(napi_env env, napi_value callback);

  std::string id;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);

  std::shared_ptr<JPAGImageViewRenderSession> renderSession = nullptr;
  std::shared_ptr<PAGViewEventDispatcher> eventDispatcher = nullptr;
  std::shared_ptr<PAGAnimator> _animator = nullptr;
  bool isVisible = false;
  int64_t compositionDuration = 0;
  uint64_t callbackGeneration = 0;
  bool released = false;
  std::mutex locker;
};
}  // namespace pag
