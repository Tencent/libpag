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
#include "gpu/Gpu.h"
#include "opengl/GLSampler.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/opengl/egl/EGLDevice.h"
#include "utils/PixelFormatUtil.h"
#include "utils/UniqueID.h"

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
  if (!initialized) {
    return nullptr;
  }
  auto info = HardwareBufferGetInfo(hardwareBuffer);
  if (info.isEmpty()) {
    return nullptr;
  }
  ScratchKey scratchKey = {};
  ComputeScratchKey(&scratchKey, hardwareBuffer);
  auto glTexture = std::static_pointer_cast<EGLHardwareTexture>(
      context->resourceCache()->findScratchResource(scratchKey));
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

  auto sampler = std::make_unique<GLSampler>();
  sampler->target = GL_TEXTURE_2D;
  sampler->format = ColorTypeToPixelFormat(info.colorType());
  glGenTextures(1, &sampler->id);
  if (sampler->id == 0) {
    eglext::eglDestroyImageKHR(display, eglImage);
    return nullptr;
  }
  glBindTexture(sampler->target, sampler->id);
  glTexParameteri(sampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(sampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(sampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(sampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  eglext::glEGLImageTargetTexture2DOES(sampler->target, (GLeglImageOES)eglImage);
  glTexture = Resource::Wrap(
      context, new EGLHardwareTexture(hardwareBuffer, eglImage, info.width(), info.height()));
  glTexture->sampler = std::move(sampler);
  return glTexture;
}

EGLHardwareTexture::EGLHardwareTexture(AHardwareBuffer* hardwareBuffer, EGLImageKHR eglImage,
                                       int width, int height)
    : Texture(width, height, ImageOrigin::TopLeft),
      hardwareBuffer(HardwareBufferRetain(hardwareBuffer)),
      eglImage(eglImage) {
}

EGLHardwareTexture::~EGLHardwareTexture() {
  HardwareBufferRelease(hardwareBuffer);
}

void EGLHardwareTexture::ComputeScratchKey(BytesKey* scratchKey, void* hardwareBuffer) {
  static const uint32_t BGRAType = UniqueID::Next();
  scratchKey->write(BGRAType);
  scratchKey->write(hardwareBuffer);
}

size_t EGLHardwareTexture::memoryUsage() const {
  auto info = HardwareBufferGetInfo(hardwareBuffer);
  return info.byteSize();
}

void EGLHardwareTexture::onReleaseGPU() {
  context->gpu()->deleteSampler(sampler.get());
  auto display = static_cast<EGLDevice*>(context->device())->getDisplay();
  eglext::eglDestroyImageKHR(display, eglImage);
}
}  // namespace tgfx

#endif