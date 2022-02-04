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

#include "platform/android/GLHardwareTexture.h"
#include <android/hardware_buffer.h>
#include "platform/android/NativeHardwareBuffer.h"
#include "platform/android/NativeHardwareBufferInterface.h"

namespace pag {

// The Android NDK doesn't exposes extensions, fake it with eglGetProcAddress
namespace glext {
PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID = {};
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = {};
PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = {};
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = {};
}  // namespace glext
using namespace glext;

std::once_flag sApiLevelOnceFlag;

static bool initGlextProc() {
  std::call_once(sApiLevelOnceFlag, []() {
    glext::eglGetNativeClientBufferANDROID =
        (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)eglGetProcAddress(
            "eglGetNativeClientBufferANDROID");
    // for AHardwareBuffer
    glext::glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    glext::eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    glext::eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  });
  return glext::glEGLImageTargetTexture2DOES && glext::eglCreateImageKHR &&
         glext::eglDestroyImageKHR;
}

std::shared_ptr<GLHardwareTexture> GLHardwareTexture::MakeFrom(
    Context* context, android::GraphicBuffer* graphicBuffer) {
  std::shared_ptr<GLHardwareTexture> glTexture = nullptr;
  BytesKey recycleKey = {};
  GLHardwareTexture::ComputeRecycleKey(&recycleKey, graphicBuffer);
  glTexture = std::static_pointer_cast<GLHardwareTexture>(context->getRecycledResource(recycleKey));
  if (glTexture) {
    return glTexture;
  }
  auto buffer =
      ((android::android_native_buffer_t*)NativeGraphicBufferInterface::Get()->getNativeBuffer(
          graphicBuffer));
  if (!buffer) {
    return nullptr;
  }
  return GLHardwareTexture::MakeFrom(context, nullptr, graphicBuffer, buffer, buffer->width,
                                     buffer->height);
}

std::shared_ptr<GLHardwareTexture> GLHardwareTexture::MakeFrom(Context* context,
                                                               AHardwareBuffer* hardwareBuffer) {
  if (!hardwareBuffer) {
    return nullptr;
  }
  static bool init = initGlextProc() && glext::eglGetNativeClientBufferANDROID;
  if (!init) {
    return nullptr;
  }
  std::shared_ptr<GLHardwareTexture> glTexture = nullptr;
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, hardwareBuffer);
  glTexture = std::static_pointer_cast<GLHardwareTexture>(context->getRecycledResource(recycleKey));
  if (glTexture) {
    return glTexture;
  }
  // Convert the AHardwareBuffer to EGLImage.
  EGLClientBuffer clientBuffer = glext::eglGetNativeClientBufferANDROID(hardwareBuffer);
  if (!clientBuffer) {
    return nullptr;
  }
  AHardwareBuffer_Desc desc;
  NativeHardwareBufferInterface::Get()->AHardwareBuffer_describe(hardwareBuffer, &desc);
  return MakeFrom(context, hardwareBuffer, nullptr, clientBuffer, desc.width, desc.height);
}

std::shared_ptr<GLHardwareTexture> GLHardwareTexture::MakeFrom(
    Context* context, AHardwareBuffer* hardwareBuffer, android::GraphicBuffer* graphicBuffer,
    EGLClientBuffer clientBuffer, int width, int height) {
  static bool init = initGlextProc();
  if (!init || !clientBuffer) {
    return nullptr;
  }
  EGLDisplay disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  EGLImageKHR eglImage = glext::eglCreateImageKHR(disp, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                                  clientBuffer, eglImageAttributes);
  if (eglImage == EGL_NO_IMAGE_KHR) return nullptr;
  GLTextureInfo glInfo = {};
  glGenTextures(1, &glInfo.id);
  glBindTexture(GL_TEXTURE_2D, glInfo.id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glext::glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)eglImage);
  auto glTexture = Resource::Wrap(
      context, new GLHardwareTexture(hardwareBuffer, graphicBuffer, eglImage, width, height));
  glTexture->sampler.glInfo = glInfo;
  glTexture->sampler.config = PixelConfig::RGBA_8888;
  return glTexture;
}

GLHardwareTexture::GLHardwareTexture(AHardwareBuffer* hardwareBuffer,
                                     android::GraphicBuffer* graphicBuffer, EGLImageKHR eglImage,
                                     int width, int height)
    : GLTexture(width, height, ImageOrigin::TopLeft),
      _eglImage(eglImage),
      hardwareBuffer(hardwareBuffer),
      graphicBuffer(graphicBuffer) {
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Acquire(graphicBuffer);
  }
  if (hardwareBuffer) {
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_acquire(hardwareBuffer);
  }
}

void GLHardwareTexture::ComputeRecycleKey(BytesKey* recycleKey, void* hardware_buffer) {
  static const uint32_t BGRAType = UniqueID::Next();
  recycleKey->write(BGRAType);
  recycleKey->write(hardware_buffer);
}

void GLHardwareTexture::onRelease(Context* context) {
  if (sampler.glInfo.id > 0) {
    auto gl = GLContext::Unwrap(context);
    gl->deleteTextures(1, &sampler.glInfo.id);
  }
  EGLDisplay disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (_eglImage != EGL_NO_IMAGE_KHR) {
    glext::eglDestroyImageKHR(disp, _eglImage);
    _eglImage = EGL_NO_IMAGE_KHR;
  }
  if (hardwareBuffer) {
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_release(hardwareBuffer);
  }
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Release(graphicBuffer);
  }
}
}  // namespace pag
