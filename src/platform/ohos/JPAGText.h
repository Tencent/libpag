//
// Created on 2024/8/2.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#pragma once

#include <napi/native_api.h>
#include "pag/pag.h"
namespace pag {
class JPAGText {
 public:
  static bool Init(napi_env env, napi_value exports);
  static napi_value ToJs(napi_env env, const std::shared_ptr<TextDocument>& text);
  static std::shared_ptr<TextDocument> FromJs(napi_env env, napi_value value);
  static std::string ClassName() {
    return "JPAGText";
  }

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
};
}  // namespace pag
