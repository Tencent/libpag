//
// Created on 2024/7/30.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsPAGView.h"

#include <ace/xcomponent/native_interface_xcomponent.h>

#include "base/utils/Log.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JsHelper.h"
#include "platform/ohos/JsPAGLayerHandle.h"

namespace pag {

struct PAGListenerCallbackData {
  std::string methodName;
  double progress = 0.0f;
};

static std::unordered_map<std::string, std::shared_ptr<JsPAGView>> ViewMap = {};

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
  auto drawable = pag::GPUDrawable::FromXComponent(component, window);
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

  JsPAGView* view = nullptr;
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
  JsPAGView* view = nullptr;
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
  auto layer = JsPAGLayerHandle::FromJs(env, args[0]);
  JsPAGView* view = nullptr;
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

static napi_value Play(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JsPAGView* view = nullptr;
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
  JsPAGView* view = nullptr;
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
  JsPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  napi_value result;
  napi_create_string_utf8(env, view->id.c_str(), view->id.length(), &result);
  return result;
}

static napi_value ViewConstructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAGView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JsPAGView>(id);
  cView->animator = PAGAnimator::MakeFrom(cView);
  cView->animator->setRepeatCount(-1);
  napi_wrap(
      env, jsView, cView.get(),
      [](napi_env, void* finalize_data, void*) {
        JsPAGView* view = static_cast<JsPAGView*>(finalize_data);
        ViewMap.erase(view->id);
        delete view;
      },
      nullptr, nullptr);
  ViewMap.emplace(id, cView);
  return jsView;
}

static void PlayingStateCallback(napi_env env, napi_value callback, void*, void* data) {
  bool* boolPtr = static_cast<bool*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_get_boolean(env, *boolPtr, &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
  delete boolPtr;
}

static napi_value SetPlayingChangeCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JsPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewPlayingChangeCallback", NAPI_AUTO_LENGTH, &resourceName);

  auto state =
      napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                      view, PlayingStateCallback, &view->playingStateCallback);
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

static napi_value setProgressCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JsPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewProgressCallback", NAPI_AUTO_LENGTH, &resourceName);
  auto state =
      napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                      nullptr, ProgressCallback, &view->progressCallback);
  return nullptr;
}

bool JsPAGView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(setPlayingChangeCallback, SetPlayingChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressCallback, setProgressCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, ViewConstructor, "");
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

void JsPAGView::onAnimationStart(PAGAnimator*) {

  napi_call_threadsafe_function(playingStateCallback, new bool(true),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JsPAGView::onAnimationCancel(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, new bool(false),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JsPAGView::onAnimationEnd(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, new bool(false),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JsPAGView::onAnimationRepeat(PAGAnimator*) {
}

void JsPAGView::onAnimationUpdate(PAGAnimator* animator) {
  napi_call_threadsafe_function(progressCallback, new double(animator->progress()),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);

  player->setProgress(animator->progress());

  player->flush();
}
}  // namespace pag