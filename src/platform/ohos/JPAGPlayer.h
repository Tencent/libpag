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
#include "pag/pag.h"

namespace pag {

class JPAGPlayer {
 public:
  explicit JPAGPlayer(std::shared_ptr<pag::PAGPlayer> pagPlayer) : pagPlayer(pagPlayer) {
  }
  static bool Init(napi_env env, napi_value exports);
  static std::shared_ptr<PAGPlayer> FromJs(napi_env env, napi_value value);
  static napi_value ToJs(napi_env env, std::shared_ptr<PAGPlayer> player);
  static std::string ClassName() {
    return "JPAGPlayer";
  }
  std::shared_ptr<pag::PAGPlayer> get() {
    return pagPlayer;
  }

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
  std::shared_ptr<pag::PAGPlayer> pagPlayer;
};
}  // namespace pag
