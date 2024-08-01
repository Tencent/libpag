//
// Created on 2024/7/23.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include <cstddef>
#include <cstdint>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
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
