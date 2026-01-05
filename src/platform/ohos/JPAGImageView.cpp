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

#include "JPAGImageView.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include <native_buffer/native_buffer.h>
#include <cstdint>
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JPAG.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JsHelper.h"
#include "rendering/utils/ApplyScaleMode.h"
#include "tgfx/core/ColorType.h"
#include "tgfx/platform/ohos/OHOSPixelMap.h"

namespace pag {
static std::unordered_map<std::string, std::shared_ptr<JPAGImageView>> ViewMap = {};

static napi_value Flush(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool flushResult = false;
  if (view != nullptr) {
    flushResult = view->flush();
  }
  napi_value result;
  napi_get_boolean(env, flushResult, &result);
  return result;
}

static napi_value Update(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  auto animator = view->getAnimator();
  if (view != nullptr && animator != nullptr) {
    animator->update();
  }
  return nullptr;
}

static napi_value SetComposition(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc < 2) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  double frameRate = 30.0f;
  napi_get_value_double(env, args[1], &frameRate);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    if (layer != nullptr && layer->layerType() == LayerType::PreCompose) {
      view->setComposition(std::static_pointer_cast<PAGComposition>(layer), frameRate);
    } else {
      view->setComposition(nullptr, 30.0f);
    }
  }
  return nullptr;
}

static napi_value ScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  PAGScaleMode scaleMode = PAGScaleMode::LetterBox;
  if (view != nullptr) {
    scaleMode = view->scaleMode();
  }
  napi_value result;
  napi_create_int32(env, static_cast<int32_t>(scaleMode), &result);
  return result;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int scaleMode = 0;
  napi_get_value_int32(env, args[0], &scaleMode);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setScaleMode(static_cast<PAGScaleMode>(scaleMode));
  }
  return nullptr;
}

static napi_value Matrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  auto matrix = Matrix::I();
  if (view != nullptr) {
    matrix = view->matrix();
  }
  return CreateMatrix(env, matrix);
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setMatrix(GetMatrix(env, args[0]));
  }
  return nullptr;
}

static napi_value CacheAllFramesInMemory(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool cacheAllFramesInMemory = false;
  if (view != nullptr) {
    cacheAllFramesInMemory = view->cacheAllFramesInMemory();
  }
  napi_value result;
  napi_get_boolean(env, cacheAllFramesInMemory, &result);
  return result;
}

static napi_value SetCacheAllFramesInMemory(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool cacheAllFramesInMemory = false;
  napi_get_value_bool(env, args[0], &cacheAllFramesInMemory);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setCacheAllFramesInMemory(cacheAllFramesInMemory);
  }
  return nullptr;
}

static napi_value RepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  int repeatCount = 0;
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      repeatCount = animator->repeatCount();
    }
  }
  napi_value result;
  napi_create_int32(env, repeatCount, &result);
  return result;
}

static napi_value SetRepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int repeatCount = 0;
  napi_get_value_int32(env, args[0], &repeatCount);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->setRepeatCount(repeatCount);
    }
  }
  return nullptr;
}

static napi_value Play(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->start();
    }
  }
  return nullptr;
}

static napi_value IsPlaying(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool isPlaying = false;
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      isPlaying = animator->isRunning();
    }
  }
  napi_value result;
  napi_get_boolean(env, isPlaying, &result);
  return result;
}

static napi_value Pause(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->cancel();
    }
  }
  return nullptr;
}

static napi_value UniqueID(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_string_utf8(env, view->id.c_str(), view->id.length(), &result);
  return result;
}

static napi_value NumFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  auto decoder = view->getDecoder();
  if (decoder) {
    napi_create_int64(env, decoder->numFrames(), &result);
  } else {
    napi_create_int64(env, 0, &result);
  }
  return result;
}

static void StateChangeCallback(napi_env env, napi_value callback, void*, void* data) {
  uint8_t state = *static_cast<uint8_t*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_uint32(env, static_cast<uint32_t>(state), &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
}

static napi_value SetStateChangeCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewStateChangeCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, view,
                                  StateChangeCallback, &view->playingStateCallback);
  return nullptr;
}

static void ProgressCallback(napi_env env, napi_value callback, void*, void*) {
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, 0, nullptr, nullptr);
}

