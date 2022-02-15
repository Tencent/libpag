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

#include "VideoSurface.h"
#include "android/native_window_jni.h"
#include "gpu/opengl/GLTexture.h"

namespace pag {
static Global<jclass> VideoSurfaceClass;
static jmethodID VideoSurface_Make;
static jmethodID VideoSurface_getOutputSurface;
static jmethodID VideoSurface_updateTexImage;
static jmethodID VideoSurface_attachToGLContext;
static jmethodID VideoSurface_videoWidth;
static jmethodID VideoSurface_videoHeight;
static jmethodID VideoSurface_onRelease;

void VideoSurface::InitJNI(JNIEnv* env, const std::string& className) {
  VideoSurfaceClass.reset(env, env->FindClass(className.c_str()));
  std::string createSig = std::string("(II)L") + className + ";";
  VideoSurface_Make = env->GetStaticMethodID(VideoSurfaceClass.get(), "Make", createSig.c_str());
  VideoSurface_getOutputSurface =
      env->GetMethodID(VideoSurfaceClass.get(), "getOutputSurface", "()Landroid/view/Surface;");
  VideoSurface_updateTexImage = env->GetMethodID(VideoSurfaceClass.get(), "updateTexImage", "()Z");
  VideoSurface_attachToGLContext =
      env->GetMethodID(VideoSurfaceClass.get(), "attachToGLContext", "(I)Z");
  VideoSurface_videoWidth = env->GetMethodID(VideoSurfaceClass.get(), "videoWidth", "()I");
  VideoSurface_videoHeight = env->GetMethodID(VideoSurfaceClass.get(), "videoHeight", "()I");
  VideoSurface_onRelease = env->GetMethodID(VideoSurfaceClass.get(), "onRelease", "()V");
}

OESTexture::OESTexture(GLTextureInfo info, int width, int height, bool hasAlpha)
    : GLTexture(width, height, ImageOrigin::TopLeft), hasAlpha(hasAlpha) {
  sampler.glInfo = info;
  sampler.config = PixelConfig::RGBA_8888;
}

void OESTexture::setTextureSize(int width, int height) {
  textureWidth = width;
  textureHeight = height;
  computeTransform();
}

void OESTexture::computeTransform() {
  if (textureWidth == 0 || textureHeight == 0 || hasAlpha) {
    return;
  }
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/nativedisplay/surfacetexture/SurfaceTexture.cpp;l=275;drc=master;bpv=0;bpt=1
  // https://stackoverflow.com/questions/6023400/opengl-es-texture-coordinates-slightly-off

  // In order to prevent bilinear sampling beyond the edge of the
  // crop rectangle we may need to shrink it by 2 texels in each
  // dimension.  Normally this would just need to take 1/2 a texel
  // off each end, but because the chroma channels of YUV420 images
  // are subsampled we may need to shrink the crop region by a whole
  // texel on each side.
  auto shrinkAmount = 1.0f;
  if (width() < textureWidth) {
    tx = shrinkAmount / static_cast<float>(textureWidth);
    sx = (static_cast<float>(width()) - (2.0f * shrinkAmount)) / static_cast<float>(textureWidth);
  }
  if (height() < textureHeight) {
    ty = shrinkAmount / static_cast<float>(textureHeight);
    sy = (static_cast<float>(height()) - (2.0f * shrinkAmount)) / static_cast<float>(textureHeight);
  }
}

Point OESTexture::getTextureCoord(float x, float y) const {
  if (hasAlpha) {
    // 如果有 alpha 通道，不需要缩小纹素
    return {x / static_cast<float>(textureWidth), y / static_cast<float>(textureHeight)};
  }
  return {x / static_cast<float>(width()) * sx + tx, y / static_cast<float>(height()) * sy + ty};
}

void OESTexture::onRelease(pag::Context* context) {
  if (sampler.glInfo.id > 0) {
    auto gl = GLContext::Unwrap(context);
    gl->deleteTextures(1, &sampler.glInfo.id);
  }
}

std::shared_ptr<VideoSurface> VideoSurface::Make(int width, int height, bool hasAlpha) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  Local<jobject> surface = {
      env, env->CallStaticObjectMethod(VideoSurfaceClass.get(), VideoSurface_Make, width, height)};
  if (surface.empty()) {
    return nullptr;
  }
  return std::shared_ptr<VideoSurface>(
      new VideoSurface(env, surface.get(), width, height, hasAlpha));
}

VideoSurface::VideoSurface(JNIEnv* env, jobject surface, int width, int height, bool hasAlpha)
    : width(width), height(height), hasAlpha(hasAlpha) {
  videoSurface.reset(env, surface);
}

VideoSurface::~VideoSurface() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(videoSurface.get(), VideoSurface_onRelease);
}

jobject VideoSurface::getOutputSurface(JNIEnv* env) const {
  return env->CallObjectMethod(videoSurface.get(), VideoSurface_getOutputSurface);
}

bool VideoSurface::attachToContext(Context* context) {
  if (oesTexture) {
    if (deviceID != context->device()->uniqueID()) {
      LOGE("VideoSurface::attachToGLContext(): VideoSurface has already attached to a Context!");
      return false;
    }
    return true;
  }
  auto gl = GLContext::Unwrap(context);
  GLTextureInfo glInfo = {};
  glInfo.target = GL::TEXTURE_EXTERNAL_OES;
  glInfo.format = GL::RGBA8;
  gl->genTextures(1, &glInfo.id);
  oesTexture = Resource::Wrap(context, new OESTexture(glInfo, width, height, hasAlpha));
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return false;
  }
  auto result = env->CallBooleanMethod(videoSurface.get(), VideoSurface_attachToGLContext,
                                       oesTexture->getGLInfo().id);
  deviceID = context->device()->uniqueID();
  if (!result) {
    LOGE("VideoSurface::attachToGLContext(): failed to attached to a Surface!");
    oesTexture = nullptr;
    deviceID = 0;
    return false;
  }
  oesTexture->attachedSurface.reset(env, videoSurface.get());
  return true;
}

void VideoSurface::markHasNewTextureImage() {
  hasPendingTextureImage = true;
}

bool VideoSurface::updateTexImage() {
  if (!hasPendingTextureImage) {
    return false;
  }
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return false;
  }
  bool status = env->CallBooleanMethod(videoSurface.get(), VideoSurface_updateTexImage);
  hasPendingTextureImage = false;
  return status;
}

std::shared_ptr<OESTexture> VideoSurface::getTexture() {
  if (oesTexture == nullptr) {
    return nullptr;
  }
  if (oesTexture->textureWidth > 0) {
    return oesTexture;
  }
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  auto textureWidth = env->CallIntMethod(videoSurface.get(), VideoSurface_videoWidth);
  auto textureHeight = env->CallIntMethod(videoSurface.get(), VideoSurface_videoHeight);
  oesTexture->setTextureSize(textureWidth, textureHeight);
  return oesTexture;
}
}  // namespace pag
