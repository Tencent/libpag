//
// Created on 2024/8/1.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#pragma once
#include <string>
#include "napi/native_api.h"
#include "pag/pag.h"

namespace pag {
class JPAGFont {
 public:
  static bool Init(napi_env env, napi_value exports);
  static napi_value ToJs(napi_env env, const PAGFont& font);
  static PAGFont FromJs(napi_env env, napi_value value);
  static std::string ClassName() {
    return "JPAGFont";
  }

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);

};
}  // namespace pag