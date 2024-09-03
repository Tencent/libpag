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

#include "JPAGImageView.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include <native_buffer/native_buffer.h>
#include <unistd.h>
#include <cstdint>
#include "base/utils/Log.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JPAG.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JsHelper.h"
#include "rendering/editing/StillImage.h"
#include "rendering/utils/ApplyScaleMode.h"
#include "tgfx/utils/Clock.h"
#include "native_window/external_window.h"

namespace pag {
static int PAGViewStateStart = 0;
static int PAGViewStateCancel = 1;
static int PAGViewStateEnd = 2;
static int PAGViewStateRepeat = 3;

static std::unordered_map<std::string, std::shared_ptr<JPAGImageView>> ViewMap = {};

void OnImageViewSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }

  std::string id(idStr);
  if (ViewMap.find(id) == ViewMap.end()) {
    return;
  }
  auto view = ViewMap[id];
  auto animator = view->getAnimator();
  if (animator == nullptr) {
    return;
  }
  view->setTargetWindow(window);
  animator->update();
}

void OnImageViewSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }

  std::string id(idStr);
  if (ViewMap.find(id) == ViewMap.end()) {
    return;
  }
  auto view = ViewMap[id];
  auto animator = view->getAnimator();
  if (animator == nullptr) {
    return;
  }
  view->invalidSize(static_cast<OHNativeWindow*>(window));
}

void OnImageViewSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }
  std::string id(idStr);
  if (ViewMap.find(id) == ViewMap.end()) {
    return;
  }
  ViewMap[id]->setTargetWindow(nullptr);
}

static void DispatchImageViewTouchEventCB(OH_NativeXComponent*, void*) {
}

static napi_value Flush(napi_env env, napi_callback_info info) {
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

static napi_value SetComposition(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    if (layer != nullptr && layer->layerType() == LayerType::PreCompose) {
      view->setComposition(std::static_pointer_cast<PAGComposition>(layer));
    } else {
      view->setComposition(nullptr);
    }
  }
  return nullptr;
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
  int state = *static_cast<int*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_uint32(env, state, &argv[0]);
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

static void ProgressCallback(napi_env env, napi_value callback, void*, void* data) {
  Frame* framePtr = static_cast<Frame*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_int64(env, *framePtr, &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
  delete framePtr;
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
    view->cacheScale = value;
    view->invalidDecoder();
  }
  return nullptr;
}

static napi_value SetMaxFrameRate(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 30.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->frameRate = value;
    view->invalidDecoder();
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
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  auto decoder = view->getDecoder();
  auto animator = view->getAnimator();
  if (!decoder || !animator) {
    napi_create_int64(env, 0, &result);
    return result;
  }
  auto currentFrame = ProgressToFrame(animator->progress(), decoder->numFrames());
  napi_create_int64(env, currentFrame, &result);
  return result;
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
  auto decoder = view->getDecoder();
  auto animator = view->getAnimator();
  if (!decoder || !animator) {
    return nullptr;
  }
  tgfx::ImageInfo imageInfo =
      tgfx::ImageInfo::Make(decoder->width(), decoder->height(), tgfx::ColorType::BGRA_8888);
  uint8_t* pixels = new (std::nothrow) uint8_t[imageInfo.byteSize()];
  if (pixels == nullptr) {
    return nullptr;
  }
  auto currentFrame = ProgressToFrame(animator->progress(), decoder->numFrames());
  if (!decoder->readFrame(currentFrame, pixels, imageInfo.rowBytes(), ColorType::BGRA_8888,
                          AlphaType::Premultiplied)) {
    delete[] pixels;
    return nullptr;
  }
  OhosPixelMapCreateOps ops;
  ops.width = imageInfo.width();
  ops.height = imageInfo.height();
  ops.pixelFormat = PIXEL_FORMAT_BGRA_8888;
  ops.alphaType = OHOS_PIXEL_MAP_ALPHA_TYPE_PREMUL;
  ops.editable = true;
  napi_value pixelMap;
  auto status = OH_PixelMap_CreatePixelMap(env, ops, pixels, imageInfo.byteSize(), &pixelMap);
  if (status == napi_ok) {
    return pixelMap;
  }
  return nullptr;
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
  napi_wrap(
      env, jsView, cView.get(),
      [](napi_env, void* finalize_data, void*) {
        JPAGImageView* view = static_cast<JPAGImageView*>(finalize_data);
        ViewMap.erase(view->id);
      },
      nullptr, nullptr);
  ViewMap.emplace(id, cView);
  return jsView;
}

std::shared_ptr<PAGDecoder> JPAGImageView::getDecoderInternal() {
  if (window == nullptr || _composition == nullptr) {
    _decoder = nullptr;
    return nullptr;
  }
  if (_decoder == nullptr) {
    float scaleFactor = 1.0;
    if (_width >= _height) {
      scaleFactor = static_cast<float>(cacheScale * (_width * 1.0 / _composition->width()));
    } else {
      scaleFactor = static_cast<float>(cacheScale * (_height * 1.0 / _composition->height()));
    }
    _decoder = PAGDecoder::MakeFrom(_composition, frameRate, scaleFactor);
  }
  return _decoder;
}

bool JPAGImageView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(setCurrentFrame, SetCurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID),
      PAG_DEFAULT_METHOD_ENTRY(setRenderScale, SetRenderScale),
      PAG_DEFAULT_METHOD_ENTRY(setMaxFrameRate, SetMaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(numFrame, NumFrame),
      PAG_DEFAULT_METHOD_ENTRY(currentImage, CurrentImage),
      PAG_DEFAULT_METHOD_ENTRY(release, Release)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  if (status != napi_ok) {
    return false;
  }
  napi_value exportInstance = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
    return true;
  }

  OH_NativeXComponent* nativeXComponent = nullptr;
  auto temp = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (temp != napi_ok) {
    return true;
  }

  static OH_NativeXComponent_Callback renderCallback;
  renderCallback.OnSurfaceCreated = OnImageViewSurfaceCreatedCB;
  renderCallback.OnSurfaceChanged = OnImageViewSurfaceChangedCB;
  renderCallback.OnSurfaceDestroyed = OnImageViewSurfaceDestroyedCB;
  renderCallback.DispatchTouchEvent = DispatchImageViewTouchEventCB;

  OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback);
  return true;
}

