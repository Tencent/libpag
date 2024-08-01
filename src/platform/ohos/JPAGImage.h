//
// Created on 2024/8/1.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#pragma once

#include <js_native_api_types.h>
#include <memory>
#include "pag/pag.h"
namespace pag {
class JPAGImage {
 public:
  static std::shared_ptr<PAGImage> FromJs(napi_env env, napi_value value);
};
}  // namespace pag
