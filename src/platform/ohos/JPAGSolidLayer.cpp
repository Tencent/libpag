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

#include "JPAGLayerHandle.h"
#include "JsHelper.h"
#include "pag/pag.h"

namespace pag {
static std::shared_ptr<PAGSolidLayer> FromJs(napi_env env, napi_value value) {
  auto layer = JPAGLayerHandle::FromJs(env, value);
  if (layer && layer->layerType() == LayerType::Solid) {
    return std::static_pointer_cast<PAGSolidLayer>(layer);
  }
  return nullptr;
}

static napi_value SolidColor(napi_env env, napi_callback_info info) {
  napi_value jsLayer = nullptr;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto solidLayer = FromJs(env, jsLayer);
  if (!solidLayer) {
    return 0;
  }

  napi_value result;
  Color color = solidLayer->solidColor();
  napi_create_int32(env, MakeColorInt(color.red, color.green, color.blue), &result);
  return result;
}

static napi_value SetSolidColor(napi_env env, napi_callback_info info) {
  napi_value jsLayer = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto solidLayer = FromJs(env, jsLayer);
  if (!solidLayer) {
    return nullptr;
  }
  int32_t color = 0;
  napi_get_value_int32(env, args[0], &color);
  solidLayer->setSolidColor(ToColor(color));
  return nullptr;
}

bool JPAGLayerHandle::InitPAGSolidLayerEnv(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {PAG_DEFAULT_METHOD_ENTRY(solidColor, SolidColor),
                                          PAG_DEFAULT_METHOD_ENTRY(setSolidColor, SetSolidColor)};

  auto status = DefineClass(env, exports, GetLayerClassName(LayerType::Solid),
                            sizeof(classProp) / sizeof(classProp[0]), classProp, Constructor,
                            GetBaseClassName());
  return status == napi_status::napi_ok;
}

}  // namespace pag
