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

#if defined(__ANDROID__) || defined(ANDROID)

#include "EGLHardwareTexture.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/hardware_buffer.h>
#include "core/utils/UniqueID.h"
#include "platform/android/HardwareBuffer.h"
#include "platform/android/HardwareBufferInterface.h"
#include "tgfx/gpu/opengl/egl/EGLDevice.h"

namespace tgfx {
namespace eglext {
static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
}  // namespace eglext

static bool InitEGLEXTProc() {
  eglext::eglGetNativeClientBufferANDROID =
      (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)eglGetProcAddress("eglGetNativeClientBufferANDROID");
  eglext::glEGLImageTargetTexture2DOES =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  eglext::eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  eglext::eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  return eglext::eglGetNativeClientBufferANDROID && eglext::glEGLImageTargetTexture2DOES &&
         eglext::eglCreateImageKHR && eglext::eglDestroyImageKHR;
}

std::shared_ptr<EGLHardwareTexture> EGLHardwareTexture::MakeFrom(Context* context,
                                                                 AHardwareBuffer* hardwareBuffer) {
  static const bool initialized = InitEGLEXTProc();
  if (!hardwareBuffer || !initialized || !HardwareBufferInterface::Available()) {
    return nullptr;
  }
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, hardwareBuffer);
  auto glTexture = std::static_pointer_cast<EGLHardwareTexture>(
      context->resourceCache()->getRecycled(recycleKey));
  if (glTexture != nullptr) {
    return glTexture;
  }
  EGLClientBuffer clientBuffer = eglext::eglGetNativeClientBufferANDROID(hardwareBuffer);
  if (!clientBuffer) {
    return nullptr;
  }
  auto display = static_cast<EGLDevice*>(context->device())->getDisplay();
  EGLint attributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  EGLImageKHR eglImage = eglext::eglCreateImageKHR(
      display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attributes);
  if (eglImage == EGL_NO_IMAGE_KHR) {
    return nullptr;
  }
  GLSampler sampler = {};
  sampler.target = GL_TEXTURE_2D;
  sampler.format = PixelFormat::RGBA_8888;
  glGenTextures(1, &sampler.id);
  if (sampler.id == 0) {
    eglext::eglDestroyImageKHR(display, eglImage);
    return nullptr;
  }
  glBindTexture(sampler.target, sampler.id);
  glTexParameteri(sampler.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(sampler.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(sampler.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(sampler.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  eglext::glEGLImageTargetTexture2DOES(sampler.target, (GLeglImageOES)eglImage);
  AHardwareBuffer_Desc desc;
  HardwareBufferInterface::Describe(hardwareBuffer, &desc);
  glTexture = Resource::Wrap(
      context, new EGLHardwareTexture(hardwareBuffer, eglImage, desc.width, desc.height));
  glTexture->sampler = sampler;
  return glTexture;
}

EGLHardwareTexture::EGLHardwareTexture(AHardwareBuffer* hardwareBuffer, EGLImageKHR eglImage,
                                       int width, int height)
    : GLTexture(width, height, ImageOrigin::TopLeft),
      hardwareBuffer(hardwareBuffer),
      eglImage(eglImage) {
  HardwareBufferInterface::Acquire(hardwareBuffer);
}

EGLHardwareTexture::~EGLHardwareTexture() {
  HardwareBufferInterface::Release(hardwareBuffer);
}

void EGLHardwareTexture::ComputeRecycleKey(BytesKey* recycleKey, void* hardwareBuffer) {
  static const uint32_t BGRAType = UniqueID::Next();
  recycleKey->write(BGRAType);
  recycleKey->write(hardwareBuffer);
}

void EGLHardwareTexture::onReleaseGPU() {
  glDeleteTextures(1, &sampler.id);
  auto display = static_cast<EGLDevice*>(context->device())->getDisplay();
  eglext::eglDestroyImageKHR(display, eglImage);
}
}  // namespace tgfx

#endif