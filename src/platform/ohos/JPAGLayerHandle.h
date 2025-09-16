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

#pragma once
#include <napi/native_api.h>
#include <pag/pag.h>

namespace pag {
class JPAGLayerHandle {
 public:
  static bool Init(napi_env env, napi_value exports);
  static std::shared_ptr<PAGLayer> FromJs(napi_env env, napi_value);
  static napi_value ToJs(napi_env env, std::shared_ptr<PAGLayer> layer);
  static std::string GetLayerClassName(LayerType layer);
  static std::string GetFileClassName();
  static std::string GetBaseClassName();

  explicit JPAGLayerHandle(std::shared_ptr<pag::PAGLayer> layer) : layer(layer) {
  }

  std::shared_ptr<pag::PAGLayer> get() {
    return layer;
  }

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
  static bool InitPAGLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGImageLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGTextLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGShapeLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGSolidLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGCompositionLayerEnv(napi_env env, napi_value exports);
  static bool InitPAGFileEnv(napi_env env, napi_value exports);

  std::shared_ptr<pag::PAGLayer> layer;
};

}  // namespace pag
