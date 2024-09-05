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

#include "JPAGDiskCache.h"
#include <cstdint>
#include "NativePlatform.h"
#include "platform/ohos/JsHelper.h"
#include "rendering/caches/DiskCache.h"

namespace pag {
static napi_value MaxDiskSize(napi_env env, napi_callback_info) {
  napi_value result;
  napi_create_int64(env, PAGDiskCache::MaxDiskSize(), &result);
  return result;
}

static napi_value SetMaxDiskSize(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t diskSize = 0;
  napi_get_value_int64(env, args[0], &diskSize);
  PAGDiskCache::SetMaxDiskSize(diskSize);
  return nullptr;
}

static napi_value RemoveAll(napi_env, napi_callback_info) {
  PAGDiskCache::RemoveAll();
  return nullptr;
}

static napi_value SetCacheDir(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char cacheDir[1024] = {0};
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], cacheDir, 1024, &length);
  auto nativePlatform = const_cast<NativePlatform*>(
      reinterpret_cast<const NativePlatform*>(NativePlatform::Current()));
  nativePlatform->setCacheDir(cacheDir);
  return nullptr;
}

napi_value JPAGDiskCache::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  return result;
}

bool JPAGDiskCache::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {PAG_STATIC_METHOD_ENTRY(MaxDiskSize, MaxDiskSize),
                                          PAG_STATIC_METHOD_ENTRY(SetMaxDiskSize, SetMaxDiskSize),
                                          PAG_STATIC_METHOD_ENTRY(RemoveAll, RemoveAll),
                                          PAG_STATIC_METHOD_ENTRY(SetCacheDir, SetCacheDir)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}

}  // namespace pag