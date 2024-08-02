//
// Created on 2024/7/23.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JPAGSurface.h"

#include <cstdint>
#include "JsHelper.h"

namespace pag {

static napi_value Width(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JPAGSurface::FromJs(env, jSurface);
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
  auto surface = JPAGSurface::FromJs(env, jSurface);
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
  auto surface = JPAGSurface::FromJs(env, jSurface);
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
  auto surface = JPAGSurface::FromJs(env, jSurface);
  bool success = false;
  if (surface) {
    success = surface->clearAll();
  }
  napi_value result;
  napi_get_boolean(env, success, &result);
  return result;
}

static napi_value FreeCache(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JPAGSurface::FromJs(env, jSurface);
  if (surface) {
    surface->freeCache();
  }
  return nullptr;
}

static napi_value MakeOffscreen(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int32_t x = 0;
  napi_get_value_int32(env, args[0], &x);
  int32_t y = 0;
  napi_get_value_int32(env, args[1], &y);
  return JPAGSurface::ToJs(env, PAGSurface::MakeOffscreen(x, y));
}

static napi_value MakeSnapshot(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JPAGSurface::FromJs(env, jSurface);
  return MakeSnapshot(env, surface.get());
}

napi_value JPAGSurface::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  void* surface = nullptr;
  napi_get_value_external(env, args[0], &surface);
  napi_wrap(
      env, result, surface,
      [](napi_env, void* finalize_data, void*) {
        JPAGSurface* surface = static_cast<JPAGSurface*>(finalize_data);
        delete surface;
      },
      nullptr, nullptr);
  return result;
}

bool JPAGSurface::Init(napi_env env, napi_value exports) {
  static const napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(MakeOffscreen, MakeOffscreen),
      PAG_DEFAULT_METHOD_ENTRY(width, Width),
      PAG_DEFAULT_METHOD_ENTRY(height, Height),
      PAG_DEFAULT_METHOD_ENTRY(updateSize, UpdateSize),
      PAG_DEFAULT_METHOD_ENTRY(clearAll, ClearAll),
      PAG_DEFAULT_METHOD_ENTRY(freeCache, FreeCache),
      PAG_DEFAULT_METHOD_ENTRY(makeSnapshot, MakeSnapshot)};

  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");

  return status == napi_ok;
}

napi_value JPAGSurface::ToJs(napi_env env, std::shared_ptr<PAGSurface> surface) {
  if (!surface) {
    return nullptr;
  }
  JPAGSurface* handler = new JPAGSurface(surface);
  napi_value result = NewInstance(env, ClassName(), handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}

std::shared_ptr<PAGSurface> JPAGSurface::FromJs(napi_env env, napi_value value) {
  JPAGSurface* cSurface = nullptr;
  auto status = napi_unwrap(env, value, (void**)&cSurface);
  if (status == napi_ok) {
    return cSurface->get();
  } else {
    return nullptr;
  }
}
}  // namespace pag