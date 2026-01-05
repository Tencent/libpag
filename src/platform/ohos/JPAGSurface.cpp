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

#include "JPAGSurface.h"
#include <native_window/external_window.h>
#include <cstdint>
#include "base/utils/Log.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JsHelper.h"

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

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value jSurface = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &jSurface, nullptr);
  auto surface = JPAGSurface::FromJs(env, jSurface);
  if (surface) {
    surface->freeCache();
  }
  void* data = nullptr;
  napi_remove_wrap(env, jSurface, &data);
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

static napi_value FromSurfaceID(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t surfaceId = 0;
  napi_get_value_int64(env, args[0], &surfaceId);
  OHNativeWindow* window = nullptr;
  int ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
  if (ret != 0) {
    LOGE("Could not Create Native Window From Surface Id:%lld", surfaceId);
    return nullptr;
  }
  auto drawable = pag::GPUDrawable::FromWindow(window);
  return JPAGSurface::ToJs(env, pag::PAGSurface::MakeFrom(drawable));
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
      PAG_STATIC_METHOD_ENTRY(FromSurfaceID, FromSurfaceID),
      PAG_DEFAULT_METHOD_ENTRY(width, Width),
      PAG_DEFAULT_METHOD_ENTRY(height, Height),
      PAG_DEFAULT_METHOD_ENTRY(updateSize, UpdateSize),
      PAG_DEFAULT_METHOD_ENTRY(clearAll, ClearAll),
      PAG_DEFAULT_METHOD_ENTRY(freeCache, FreeCache),
      PAG_DEFAULT_METHOD_ENTRY(release, Release),
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
