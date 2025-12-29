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

#include "JPAG.h"
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "platform/ohos/JPAG.h"
#include "platform/ohos/JPAGDiskCache.h"
#include "platform/ohos/JPAGFont.h"
#include "platform/ohos/JPAGImage.h"
#include "platform/ohos/JPAGImageView.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JPAGPlayer.h"
#include "platform/ohos/JPAGSurface.h"
#include "platform/ohos/JPAGText.h"
#include "platform/ohos/JPAGView.h"
#include "platform/ohos/JsHelper.h"
#include "platform/ohos/NativeDisplayLink.h"

namespace pag {
static napi_value SDKVersion(napi_env env, napi_callback_info) {
  napi_value value;
  auto version = PAG::SDKVersion();
  napi_create_string_utf8(env, version.c_str(), version.length(), &value);
  return value;
}

napi_value JPAG::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  return result;
}

bool JPAG::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {PAG_STATIC_METHOD_ENTRY(SDKVersion, SDKVersion)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}
}  // namespace pag

EXTERN_C_START

static napi_value Init(napi_env env, napi_value exports) {
  bool result = pag::JPAG::Init(env, exports) && pag::JPAGLayerHandle::Init(env, exports) &&
                pag::JPAGImage::Init(env, exports) && pag::JPAGPlayer::Init(env, exports) &&
                pag::JPAGSurface::Init(env, exports) && pag::JPAGFont::Init(env, exports) &&
                pag::JPAGText::Init(env, exports) && pag::JPAGImage::Init(env, exports) &&
                pag::JPAGView::Init(env, exports) && pag::JPAGImageView::Init(env, exports) &&
                pag::JPAGDiskCache::Init(env, exports) &&
                pag::XComponentHandler::Init(env, exports) &&
                pag::NativeDisplayLink::InitThreadSafeFunction(env);
  if (!result) {
    LOGE("PAG InitFailed");
  }
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    1, 0, nullptr, Init, "pag", ((void*)0), {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  napi_module_register(&demoModule);
}