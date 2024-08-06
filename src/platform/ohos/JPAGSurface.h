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

#pragma once
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <cstddef>
#include <cstdint>
#include "pag/pag.h"

namespace pag {

class JPAGSurface {
 public:
  explicit JPAGSurface(std::shared_ptr<pag::PAGSurface> pagSurface) : pagSurface(pagSurface) {
  }
  static bool Init(napi_env env, napi_value exports);
  static std::shared_ptr<PAGSurface> FromJs(napi_env env, napi_value value);
  static napi_value ToJs(napi_env env, std::shared_ptr<PAGSurface> surface);
  static std::string ClassName() {
    return "JPAGSurface";
  }

  std::shared_ptr<pag::PAGSurface> get() {
    return pagSurface;
  }

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
  std::shared_ptr<pag::PAGSurface> pagSurface;
};

}  // namespace pag
