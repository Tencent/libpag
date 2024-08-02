//
// Created on 2024/7/18.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#include <napi/native_api.h>
#include <string>
#include "pag/pag.h"
#include "pag/types.h"

#define PAG_DEFAULT_METHOD_ENTRY(name, func)                               \
  {                                                                        \
#name, nullptr, func, nullptr, nullptr, nullptr, napi_default, nullptr \
  }
#define PAG_STATIC_METHOD_ENTRY(name, func)                               \
  {                                                                       \
#name, nullptr, func, nullptr, nullptr, nullptr, napi_static, nullptr \
  }

namespace pag {

bool Init(napi_env env, napi_value exports);

napi_status DefineClass(napi_env env, napi_value exports, const std::string& utf8name,
                        size_t propertyCount, const napi_property_descriptor* properties,
                        napi_callback constructor, const std::string& parentName);

napi_value GetConstructor(napi_env env, const std::string& name);

napi_value NewInstance(napi_env env, const std::string& name, void* handler);

napi_value CreateMarkers(napi_env env, const std::vector<const Marker*>& markers);

napi_value CreateRect(napi_env env, const Rect& rect);

Rect GetRect(napi_env env, napi_value value);

napi_value CreateMatrix(napi_env env, const Matrix& matrix);

Matrix GetMatrix(napi_env env, napi_value value);

napi_value MakeSnapshot(napi_env env, PAGSurface* surface);

int MakeColorInt(uint32_t red, uint32_t green, uint32_t blue);

Color ToColor(int value);

napi_value CreateVideoRanges(napi_env env, const std::vector<PAGVideoRange>& videoRanges);

}  // namespace pag
