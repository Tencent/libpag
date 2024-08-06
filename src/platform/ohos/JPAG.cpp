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

#include "JPAG.h"
#include "JsHelper.h"
#include "pag/pag.h"

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