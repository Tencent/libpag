
#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
EXTERN_C_START

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }

  
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }
    
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
  if ((component == nullptr) || (window == nullptr)) {
    return;
  }

  char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
  uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
  if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }
    
}

static napi_value Init(napi_env env, napi_value exports) {

  napi_value exportInstance = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
    return nullptr;
  }

  OH_NativeXComponent* nativeXComponent = nullptr;
  auto temp = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (temp != napi_ok) {
    return nullptr;
  }

  OH_NativeXComponent_Callback renderCallback;
  renderCallback.OnSurfaceCreated = OnSurfaceCreatedCB;
  renderCallback.OnSurfaceChanged = OnSurfaceChangedCB;
  renderCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;

  OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback);

  
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "testnative",
    .nm_priv = ((void*)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  napi_module_register(&demoModule);
}
