//
// Created on 2024/7/18.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsHelper.h"
#include "JsPAGLayerHandle.h"
#include "JsPAGPlayer.h"
#include "JsPAGSurface.h"
#include "base/utils/Log.h"
#include "platform/ohos/JsPAGView.h"

namespace pag {

static std::unordered_map<std::string, napi_ref> ConstructorRefMap;

bool Init(napi_env env, napi_value exports) {
  return JsPAGLayerHandle::Init(env, exports) && JsPAGPlayer::Init(env, exports) &&
         JsPAGSurface::Init(env, exports) && JsPAGView::Init(env, exports);
}

bool SetConstructor(napi_env env, napi_value constructor, const std::string& name) {
  if (env == nullptr || constructor == nullptr || name.empty()) {
    return false;
  }
  if (ConstructorRefMap.find(name) != ConstructorRefMap.end()) {
    napi_delete_reference(env, ConstructorRefMap.at(name));
    ConstructorRefMap.erase(name);
  }
  napi_ref ref;
  napi_create_reference(env, constructor, 1, &ref);
  ConstructorRefMap[name] = ref;
  return true;
}

napi_value GetConstructor(napi_env env, const std::string& name) {
  if (env == nullptr || name.empty()) {
    return nullptr;
  }
  napi_value result = nullptr;
  if (ConstructorRefMap.find(name) != ConstructorRefMap.end()) {
    napi_get_reference_value(env, ConstructorRefMap.at(name), &result);
  }
  return result;
}

napi_status ExtendClass(napi_env env, napi_value constructor, const std::string& parentName) {
  if (env == nullptr || constructor == nullptr || parentName.empty()) {
    return napi_status::napi_invalid_arg;
  }
  napi_value baseConstructor = GetConstructor(env, parentName);
  if (baseConstructor == nullptr) {
    tgfx::PrintLog("ExtendClass get baseConstructor failed status");
    return napi_status::napi_invalid_arg;
  }
  napi_value basePrototype;
  napi_status statusCode =
      napi_get_named_property(env, baseConstructor, "prototype", &basePrototype);
  if (statusCode != napi_status::napi_ok) {
    tgfx::PrintLog("ExtendClass get baseConstructor's prototype  failed status :%d", statusCode);
    return statusCode;
  }
  napi_value derivedPrototype;
  statusCode = napi_get_named_property(env, constructor, "prototype", &derivedPrototype);
  if (statusCode != napi_status::napi_ok) {
    tgfx::PrintLog("ExtendClass get constructor's prototype  failed status :%d", statusCode);
    return statusCode;
  }
  return napi_set_named_property(env, derivedPrototype, "__proto__", basePrototype);
}

napi_status DefineClass(napi_env env, napi_value exports, const std::string& utf8name,
                        size_t propertyCount, const napi_property_descriptor* properties,
                        napi_callback constructor, const std::string& parentName) {
  napi_value classConstructor;
  auto status = napi_define_class(env, utf8name.c_str(), utf8name.length(), constructor, nullptr,
                                  propertyCount, properties, &classConstructor);
  if (status != napi_status::napi_ok) {
    tgfx::PrintLog("DefineClass napi_define_class failed:%d", status);
    return status;
  }
  status = napi_set_named_property(env, exports, utf8name.c_str(), classConstructor);
  if (status != napi_status::napi_ok) {
    tgfx::PrintLog("DefineClass napi_set_named_property failed:%d", status);
    return status;
  }
  SetConstructor(env, classConstructor, utf8name);
  if (parentName.empty()) {
    return status;
  }
  status = ExtendClass(env, classConstructor, parentName);
  if (status != napi_status::napi_ok) {
    tgfx::PrintLog("DefineClass ExtendClass failed:%d", status);
    return status;
  }
  return status;
}

napi_value NewInstance(napi_env env, const std::string& name, void* handler) {
  if (env == nullptr || handler == nullptr || name.empty()) {
    return nullptr;
  }
  napi_value constructor = GetConstructor(env, name);
  if (constructor == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  napi_value external[1];
  auto status = napi_create_external(env, handler, nullptr, nullptr, &external[0]);
  if (status != napi_ok) {
    tgfx::PrintLog("NewInstance napi_create_external failed :%d", status);
    return nullptr;
  }
  status = napi_new_instance(env, constructor, 1, external, &result);
  if (status != napi_ok) {
    tgfx::PrintLog("NewInstance napi_new_instance failed :%d", status);
    return nullptr;
  }
  return result;
}

napi_value CreateMarker(napi_env env, const Marker* marker) {
  if (env == nullptr || marker == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  auto status = napi_create_object(env, &result);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_create_object failed :%d", status);
    return nullptr;
  }
  napi_value startTime;
  status = napi_create_int64(env, marker->startTime, &startTime);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "startTime", startTime);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value duration;
  status = napi_create_int64(env, marker->duration, &duration);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "duration", duration);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value comment;
  status =
      napi_create_string_utf8(env, marker->comment.c_str(), marker->comment.length(), &comment);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_create_string_utf8 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "comment", comment);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  return result;
}

