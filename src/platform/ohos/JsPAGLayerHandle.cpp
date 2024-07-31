//
// Created on 2024/7/18.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsPAGLayerHandle.h"
#include "JsHelper.h"

namespace pag {
std::shared_ptr<PAGLayer> JsPAGLayerHandle::FromJs(napi_env env, napi_value value) {
  JsPAGLayerHandle* cLayerHandler = nullptr;
  auto status = napi_unwrap(env, value, (void**)&cLayerHandler);
  if (status == napi_ok && cLayerHandler) {
    return cLayerHandler->get();
  } else {
    return nullptr;
  }
}

napi_value JsPAGLayerHandle::ToJs(napi_env env, std::shared_ptr<PAGLayer> layer) {
  if (env == nullptr || layer == nullptr) {
    return nullptr;
  }
  std::string className = "";
  if (layer->isPAGFile()) {
    className = GetFileClassName();
  } else {
    className = GetLayerClassName(layer->layerType());
  }
  auto handler = new JsPAGLayerHandle(layer);
  auto result = NewInstance(env, className, handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}

std::string JsPAGLayerHandle::GetLayerClassName(LayerType layerType) {
  switch (layerType) {
    case LayerType::PreCompose:
      return "JsPAGComposition";
    case LayerType::Image:
      return "JsPAGImageLayer";
    case LayerType::Text:
      return "JsPAGTextLayer";
    case LayerType::Shape:
      return "JsPAGShapeLayer";
    case LayerType::Solid:
      return "JsPAGSolidLayer";
    default:
      return GetBaseClassName();
  }
}

std::string JsPAGLayerHandle::GetFileClassName() {
  return "JsPAGFile";
}

std::string JsPAGLayerHandle::GetBaseClassName() {
  return "JsPAGLayer";
}

bool JsPAGLayerHandle::InitPAGImageLayerEnv(napi_env env, napi_value exports) {
  return true;
}
bool JsPAGLayerHandle::InitPAGTextLayerEnv(napi_env env, napi_value exports) {
  return true;
}
bool JsPAGLayerHandle::InitPAGShapeLayerEnv(napi_env env, napi_value exports) {
  return true;
}
bool JsPAGLayerHandle::InitPAGSolidLayerEnv(napi_env env, napi_value exports) {
  return true;
}

bool JsPAGLayerHandle::InitPAGLayerEnv(napi_env env, napi_value exports) {
  //  napi_property_descriptor classProp[] = {0};
  auto status = DefineClass(env, exports, GetBaseClassName(), 0, nullptr,
                            ConstructorWithHandler<JsPAGLayerHandle>, "");
  return status == napi_ok;
}

bool JsPAGLayerHandle::Init(napi_env env, napi_value exports) {
  return InitPAGLayerEnv(env, exports) && InitPAGImageLayerEnv(env, exports) &&
         InitPAGTextLayerEnv(env, exports) && InitPAGSolidLayerEnv(env, exports) &&
         InitPAGShapeLayerEnv(env, exports) && InitPAGCompositionLayerEnv(env, exports) &&
         InitPAGFileEnv(env, exports);
}

}  // namespace pag