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
#include <cstdint>
#include "base/utils/Log.h"
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JsHelper.h"

namespace pag {
static int PAGViewStateStart = 0;
static int PAGViewStateCancel = 1;
static int PAGViewStateEnd = 2;
static int PAGViewStateRepeat = 3;

static std::unordered_map<std::string, std::shared_ptr<JPAGImageView>> ViewMap = {};

void OnSurfaceCreatedCB2(OH_NativeXComponent* component, void* window) {
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
  ViewMap[id]->window = static_cast<OHNativeWindow*>(window);
}

void OnSurfaceChangedCB2(OH_NativeXComponent* component, void* window) {
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
  //  view->invalidSize();
}

void OnSurfaceDestroyedCB2(OH_NativeXComponent* component, void* window) {
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
  ViewMap[id]->window = nullptr;
}

static void DispatchTouchEventCB2(OH_NativeXComponent*, void*) {
}

static napi_value Flush(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->animator->update();
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
    if (view->composition) {
      view->animator->setProgress(
          FrameToProgress(currentFrame, view->composition->getFile()->duration()));
    } else {
      view->animator->setProgress(0);
    }
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
    if (layer != nullptr) {
      if (layer->layerType() == LayerType::PreCompose) {
        view->composition = std::static_pointer_cast<PAGComposition>(layer);
        view->updateDecoder();
        view->animator->setDuration(layer->duration());
      }
    } else {
      view->composition = nullptr;
      view->updateDecoder();
      view->animator->setDuration(0);
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
    view->animator->setRepeatCount(repeatCount);
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
    view->animator->start();
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
    view->animator->cancel();
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
  napi_value result;
  napi_create_string_utf8(env, view->id.c_str(), view->id.length(), &result);
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
    view->updateDecoder();
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
    view->updateDecoder();
  }
  return nullptr;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int value = 0;
  napi_get_value_int32(env, args[0], &value);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    //    view->player->setScaleMode(value);
  }
  return nullptr;
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
    //    view->player->setMatrix(GetMatrix(env, args[0]));
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
  napi_create_int64(env, view->currentFrame, &result);
  return result;
}

static napi_value GetLayersUnderPoint(napi_env, napi_callback_info) {
  //  size_t argc = 2;
  //  napi_value args[2] = {nullptr};
  //  napi_value jsView = nullptr;
  //  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  //  JPAGImageView* view = nullptr;
  //  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  //  if (!view) {
  //    return nullptr;
  //  }
  //  double surfaceX = 0;
  //  napi_get_value_double(env, args[0], &surfaceX);
  //  double surfaceY = 0;
  //  napi_get_value_double(env, args[1], &surfaceY);
  //  auto layers = view->player->getLayersUnderPoint(surfaceX, surfaceY);
  //  napi_value result;
  //  napi_create_array_with_length(env, layers.size(), &result);
  //  for (uint32_t i = 0; i < layers.size(); i++) {
  //    auto layer = JPAGLayerHandle::ToJs(env, layers[i]);
  //    if (!layer) {
  //      break;
  //    }
  //    napi_set_element(env, result, i, layer);
  //  }
  //  return result;
  return nullptr;
}

static napi_value MakeSnapshot(napi_env env, napi_callback_info info) {
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
  tgfx::ImageInfo imageInfo = tgfx::ImageInfo::Make(
      view->getDecoder()->width(), view->getDecoder()->height(), tgfx::ColorType::BGRA_8888);
  uint8_t* pixels = new (std::nothrow) uint8_t[imageInfo.byteSize()];
  if (pixels == nullptr) {
    return nullptr;
  }
  if (!decoder->readFrame(0, pixels, imageInfo.rowBytes(), ColorType::BGRA_8888,
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

napi_value JPAGImageView::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAImageGView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JPAGImageView>(id);
  cView->animator = PAGAnimator::MakeFrom(cView);
  cView->animator->setRepeatCount(-1);
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
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(getLayersUnderPoint, GetLayersUnderPoint),
      PAG_DEFAULT_METHOD_ENTRY(makeSnapshot, MakeSnapshot)};
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
  renderCallback.OnSurfaceCreated = OnSurfaceCreatedCB2;
  renderCallback.OnSurfaceChanged = OnSurfaceChangedCB2;
  renderCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB2;
  renderCallback.DispatchTouchEvent = DispatchTouchEventCB2;

  OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback);
  return true;
}

void JPAGImageView::onAnimationStart(PAGAnimator*) {

  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateStart,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGImageView::onAnimationCancel(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateCancel,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGImageView::onAnimationEnd(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateEnd,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGImageView::onAnimationRepeat(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateRepeat,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGImageView::onAnimationUpdate(PAGAnimator* animator) {
  auto renderTarget = getRenderTarget();
  auto frame = ProgressToFrame(animator->progress(), composition->getFile()->duration());
  napi_call_threadsafe_function(progressCallback, new Frame(frame),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  if (_decoder && _decoder->checkFrameChanged(frame) && renderTarget) {
    _decoder->readFrame(frame, renderTarget);
    Region region{nullptr, 0};
    OH_NativeWindow_NativeWindowFlushBuffer(window, windowBuffer, windowBufferFd, region);
  }
}

std::shared_ptr<PAGDecoder> JPAGImageView::getDecoder() {
  return _decoder;
}

void JPAGImageView::updateDecoder() {
  _decoder = PAGDecoder::MakeFrom(composition, frameRate, cacheScale);
}

HardwareBufferRef JPAGImageView::getRenderTarget() {
  if (window) {
    if (!windowBuffer) {
      OH_NativeWindow_NativeWindowRequestBuffer(window, &windowBuffer, &windowBufferFd);
      OH_NativeBuffer_FromNativeWindowBuffer(windowBuffer, &buffer);
    }
  }
  return buffer;
}

}  // namespace pag