void JPAGImageView::onAnimationStart(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback, &PAGViewStateStart,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationCancel(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback, &PAGViewStateCancel,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationEnd(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback, &PAGViewStateEnd,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationRepeat(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback, &PAGViewStateRepeat,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGImageView::onAnimationUpdate(PAGAnimator* animator) {
  std::lock_guard lock_guard(locker);
  Frame frame = 0;
  if (_composition != nullptr) {
    frame = ProgressToFrame(animator->progress(), _composition->getFile()->duration());
  }

  if (progressCallback) {
    napi_call_threadsafe_function(progressCallback, new Frame(frame),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
  auto decoder = getDecoderInternal();
  if (window && decoder && decoder->checkFrameChanged(frame)) {
    if (decoder && decoder->checkFrameChanged(frame)) {

      if (images.find(frame) == images.end()) {
        tgfx::Bitmap bitmap;
        tgfx::Clock clock;
        bitmap.allocPixels(decoder->width(), decoder->height(), false, true);
        auto pixels = bitmap.lockPixels();
        if (pixels == nullptr) {
          return;
        }
        decoder->readFrame(frame, pixels, bitmap.rowBytes());
        bitmap.unlockPixels();
        images[frame] = tgfx::Image::MakeFrom(bitmap);
        LOGE("onAnimationUpdate createImage:%lld", clock.measure());
      }
      tgfx::Clock clock;
      auto device = window->getDevice();
      if (!device) {
        return;
      }
      auto context = device->lockContext();
      if (!context) {
        return;
      }
      auto surface = window->getSurface(context, true);
      if (surface == nullptr) {
        surface = window->getSurface(context, false);
      }
      if (!surface) {
        device->unlock();
      }
      auto canvas = surface->getCanvas();
      if (!canvas) {
        device->unlock();
      }
      canvas->clear();
      auto matrix = ApplyScaleMode(PAGScaleMode::LetterBox, images[frame]->width(),
                                      images[frame]->height(), _width, _height);
      canvas->drawImage(images[frame], ToTGFX(matrix));
      surface->flush();
      context->submit();
      window->present(context);
      device->unlock();
      LOGE("onAnimationUpdate draw:%lld", clock.measure());
    }
  }
}

std::shared_ptr<PAGDecoder> JPAGImageView::getDecoder() {
  std::lock_guard lock_guard(locker);
  return getDecoderInternal();
}

void JPAGImageView::setTargetWindow(void* targetWindow) {
  std::lock_guard lock_guard(locker);
  _window = targetWindow;
  window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(targetWindow));
  invalidSize(static_cast<OHNativeWindow*>(targetWindow));
}

void JPAGImageView::invalidDecoder() {
  std::lock_guard lock_guard(locker);
  _decoder = nullptr;
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

void JPAGImageView::setComposition(std::shared_ptr<PAGComposition> composition) {
  std::lock_guard lock_guard(locker);
  if (_animator == nullptr) {
    return;
  }
  if (composition != nullptr) {
    _animator->setDuration(composition->duration());
  } else {
    _animator->setDuration(0);
  }
  _decoder = nullptr;
  _composition = composition;
}

void JPAGImageView::invalidSize(OHNativeWindow* nativeWindow) {
  _decoder = nullptr;
  if (window) {
    window->invalidSize();
  }
  OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &_height, &_width);
}

void JPAGImageView::release() {
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

  if (_decoder) {
    _decoder = nullptr;
  }
}

}  // namespace pag