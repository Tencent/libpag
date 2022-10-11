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

#if defined(_WIN32)

#include "EGLDisplayWrapper.h"
#include <EGL/eglext.h>
#include "WinUser.h"

namespace tgfx {
enum { D3D11, D3D11_FL_9_3, D3D11_WARP, PLAN_END };

// These are preferred display attributes and request ANGLE's D3D11
// renderer. eglInitialize will only succeed with these attributes if the
// hardware supports D3D11 Feature Level 10_0+.
const EGLint d3d11_display_attributes[] = {
    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

    // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that will
    // enable ANGLE to automatically call the IDXGIDevice3::Trim method on
    // behalf of the application when it gets suspended.
    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
    EGL_TRUE,

    // This extension allows angle to render directly on a D3D swapchain
    // in the correct orientation on D3D11.
    EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE,
    EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE,

    EGL_NONE,
};

// These are used to request ANGLE's D3D11 renderer, with D3D11 Feature
// Level 9_3.
const EGLint d3d11_fl_9_3_display_attributes[] = {
    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
    9,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
    3,
    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
    EGL_TRUE,
    EGL_NONE,
};

// These attributes request D3D11 WARP (software rendering fallback) in case
// hardware-backed D3D11 is unavailable.
const EGLint d3d11_warp_display_attributes[] = {
    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
    EGL_TRUE,
    EGL_NONE,
};

static int Win32EGLDisplayPlan = D3D11;

EGLDisplay EGLDisplayWrapper::EGLGetPlatformDisplay() {
  EGLDisplay display;

  auto hdc = GetDC(nullptr);

  auto eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
      eglGetProcAddress("eglGetPlatformDisplayEXT"));
  if (!eglGetPlatformDisplayEXT) {
    return nullptr;
  }

  if (Win32EGLDisplayPlan == D3D11) {
    display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, hdc, d3d11_display_attributes);
  } else if (Win32EGLDisplayPlan == D3D11_FL_9_3) {
    display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, hdc, d3d11_fl_9_3_display_attributes);
  } else if (Win32EGLDisplayPlan == D3D11_WARP) {
    display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, hdc, d3d11_warp_display_attributes);
  } else {
    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
  }

  if (display == EGL_NO_DISPLAY) {
    Win32EGLDisplayPlan++;
    return EGLDisplayWrapper::EGLGetPlatformDisplay();
  }

  return display;
}

bool EGLDisplayWrapper::HasNext() {
  Win32EGLDisplayPlan++;
  return Win32EGLDisplayPlan < PLAN_END;
}
}  // namespace tgfx
#endif