
#pragma once
#include <napi/native_api.h>
#include <pag/pag.h>
namespace pag {
class JsPAGLayerHandle {
 public:
  static bool Init(napi_env env, napi_value exports);
  static std::shared_ptr<PAGLayer> FromJs(napi_env env, napi_value);
  static napi_value ToJs(napi_env env, std::shared_ptr<PAGLayer> layer);
  static std::string GetLayerClassName(LayerType layer);
  static std::string GetFileClassName();
  static std::string GetBaseClassName();

  explicit JsPAGLayerHandle(std::shared_ptr<pag::PAGLayer> layer) : layer(layer) {
  }

  std::shared_ptr<pag::PAGLayer> get() {
    return layer;
  }

 private:
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
