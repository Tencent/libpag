
#include "platform/ohos/JsHelper.h"
#include "napi/native_api.h"
EXTERN_C_START

static napi_value Init(napi_env env, napi_value exports) {
  pag::Init(env, exports);
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    1, 0, nullptr, Init, "pag", ((void*)0), {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  napi_module_register(&demoModule);
}
