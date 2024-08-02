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

#include "JPAGView.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstdint>

#include "base/utils/Log.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JsHelper.h"
#include "platform/ohos/JPAGLayerHandle.h"

namespace pag {
static int PAGViewStateStart = 0;
static int PAGViewStateCancel = 1;
static int PAGViewStateEnd = 2;
static int PAGViewStateRepeat = 3;

static std::unordered_map<std::string, std::shared_ptr<JPAGView>> ViewMap = {};

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
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
    LOGE("Could not find PAGView on Surface Created id:%s", id.c_str());
    return;
  }
  auto drawable = pag::GPUDrawable::FromWindow(static_cast<NativeWindow*>(window));
  auto view = ViewMap[id];
  view->player->setSurface(pag::PAGSurface::MakeFrom(drawable));
  view->animator->update();
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
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
    LOGE("Could not find PAGView on Surface change id:%s", id.c_str());
    return;
  }
  auto surface = ViewMap[id]->player->getSurface();
  if (!surface) {
    LOGE("PAGView id:%s without PAGSurface while surface size change", id.c_str());
    return;
  }
  surface->updateSize();
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
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
    LOGE("Could not find PAGView on Surface Destroyed id:%s", id.c_str());
    return;
  }
  ViewMap[id]->player->setSurface(nullptr);
}

static void DispatchTouchEventCB(OH_NativeXComponent*, void*) {
}

static napi_value Flush(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->animator->update();
  }
  return nullptr;
}

static napi_value SetProgress(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double progress = 0;
  napi_get_value_double(env, args[0], &progress);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->animator->setProgress(progress);
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
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    if (layer != nullptr) {
      if (layer->layerType() == LayerType::PreCompose) {
        view->player->setComposition(std::static_pointer_cast<PAGComposition>(layer));
        view->animator->setDuration(layer->duration());
      }
    } else {
      view->player->setComposition(nullptr);
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
  JPAGView* view = nullptr;
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
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->prepare();
    view->animator->start();
  }
  return nullptr;
}

static napi_value Pause(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
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
  JPAGView* view = nullptr;
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
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewStateChangeCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, view,
                                  StateChangeCallback, &view->playingStateCallback);
  return nullptr;
}

static void ProgressCallback(napi_env env, napi_value callback, void*, void* data) {
  double* progressPtr = static_cast<double*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_double(env, *progressPtr, &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
  delete progressPtr;
}

static napi_value SetProgressUpdateCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewProgressCallback", NAPI_AUTO_LENGTH, &resourceName);
  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                  nullptr, ProgressCallback, &view->progressCallback);
  return nullptr;
}

static napi_value SetSync(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->animator->setSync(value);
  }
  return nullptr;
}

static napi_value SetVideoEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setVideoEnabled(value);
  }
  return nullptr;
}

static napi_value SetCacheEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setCacheEnabled(value);
  }
  return nullptr;
}

static napi_value SetCacheScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 1.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setCacheScale(value);
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
  double value = 60.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setMaxFrameRate(value);
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
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setScaleMode(value);
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
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->player->setMatrix(GetMatrix(env, args[0]));
  }
  return nullptr;
}

static napi_value CurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, view->player->currentFrame(), &result);
  return result;
}

static napi_value GetLayersUnderPoint(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsView = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (!view) {
    return nullptr;
  }
  double surfaceX = 0;
  napi_get_value_double(env, args[0], &surfaceX);
  double surfaceY = 0;
  napi_get_value_double(env, args[1], &surfaceY);
  auto layers = view->player->getLayersUnderPoint(surfaceX, surfaceY);
  napi_value result;
  napi_create_array_with_length(env, layers.size(), &result);
  for (uint32_t i = 0; i < layers.size(); i++) {
    auto layer = JPAGLayerHandle::ToJs(env, layers[i]);
    if (!layer) {
      break;
    }
    napi_set_element(env, result, i, layer);
  }
  return result;
}

static napi_value FreeCache(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto surface = view->player->getSurface();
  if (surface) {
    surface->freeCache();
  }
  return nullptr;
}

static napi_value MakeSnapshot(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto surface = view->player->getSurface();
  return MakeSnapshot(env, surface.get());
}

napi_value JPAGView::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAGView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JPAGView>(id);
  cView->animator = PAGAnimator::MakeFrom(cView);
  cView->animator->setRepeatCount(-1);
  napi_wrap(
      env, jsView, cView.get(),
      [](napi_env, void* finalize_data, void*) {
        JPAGView* view = static_cast<JPAGView*>(finalize_data);
        ViewMap.erase(view->id);
        delete view;
      },
      nullptr, nullptr);
  ViewMap.emplace(id, cView);
  return jsView;
}

bool JPAGView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID),
      PAG_DEFAULT_METHOD_ENTRY(setSync, SetSync),
      PAG_DEFAULT_METHOD_ENTRY(setVideoEnabled, SetVideoEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setCacheEnabled, SetCacheEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setCacheScale, SetCacheScale),
      PAG_DEFAULT_METHOD_ENTRY(setMaxFrameRate, SetMaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(getLayersUnderPoint, GetLayersUnderPoint),
      PAG_DEFAULT_METHOD_ENTRY(freeCache, FreeCache),
      PAG_DEFAULT_METHOD_ENTRY(makeSnapshot, MakeSnapshot)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  napi_value exportInstance = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
    return false;
  }

  OH_NativeXComponent* nativeXComponent = nullptr;
  auto temp = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (temp != napi_ok) {
    return false;
  }

  static OH_NativeXComponent_Callback renderCallback;
  renderCallback.OnSurfaceCreated = OnSurfaceCreatedCB;
  renderCallback.OnSurfaceChanged = OnSurfaceChangedCB;
  renderCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
  renderCallback.DispatchTouchEvent = DispatchTouchEventCB;

  OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback);
  return status == napi_ok;
}

void JPAGView::onAnimationStart(PAGAnimator*) {

  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateStart,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGView::onAnimationCancel(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateCancel,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGView::onAnimationEnd(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateEnd,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGView::onAnimationRepeat(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGViewStateRepeat,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGView::onAnimationUpdate(PAGAnimator* animator) {
  napi_call_threadsafe_function(progressCallback, new double(animator->progress()),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  player->setProgress(animator->progress());
  player->flush();
}

}  // namespace pag