napi_value CreateMarkers(napi_env env, const std::vector<const Marker*>& markers) {
  napi_value result;
  auto status = napi_create_array_with_length(env, markers.size(), &result);
  if (status != napi_ok) {
    return nullptr;
  }
  for (size_t i = 0; i < markers.size(); i++) {
    auto jsMarker = CreateMarker(env, markers[i]);
    if (jsMarker == nullptr) {
      return nullptr;
    }
    napi_set_element(env, result, i, jsMarker);
  }
  return result;
}

napi_value CreateRect(napi_env env, const Rect& rect) {
  if (env == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  auto status = napi_create_array_with_length(env, 4, &result);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateRect napi_create_array_with_length failed :%d", status);
    return nullptr;
  }
  napi_value l;
  status = napi_create_double(env, rect.left, &l);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateRect napi_create_int64 failed :%d", status);
    return nullptr;
  }
  napi_value t;
  status = napi_create_double(env, rect.top, &t);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }
  napi_value r;
  status = napi_create_double(env, rect.right, &r);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }

  napi_value b;
  status = napi_create_double(env, rect.bottom, &b);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }

  napi_set_element(env, result, 0, l);
  napi_set_element(env, result, 1, t);
  napi_set_element(env, result, 2, r);
  napi_set_element(env, result, 3, b);
  return result;
}

napi_value CreateMatrix(napi_env env, const Matrix& matrix) {
  if (env == nullptr) {
    return nullptr;
  }
  float buffer[9];
  matrix.get9(buffer);
  napi_value result = nullptr;
  auto status = napi_create_array_with_length(env, 9, &result);
  if (status != napi_ok) {
    tgfx::PrintLog("CreateMatrix napi_create_array_with_length failed :%d", status);
    return nullptr;
  }
  for (size_t i = 0; i < 9; i++) {
    napi_value ele;
    status = napi_create_double(env, buffer[i], &ele);
    if (status != napi_ok) {
      tgfx::PrintLog("CreateMatrix napi_create_int64 failed :%d", status);
      return nullptr;
    }
    status = napi_set_element(env, result, i, ele);
    if (status != napi_ok) {
      tgfx::PrintLog("CreateMatrix napi_set_element failed :%d", status);
      return nullptr;
    }
  }
  return result;
}

Matrix GetMatrix(napi_env env, napi_value value) {
  if (env == nullptr || value == nullptr) {
    return {};
  }

  Matrix result;
  for (size_t i = 0; i < 9; i++) {
    napi_value ele;
    auto status = napi_get_element(env, value, i, &ele);
    if (status != napi_ok) {
      tgfx::PrintLog("GetMatrix napi_get_element failed :%d", status);
      return {};
    }
    double val = 0;
    status = napi_get_value_double(env, ele, &val);
    if (status != napi_ok) {
      tgfx::PrintLog("GetMatrix napi_get_value_double failed :%d", status);
      return {};
    }
    result.set(i, val);
  }
  return result;
}

}  // namespace pag