static napi_value SetProgressUpdateCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewProgressCallback", NAPI_AUTO_LENGTH, &resourceName);
  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                  nullptr, ProgressCallback, &view->progressCallback);
  return nullptr;
}

static napi_value RenderScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_double(env, view->renderScale(), &result);
  return result;
}

static napi_value SetRenderScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 1.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setRenderScale(value);
  }
  return nullptr;
}

static napi_value CurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  Frame currentFrame = 0;
  if (view != nullptr) {
    currentFrame = view->currentFrame();
  }
  napi_value result;
  napi_create_int64(env, currentFrame, &result);
  return result;
}

static napi_value SetCurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int64_t currentFrame = 0;
  napi_get_value_int64(env, args[0], &currentFrame);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setCurrentFrame(currentFrame);
  }
  return nullptr;
}

static napi_value CurrentImage(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  return view->getCurrentPixelMap(env);
}

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  view->release();
  return nullptr;
}

napi_value JPAGImageView::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAImageView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JPAGImageView>(id);
  cView->_animator = PAGAnimator::MakeFrom(cView);
  XComponentHandler::AddListener(id, cView);
  napi_wrap(
      env, jsView, cView.get(),
      [](napi_env, void* finalize_data, void*) {
        JPAGImageView* view = static_cast<JPAGImageView*>(finalize_data);
        XComponentHandler::RemoveListener(view->id);
        ViewMap.erase(view->id);
      },
      nullptr, nullptr);
  ViewMap.emplace(id, cView);
  return jsView;
}

std::shared_ptr<PAGDecoder> JPAGImageView::getDecoderInternal() {
  if (targetWindow == nullptr || _composition == nullptr) {
    invalidDecoder();
    return nullptr;
  }
  if (_decoder == nullptr) {
    float scaleFactor = 1.0;
    if (_width >= _height) {
      scaleFactor = static_cast<float>(_renderScale * (_width * 1.0 / _composition->width()));
    } else {
      scaleFactor = static_cast<float>(_renderScale * (_height * 1.0 / _composition->height()));
    }
    _decoder = PAGDecoder::MakeFrom(_composition, _frameRate, scaleFactor);
    refreshMatrixFromScaleMode();
  }
  return _decoder;
}

bool JPAGImageView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(scaleMode, ScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(matrix, Matrix),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(cacheAllFramesInMemory, CacheAllFramesInMemory),
      PAG_DEFAULT_METHOD_ENTRY(setCacheAllFramesInMemory, SetCacheAllFramesInMemory),
      PAG_DEFAULT_METHOD_ENTRY(repeatCount, RepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(isPlaying, IsPlaying),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID),
      PAG_DEFAULT_METHOD_ENTRY(setRenderScale, SetRenderScale),
      PAG_DEFAULT_METHOD_ENTRY(renderScale, RenderScale),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(setCurrentFrame, SetCurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(numFrame, NumFrame),
      PAG_DEFAULT_METHOD_ENTRY(setCurrentFrame, SetCurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(currentImage, CurrentImage),
      PAG_DEFAULT_METHOD_ENTRY(update, Update),
      PAG_DEFAULT_METHOD_ENTRY(release, Release)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  if (status != napi_ok) {
    return false;
  }
  return true;
}

void JPAGImageView::onAnimationStart(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Start),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationCancel(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Cancel),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationEnd(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::End),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationRepeat(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Repeat),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationUpdate(PAGAnimator* animator) {
  std::lock_guard lock_guard(locker);
  if (progressCallback) {
    napi_call_threadsafe_function(progressCallback, nullptr,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
  Frame frame = 0;
  auto decoder = getDecoderInternal();
  if (_composition != nullptr && decoder != nullptr) {
    frame = ProgressToFrame(animator->progress(), _decoder->numFrames());
  }
  handleFrame(frame);
}

void JPAGImageView::onSurfaceCreated(NativeWindow* window) {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr) {
    return;
  }
  _window = window;
  targetWindow = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(_window));
  invalidSize();
  _animator->update();
}

void JPAGImageView::onSurfaceSizeChanged() {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr) {
    return;
  }
  invalidSize();
}

void JPAGImageView::onSurfaceDestroyed() {
  std::lock_guard lock_guard(locker);
  _window = nullptr;
  targetWindow = nullptr;
  invalidSize();
}

std::shared_ptr<PAGDecoder> JPAGImageView::getDecoder() {
  std::lock_guard lock_guard(locker);
  return getDecoderInternal();
}

void JPAGImageView::invalidSize() {
  if (targetWindow && _window) {
    targetWindow->invalidSize();
    OH_NativeWindow_NativeWindowHandleOpt(_window, GET_BUFFER_GEOMETRY, &_height, &_width);
  } else {
    _width = 0;
    _height = 0;
  }
  invalidDecoder();
}

void JPAGImageView::invalidDecoder() {
  _decoder = nullptr;
  images.clear();
}

std::shared_ptr<PAGAnimator> JPAGImageView::getAnimator() {
  std::lock_guard lock_guard(locker);
  return _animator;
}

void JPAGImageView::setCurrentFrame(Frame currentFrame) {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr || _composition == nullptr || _decoder == nullptr) {
    return;
  }
  _animator->setProgress(FrameToProgress(currentFrame, _decoder->numFrames()));
}

