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
    tgfx::PrintLog("JsPAGLayerHandle::ToJs napi_create_external failed :%d", status);
    return nullptr;
  }
  status = napi_new_instance(env, constructor, 1, external, &result);
  if (status != napi_ok) {
    tgfx::PrintLog("JsPAGLayerHandle::ToJs napi_new_instance failed :%d", status);
    return nullptr;
  }
  return result;
}

}  // namespace pag