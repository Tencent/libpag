#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include "egl_core.h"

using namespace NativeXComponentSample;

static EGLCore eglCore;

static napi_value OnDraw(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double value;
    napi_get_value_double(env, args[0], &value);
    auto drawIndex = static_cast<int>(value);
    if (drawIndex % 2 == 0) {
        int hasChangeColor = 0;
        eglCore.ChangeColor(hasChangeColor);
    } else {
        int hasDraw = 0;
        eglCore.Draw(hasDraw);
    }
    return nullptr;
}

static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {}

static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {}

static void DispatchTouchEventCB(OH_NativeXComponent *component, void *window) {}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    uint64_t width;
    uint64_t height;
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }
    eglCore.EglContextInit(window, width, height);
    eglCore.Background();
    int hasDraw = 0;
    eglCore.Draw(hasDraw);
}

static void RegisterCallback(napi_env env, napi_value exports) {
    napi_status status;
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;
    status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        return;
    }
    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        return;
    }
    static OH_NativeXComponent_Callback callback;
    callback.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback.OnSurfaceChanged = OnSurfaceChangedCB;
    callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {{"draw", nullptr, OnDraw, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    RegisterCallback(env, exports);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "hello2d",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterHello2dModule(void) { napi_module_register(&demoModule); }