Frame JPAGImageView::currentFrame() {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr || _decoder == nullptr) {
    return 0;
  }
  return ProgressToFrame(_animator->progress(), _decoder->numFrames());
}

void JPAGImageView::setComposition(std::shared_ptr<PAGComposition> composition, float frameRate) {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr) {
    return;
  }
  if (composition != nullptr) {
    _animator->setDuration(composition->duration());
  } else {
    _animator->setDuration(0);
  }
  _composition = composition;
  _frameRate = frameRate;
  invalidDecoder();
}

void JPAGImageView::setScaleMode(PAGScaleMode scaleMode) {
  std::lock_guard lock_guard(locker);
  _scaleMode = scaleMode;
  refreshMatrixFromScaleMode();
}

PAGScaleMode JPAGImageView::scaleMode() {
  return _scaleMode;
}

void JPAGImageView::setMatrix(const class Matrix& matrix) {
  std::lock_guard lock_guard(locker);
  _matrix = ToTGFX(matrix);
  _scaleMode = PAGScaleMode::None;
}

class Matrix JPAGImageView::matrix() {
  std::lock_guard lock_guard(locker);
  return ToPAG(_matrix);
}

void JPAGImageView::setRenderScale(float renderScale) {
  std::lock_guard lock_guard(locker);
  if (renderScale <= 0.0 || renderScale > 1.0) {
    renderScale = 1.0;
  }
  if (_renderScale == renderScale) {
    return;
  }
  _renderScale = renderScale;
  invalidDecoder();
}

float JPAGImageView::renderScale() {
  return _renderScale;
}

void JPAGImageView::setCacheAllFramesInMemory(bool cacheAllFramesInMemory) {
  std::lock_guard lock_guard(locker);
  if (_cacheAllFramesInMemory == cacheAllFramesInMemory) {
    return;
  }
  _cacheAllFramesInMemory = cacheAllFramesInMemory;
  if (!_cacheAllFramesInMemory) {
    images.clear();
  }
}

bool JPAGImageView::cacheAllFramesInMemory() {
  return _cacheAllFramesInMemory;
}

bool JPAGImageView::flush() {
  std::lock_guard lock_guard(locker);
  auto decoder = getDecoderInternal();
  if (decoder && _animator) {
    return handleFrame(ProgressToFrame(_animator->progress(), decoder->numFrames()));
  }
  return false;
}

void JPAGImageView::refreshMatrixFromScaleMode() {
  if (_scaleMode == PAGScaleMode::None) {
    return;
  }
  if (_decoder == nullptr) {
    return;
  }

  _matrix = ToTGFX(ApplyScaleMode(_scaleMode, _decoder->width() / _renderScale,
                                  _decoder->height() / _renderScale, _width, _height));
}

bool JPAGImageView::handleFrame(Frame frame) {
  auto decoder = getDecoderInternal();
  if (!decoder) {
    return false;
  }
  if (!decoder->checkFrameChanged(frame)) {
    return true;
  }
  auto image = getImage(frame);
  if (!image.second) {
    return false;
  }
  currentBitmap = image.first;
  currentImage = image.second;
  return present(currentImage);
}

