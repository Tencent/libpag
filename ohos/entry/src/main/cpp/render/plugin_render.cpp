/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdint>
#include <hilog/log.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <string>

#include "../common/common.h"
#include "../manager/plugin_manager.h"
#include "plugin_render.h"

namespace NativeXComponentSample {
namespace {
void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceCreatedCB");
    if ((component == nullptr) || (window == nullptr)) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceCreatedCB: component or window is null");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceCreatedCB: Unable to get XComponent id");
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    uint64_t width;
    uint64_t height;
    int32_t xSize = OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    if ((xSize == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) && (render != nullptr)) {
        if (render->eglCore_->EglContextInit(window, width, height)) {
            render->eglCore_->Background();
        }
    }
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceChangedCB");
    if ((component == nullptr) || (window == nullptr)) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceChangedCB: component or window is null");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceChangedCB: Unable to get XComponent id");
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnSurfaceChanged(component, window);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "surface changed");
    }
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceDestroyedCB");
    if ((component == nullptr) || (window == nullptr)) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceDestroyedCB: component or window is null");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceDestroyedCB: Unable to get XComponent id");
        return;
    }

    std::string id(idStr);
    PluginRender::Release(id);
}

void DispatchTouchEventCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "DispatchTouchEventCB");
    if ((component == nullptr) || (window == nullptr)) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "DispatchTouchEventCB: component or window is null");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "DispatchTouchEventCB: Unable to get XComponent id");
        return;
    }

    std::string id(idStr);
    PluginRender* render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnTouchEvent(component, window);
    }
}

void DispatchMouseEventCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "DispatchMouseEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnMouseEvent(component, window);
    }
}

void DispatchHoverEventCB(OH_NativeXComponent* component, bool isHover)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "DispatchHoverEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnHoverEvent(component, isHover);
    }
}

void OnFocusEventCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnFocusEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnFocusEvent(component, window);
    }
}

void OnBlurEventCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnBlurEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnBlurEvent(component, window);
    }
}

void OnKeyEventCB(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Callback", "OnKeyEventCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }
    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnKeyEvent(component, window);
    }
}
} // namespace

std::unordered_map<std::string, PluginRender*> PluginRender::instance_;
int32_t PluginRender::hasDraw_ = 0;
int32_t PluginRender::hasChangeColor_ = 0;

PluginRender::PluginRender(std::string& id)
{
    this->id_ = id;
    this->eglCore_ = new EGLCore();
}

PluginRender* PluginRender::GetInstance(std::string& id)
{
    if (instance_.find(id) == instance_.end()) {
        PluginRender* instance = new PluginRender(id);
        instance_[id] = instance;
        return instance;
    } else {
        return instance_[id];
    }
}

void PluginRender::Export(napi_env env, napi_value exports)
{
    if ((env == nullptr) || (exports == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "Export: env or exports is null");
        return;
    }

    napi_property_descriptor desc[] = {
        {"drawPattern", nullptr, PluginRender::NapiDrawPattern, nullptr, nullptr,
         nullptr, napi_default, nullptr},
        {"getStatus", nullptr, PluginRender::TestGetXComponentStatus, nullptr, nullptr,
         nullptr, napi_default, nullptr}};
    if (napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "Export: napi_define_properties failed");
    }
}

// NAPI registration method type napi_callback. If no value is returned, nullptr is returned.
napi_value PluginRender::NapiDrawPattern(napi_env env, napi_callback_info info)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern");
    if ((env == nullptr) || (info == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern: env or info is null");
        return nullptr;
    }

    napi_value thisArg;
    if (napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, nullptr) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern: napi_get_cb_info fail");
        return nullptr;
    }

    napi_value exportInstance;
    if (napi_get_named_property(env, thisArg, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern: napi_get_named_property fail");
        return nullptr;
    }

    OH_NativeXComponent* nativeXComponent = nullptr;
    if (napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent)) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern: napi_unwrap fail");
        return nullptr;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "NapiDrawPattern: Unable to get XComponent id");
        return nullptr;
    }

    std::string id(idStr);
    PluginRender* render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->eglCore_->Draw(hasDraw_);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "render->eglCore_->Draw() executed");
    }
    return nullptr;
}

void PluginRender::Release(std::string& id)
{
    PluginRender* render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->eglCore_->Release();
        delete render->eglCore_;
        render->eglCore_ = nullptr;
        instance_.erase(instance_.find(id));
    }
}

void PluginRender::OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "OnSurfaceChanged: Unable to get XComponent id");
        return;
    }

    std::string id(idStr);
    PluginRender* render = PluginRender::GetInstance(id);
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "OH_NativeXComponent_GetXComponentOffset",
        "offsetX = %{public}lf, offsetY = %{public}lf", offsetX, offsetY);
    uint64_t width;
    uint64_t height;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    if (render != nullptr) {
        render->eglCore_->UpdateSize(width, height);
    }
}

