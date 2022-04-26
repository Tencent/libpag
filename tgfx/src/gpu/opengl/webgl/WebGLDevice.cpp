/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/gpu/opengl/webgl/WebGLDevice.h"
#include "WebGLProcGetter.h"
#include "core/utils/Log.h"

namespace tgfx {

void* GLDevice::CurrentNativeHandle() {
  return reinterpret_cast<void*>(emscripten_webgl_get_current_context());
}

std::shared_ptr<GLDevice> GLDevice::Current() {
  auto context = emscripten_webgl_get_current_context();
  auto glDevice = GLDevice::Get(reinterpret_cast<void*>(context));
  if (glDevice) {
    return std::static_pointer_cast<WebGLDevice>(glDevice);
  }
  return WebGLDevice::Wrap(context, true);
}

std::shared_ptr<GLDevice> GLDevice::Make(void*) {
  return nullptr;
}

std::shared_ptr<WebGLDevice> WebGLDevice::MakeFrom(const std::string& canvasID) {
  auto oldContext = emscripten_webgl_get_current_context();

  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.depth = EM_FALSE;
  attrs.stencil = EM_FALSE;
  attrs.antialias = EM_FALSE;
  auto context = emscripten_webgl_create_context(canvasID.c_str(), &attrs);
  if (context == 0) {
    LOGE("WebGLDevice::MakeFrom emscripten_webgl_create_context error");
    return nullptr;
  }
  auto result = emscripten_webgl_make_context_current(context);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    emscripten_webgl_destroy_context(context);
    if (oldContext) {
      emscripten_webgl_make_context_current(oldContext);
    }
    return nullptr;
  }
  emscripten_webgl_make_context_current(0);
  if (oldContext) {
    emscripten_webgl_make_context_current(oldContext);
  }
  return WebGLDevice::Wrap(context);
}

std::shared_ptr<WebGLDevice> WebGLDevice::Wrap(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglContext,
                                               bool isAdopted) {
  if (webglContext == 0) {
    return nullptr;
  }
  auto oldContext = emscripten_webgl_get_current_context();
  if (oldContext != webglContext) {
    auto result = emscripten_webgl_make_context_current(webglContext);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
      emscripten_webgl_make_context_current(0);
      if (oldContext) {
        emscripten_webgl_make_context_current(oldContext);
      }
      return nullptr;
    }
  }
  auto device = std::shared_ptr<WebGLDevice>(new WebGLDevice(webglContext));
  device->isAdopted = isAdopted;
  device->context = webglContext;
  device->weakThis = device;
  if (oldContext != webglContext) {
    emscripten_webgl_make_context_current(0);
    if (oldContext) {
      emscripten_webgl_make_context_current(oldContext);
    }
  }
  return device;
}

WebGLDevice::WebGLDevice(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE nativeHandle)
    : GLDevice(reinterpret_cast<void*>(nativeHandle)) {
}

WebGLDevice::~WebGLDevice() {
  releaseAll();
  if (isAdopted) {
    return;
  }
  emscripten_webgl_destroy_context(context);
}

bool WebGLDevice::onMakeCurrent() {
  oldContext = emscripten_webgl_get_current_context();
  if (oldContext == context) {
    // 如果外部已经设定好了当前的 Context，以外部设定的为准，不再切换。
    return true;
  }
  auto result = emscripten_webgl_make_context_current(context);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    LOGE("WebGLDevice::onMakeCurrent failure result = %d", result);
    return false;
  }
  return true;
}

void WebGLDevice::onClearCurrent() {
  emscripten_webgl_make_context_current(0);
  if (oldContext) {
    emscripten_webgl_make_context_current(oldContext);
  }
}

bool WebGLDevice::sharableWith(void* nativeContext) const {
  return nativeHandle == nativeContext;
}
}  // namespace tgfx