std::pair<tgfx::Bitmap, std::shared_ptr<tgfx::Image>> JPAGImageView::getImage(Frame frame) {
  if (_cacheAllFramesInMemory && images.find(frame) != images.end()) {
    return images[frame];
  }

  tgfx::Bitmap bitmap;
  if (!bitmap.allocPixels(_decoder->width(), _decoder->height(), false, false)) {
    return {{}, nullptr};
  }
  auto pixels = bitmap.lockPixels();
  if (pixels == nullptr) {
    return {{}, nullptr};
  }
  _decoder->readFrame(frame, pixels, bitmap.rowBytes());
  bitmap.unlockPixels();
  auto image = tgfx::Image::MakeFrom(bitmap);

  if (image == nullptr) {
    return {{}, nullptr};
  }
  if (_cacheAllFramesInMemory) {
    images[frame] = {bitmap, image};
  }
  return {bitmap, image};
}

bool JPAGImageView::present(std::shared_ptr<tgfx::Image> image) {
  if (!targetWindow && image) {
    return false;
  }
  auto device = targetWindow->getDevice();
  if (!device) {
    return false;
  }
  auto context = device->lockContext();
  if (!context) {
    return false;
  }
  auto surface = targetWindow->getSurface(context, true);
  if (surface == nullptr) {
    surface = targetWindow->getSurface(context, false);
  }
  if (!surface) {
    device->unlock();
    return false;
  }
  auto canvas = surface->getCanvas();
  if (!canvas) {
    device->unlock();
    return false;
  }
  canvas->clear();
  tgfx::Matrix imageMatrix = tgfx::Matrix::MakeScale(1.0 / _renderScale);
  imageMatrix.postConcat(_matrix);
  canvas->save();
  canvas->concat(imageMatrix);
  canvas->drawImage(image);
  canvas->restore();
  context->flushAndSubmit();
  targetWindow->present(context);
  context->purgeResourcesNotUsedSince(std::chrono::steady_clock::now());
  device->unlock();
  return true;
}

napi_value JPAGImageView::getCurrentPixelMap(napi_env env) {
  std::lock_guard lock_guard(locker);
  if (currentBitmap.isEmpty()) {
    return nullptr;
  }

  // create PixelMap
  OhosPixelMapCreateOps ops;
  ops.width = currentBitmap.width();
  ops.height = currentBitmap.height();
  ops.pixelFormat = currentBitmap.colorType() == tgfx::ColorType::RGBA_8888
                        ? PIXEL_FORMAT_RGBA_8888
                        : PIXEL_FORMAT_BGRA_8888;
  ops.alphaType = OHOS_PIXEL_MAP_ALPHA_TYPE_PREMUL;
  ops.editable = false;
  napi_value pixelMap;
  auto pixels = currentBitmap.lockPixels();
  auto status = OH_PixelMap_CreatePixelMapWithStride(env, ops, pixels, currentBitmap.byteSize(),
                                                     currentBitmap.rowBytes(), &pixelMap);
  currentBitmap.unlockPixels();

  // readPixels
  auto nativePixelMap = OH_PixelMap_InitNativePixelMap(env, pixelMap);
  if (!nativePixelMap) {
    return nullptr;
  }
  void* pixelMapAddress = nullptr;
  OH_PixelMap_AccessPixels(nativePixelMap, &pixelMapAddress);
  currentBitmap.readPixels(currentBitmap.info(), pixelMapAddress);
  OH_PixelMap_UnAccessPixels(nativePixelMap);
  if (status == napi_ok) {
    return pixelMap;
  } else {
    return nullptr;
  }
}

void JPAGImageView::release() {
  // A memory leak may occur if the timer is not cancelled upon release.
  if (_animator) {
    _animator->cancel();
  }

  std::lock_guard lock_guard(locker);
  if (progressCallback != nullptr) {
    napi_release_threadsafe_function(progressCallback, napi_tsfn_abort);
    progressCallback = nullptr;
  }
  if (playingStateCallback != nullptr) {
    napi_release_threadsafe_function(playingStateCallback, napi_tsfn_abort);
    playingStateCallback = nullptr;
  }

  if (_animator) {
    _animator = nullptr;
  }

  invalidDecoder();
}

}  // namespace pag
