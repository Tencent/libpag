//
// Created on 2024/7/23.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsPAGSurface.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstdint>

#include "JsHelper.h"
#include "platform/ohos/GPUDrawable.h"

namespace pag {

static std::unordered_map<std::string, std::shared_ptr<pag::PAGSurface>> SurfaceMap = {};

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
  auto drawable = pag::GPUDrawable::FromXComponent(component, window);
  if (drawable) {
    SurfaceMap[id] = pag::PAGSurface::MakeFrom(drawable);
  } else {
    SurfaceMap.erase(id);
  }
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
  if (SurfaceMap.find(id) != SurfaceMap.end()) {
    SurfaceMap[id]->updateSize();
  }
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
  if (SurfaceMap.find(id) != SurfaceMap.end()) {
    SurfaceMap[id]->clearAll();
    SurfaceMap.erase(id);
  }
}

static void DispatchTouchEventCB(OH_NativeXComponent*, void*) {
}

static napi_value Width(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JsPAGSurface::FromJs(env, jSurface);
  int width = 0;
  if (surface) {
    width = surface->width();
  }
  napi_value result;
  napi_create_int32(env, width, &result);
  return result;
}

static napi_value Height(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JsPAGSurface::FromJs(env, jSurface);
  int height = 0;
  if (surface) {
    height = surface->height();
  }
  napi_value result;
  napi_create_int32(env, height, &result);
  return result;
}

static napi_value UpdateSize(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JsPAGSurface::FromJs(env, jSurface);
  if (surface) {
    surface->updateSize();
  }
  return nullptr;
}

static napi_value ClearAll(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JsPAGSurface::FromJs(env, jSurface);
  if (surface) {
    surface->clearAll();
  }
  return nullptr;
}

static napi_value FreeCache(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JsPAGSurface::FromJs(env, jSurface);
  if (surface) {
    surface->freeCache();
  }
  return nullptr;
}

static napi_value GetSurface(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc != 1) {
    return nullptr;
  }
  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], idStr, OH_XCOMPONENT_ID_LEN_MAX, &length);
  std::string id = {idStr, length};
  if (SurfaceMap.find(id) != SurfaceMap.end()) {
    return pag::JsPAGSurface::ToJs(env, SurfaceMap[id]);
  }
  return nullptr;
}

static napi_value SetupOffscreen(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int32_t x = 0;
  napi_get_value_int32(env, args[0], &x);
  int32_t y = 0;
  napi_get_value_int32(env, args[1], &y);
  return JsPAGSurface::ToJs(env, PAGSurface::MakeOffscreen(x, y));
}

bool JsPAGSurface::Init(napi_env env, napi_value exports) {
  static const napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(GetSurface, GetSurface),
      PAG_STATIC_METHOD_ENTRY(SetupOffscreen, SetupOffscreen),

      PAG_DEFAULT_METHOD_ENTRY(width, Width),
      PAG_DEFAULT_METHOD_ENTRY(height, Height),
      PAG_DEFAULT_METHOD_ENTRY(updateSize, UpdateSize),
      PAG_DEFAULT_METHOD_ENTRY(clearAll, ClearAll),
      PAG_DEFAULT_METHOD_ENTRY(freeCache, FreeCache)};

  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, ConstructorWithHandler<JsPAGSurface>, "");

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

napi_value JsPAGSurface::ToJs(napi_env env, std::shared_ptr<PAGSurface> surface) {
  if (!surface) {
    return nullptr;
  }
  JsPAGSurface* handler = new JsPAGSurface(surface);
  napi_value result = NewInstance(env, ClassName(), handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}

std::shared_ptr<PAGSurface> JsPAGSurface::FromJs(napi_env env, napi_value value) {
  JsPAGSurface* cSurface = nullptr;
  auto status = napi_unwrap(env, value, (void**)&cSurface);
  if (status == napi_ok) {
    return cSurface->get();
  } else {
    return nullptr;
  }
}
}  // namespace pag