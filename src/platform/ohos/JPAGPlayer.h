//
// Created on 2024/7/16.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "napi/native_api.h"

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