void PluginRender::OnTouchEvent(OH_NativeXComponent* component, void* window)
{
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = { '\0' };
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Callback", "DispatchTouchEventCB: Unable to get XComponent id");
        return;
    }
    OH_NativeXComponent_TouchEvent touchEvent;
    OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
    std::string id(idStr);
    PluginRender* render = PluginRender::GetInstance(id);
    if (render != nullptr && touchEvent.type == OH_NativeXComponent_TouchEventType::OH_NATIVEXCOMPONENT_UP) {
        render->eglCore_->ChangeColor(hasChangeColor_);
    }
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    OH_NativeXComponent_TouchPointToolType toolType =
        OH_NativeXComponent_TouchPointToolType::OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN;
    OH_NativeXComponent_GetTouchPointToolType(component, 0, &toolType);
    OH_NativeXComponent_GetTouchPointTiltX(component, 0, &tiltX);
    OH_NativeXComponent_GetTouchPointTiltY(component, 0, &tiltY);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "OnTouchEvent",
        "touch info: toolType = %{public}d, tiltX = %{public}lf, tiltY = %{public}lf", toolType, tiltX, tiltY);
}

void PluginRender::RegisterCallback(OH_NativeXComponent* nativeXComponent)
{
    renderCallback_.OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback_.OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback_.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    renderCallback_.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback_);

    mouseCallback_.DispatchMouseEvent = DispatchMouseEventCB;
    mouseCallback_.DispatchHoverEvent = DispatchHoverEventCB;
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &mouseCallback_);

    OH_NativeXComponent_RegisterFocusEventCallback(nativeXComponent, OnFocusEventCB);
    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, OnKeyEventCB);
    OH_NativeXComponent_RegisterBlurEventCallback(nativeXComponent, OnBlurEventCB);
}

void PluginRender::OnMouseEvent(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "OnMouseEvent");
    OH_NativeXComponent_MouseEvent mouseEvent;
    int32_t ret = OH_NativeXComponent_GetMouseEvent(component, window, &mouseEvent);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender",
            "MouseEvent Info: x = %{public}f, y = %{public}f, action = %{public}d, button = %{public}d", mouseEvent.x,
            mouseEvent.y, mouseEvent.action, mouseEvent.button);
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "GetMouseEvent error");
    }
}

void PluginRender::OnHoverEvent(OH_NativeXComponent* component, bool isHover)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "OnHoverEvent isHover_ = %{public}d", isHover);
}

void PluginRender::OnFocusEvent(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "OnFocusEvent");
}

void PluginRender::OnBlurEvent(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "OnBlurEvent");
}

void PluginRender::OnKeyEvent(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender", "OnKeyEvent");

    OH_NativeXComponent_KeyEvent* keyEvent = nullptr;
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) >= 0) {
        OH_NativeXComponent_KeyAction action;
        OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
        OH_NativeXComponent_KeyCode code;
        OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);
        OH_NativeXComponent_EventSourceType sourceType;
        OH_NativeXComponent_GetKeyEventSourceType(keyEvent, &sourceType);
        int64_t deviceId;
        OH_NativeXComponent_GetKeyEventDeviceId(keyEvent, &deviceId);
        int64_t timeStamp;
        OH_NativeXComponent_GetKeyEventTimestamp(keyEvent, &timeStamp);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginRender",
            "KeyEvent Info: action=%{public}d, code=%{public}d, sourceType=%{public}d, deviceId=%{public}ld, "
            "timeStamp=%{public}ld",
            action, code, sourceType, deviceId, timeStamp);
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginRender", "GetKeyEvent error");
    }
}

napi_value PluginRender::TestGetXComponentStatus(napi_env env, napi_callback_info info)
{
    napi_value hasDraw;
    napi_value hasChangeColor;

    napi_status ret = napi_create_int32(env, hasDraw_, &(hasDraw));
    if (ret != napi_ok) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "TestGetXComponentStatus", "napi_create_int32 hasDraw_ error");
        return nullptr;
    }
    ret = napi_create_int32(env, hasChangeColor_, &(hasChangeColor));
    if (ret != napi_ok) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "TestGetXComponentStatus", "napi_create_int32 hasChangeColor_ error");
        return nullptr;
    }

    napi_value obj;
    ret = napi_create_object(env, &obj);
    if (ret != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "TestGetXComponentStatus", "napi_create_object error");
        return nullptr;
    }
    ret = napi_set_named_property(env, obj, "hasDraw", hasDraw);
    if (ret != napi_ok) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "TestGetXComponentStatus", "napi_set_named_property hasDraw error");
        return nullptr;
    }
    ret = napi_set_named_property(env, obj, "hasChangeColor", hasChangeColor);
    if (ret != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "TestGetXComponentStatus",
            "napi_set_named_property hasChangeColor error");
        return nullptr;
    }
    return obj;
}
} // namespace NativeXComponentSample
