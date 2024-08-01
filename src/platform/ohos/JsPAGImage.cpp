//
// Created on 2024/8/1.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsPAGImage.h"

namespace pag {
std::shared_ptr<PAGImage> JsPAGImage::FromJs(napi_env, napi_value) {
  return nullptr;
}
}  // namespace pag