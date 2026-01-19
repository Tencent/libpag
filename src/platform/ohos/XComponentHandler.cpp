/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "XComponentHandler.h"
#include <ace/xcomponent/native_interface_xcomponent.h>

namespace pag {

std::unordered_map<std::string, std::weak_ptr<XComponentListener>> XComponentListeners;

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

  std::string id(idStr);
  if (XComponentListeners.find(id) == XComponentListeners.end()) {
    return;
  }
  auto listener = XComponentListeners[id].lock();
  if (listener) {
    listener->onSurfaceCreated(static_cast<NativeWindow*>(window));
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
  std::string id(idStr);
  auto listener = XComponentListeners[id].lock();
  if (listener) {
    listener->onSurfaceSizeChanged();
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

  std::string id(idStr);
  auto listener = XComponentListeners[id].lock();
  if (listener) {
    listener->onSurfaceDestroyed();
  }
}

static void DispatchTouchEventCB(OH_NativeXComponent*, void*) {
}

bool XComponentHandler::Init(napi_env env, napi_value exports) {
  napi_value exportInstance = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
    return true;
  }

  OH_NativeXComponent* nativeXComponent = nullptr;
  auto status = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (status != napi_ok) {
    return true;
  }

  static OH_NativeXComponent_Callback renderCallback;
  renderCallback.OnSurfaceCreated = OnSurfaceCreatedCB;
  renderCallback.OnSurfaceChanged = OnSurfaceChangedCB;
  renderCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
  renderCallback.DispatchTouchEvent = DispatchTouchEventCB;
  OH_NativeXComponent_RegisterCallback(nativeXComponent, &renderCallback);
  return true;
}

bool XComponentHandler::AddListener(const std::string& xComponentID,
                                    std::weak_ptr<XComponentListener> listener) {
  if (XComponentListeners.find(xComponentID) != XComponentListeners.end()) {
    return false;
  }
  XComponentListeners[xComponentID] = listener;
  return true;
}

bool XComponentHandler::RemoveListener(const std::string& xComponentID) {
  if (XComponentListeners.find(xComponentID) == XComponentListeners.end()) {
    return false;
  }
  XComponentListeners.erase(xComponentID);
  return true;
}

}  // namespace pag
