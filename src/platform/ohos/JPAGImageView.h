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
#include "tgfx/gpu/Window.h"

namespace pag {
class JPAGImageView : public PAGAnimator::Listener, public XComponentListener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static inline std::string ClassName() {
    return "JPAGImageView";
  }

  explicit JPAGImageView(const std::string& id) : id(std::move(id)) {
  }

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

  std::shared_ptr<PAGDecoder> getDecoder();

  std::shared_ptr<PAGAnimator> getAnimator();

  void setCurrentFrame(Frame currentFrame);

  Frame currentFrame();

  void setComposition(std::shared_ptr<PAGComposition> composition, float frameRate);

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

  void release();

  std::string id;

  napi_threadsafe_function progressCallback = nullptr;
  napi_threadsafe_function playingStateCallback = nullptr;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);

  std::shared_ptr<PAGDecoder> getDecoderInternal();

  void invalidSize();

  void invalidDecoder();

  void refreshMatrixFromScaleMode();

  bool handleFrame(Frame frame);

  std::pair<tgfx::Bitmap, std::shared_ptr<tgfx::Image>> getImage(Frame frame);

  bool present(std::shared_ptr<tgfx::Image> image);

  std::mutex locker;
  int _width = 0;
  int _height = 0;
  float _renderScale = 1.0f;
  float _frameRate = 30.0f;
  PAGScaleMode _scaleMode = PAGScaleMode::LetterBox;
  tgfx::Matrix _matrix = tgfx::Matrix::I();
  bool _cacheAllFramesInMemory = false;
  std::shared_ptr<PAGComposition> _composition = nullptr;
  std::shared_ptr<PAGAnimator> _animator = nullptr;
  std::shared_ptr<PAGDecoder> _decoder = nullptr;

  NativeWindow* _window = nullptr;
  std::shared_ptr<tgfx::Window> targetWindow = nullptr;

  std::shared_ptr<tgfx::Image> currentImage = nullptr;
  tgfx::Bitmap currentBitmap;

  std::unordered_map<Frame, std::pair<tgfx::Bitmap, std::shared_ptr<tgfx::Image>>> images;
};
}  // namespace pag
