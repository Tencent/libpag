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
#include "base/utils/Log.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
static Global<jclass> VideoSurfaceClass;
static jmethodID VideoSurface_Make;
static jmethodID VideoSurface_updateTexImage;
static jmethodID VideoSurface_attachToGLContext;
static jmethodID VideoSurface_videoWidth;
static jmethodID VideoSurface_videoHeight;
static jmethodID VideoSurface_release;

void VideoSurface::InitJNI(JNIEnv* env, const std::string& className) {
  VideoSurfaceClass.reset(env, env->FindClass(className.c_str()));
  std::string createSig = std::string("(II)L") + className + ";";
  VideoSurface_Make = env->GetStaticMethodID(VideoSurfaceClass.get(), "Make", createSig.c_str());
  VideoSurface_updateTexImage = env->GetMethodID(VideoSurfaceClass.get(), "updateTexImage", "()Z");
  VideoSurface_attachToGLContext =
      env->GetMethodID(VideoSurfaceClass.get(), "attachToGLContext", "(I)Z");
  VideoSurface_videoWidth = env->GetMethodID(VideoSurfaceClass.get(), "videoWidth", "()I");
  VideoSurface_videoHeight = env->GetMethodID(VideoSurfaceClass.get(), "videoHeight", "()I");
  VideoSurface_release = env->GetMethodID(VideoSurfaceClass.get(), "release", "()V");
}

OESTexture::OESTexture(const tgfx::GLSampler& glSampler, int width, int height)
    : GLTexture(width, height, tgfx::ImageOrigin::TopLeft) {
  sampler = glSampler;
}

void OESTexture::setTextureSize(int width, int height) {
  textureWidth = width;
  textureHeight = height;
  computeTransform();
}

void OESTexture::computeTransform() {
  if (textureWidth == 0 || textureHeight == 0) {
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

tgfx::Point OESTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()) * sx + tx, y / static_cast<float>(height()) * sy + ty};
}

void OESTexture::onReleaseGPU() {
  if (sampler.id > 0) {
    auto gl = tgfx::GLFunctions::Get(context);
    gl->deleteTextures(1, &sampler.id);
  }
}

std::shared_ptr<VideoSurface> VideoSurface::Make(int width, int height) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  Local<jobject> surface = {
      env, env->CallStaticObjectMethod(VideoSurfaceClass.get(), VideoSurface_Make, width, height)};
  if (surface.empty()) {
    return nullptr;
  }
  return std::shared_ptr<VideoSurface>(new VideoSurface(env, surface.get(), width, height));
}

VideoSurface::VideoSurface(JNIEnv* env, jobject surface, int width, int height)
    : width(width), height(height) {
  videoSurface.reset(env, surface);
}

VideoSurface::~VideoSurface() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(videoSurface.get(), VideoSurface_release);
}

jobject VideoSurface::getVideoSurface() const {
  return videoSurface.get();
}

void VideoSurface::markPendingTexImage() {
  hasPendingTextureImage = true;
}

void VideoSurface::clearPendingTexImage() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  updateTexImage(env);
}

std::shared_ptr<tgfx::Texture> VideoSurface::makeTexture(tgfx::Context* context) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  if (!attachToContext(env, context)) {
    return nullptr;
  }
  if (!updateTexImage(env)) {
    return nullptr;
  }
  if (oesTexture == nullptr) {
    auto textureWidth = env->CallIntMethod(videoSurface.get(), VideoSurface_videoWidth);
    auto textureHeight = env->CallIntMethod(videoSurface.get(), VideoSurface_videoHeight);
    oesTexture = tgfx::Resource::Wrap(context, new OESTexture(glInfo, width, height));
    oesTexture->setTextureSize(textureWidth, textureHeight);
    oesTexture->attachedSurface.reset(env, videoSurface.get());
  }
  return oesTexture;
}

bool VideoSurface::attachToContext(JNIEnv* env, tgfx::Context* context) {
  if (glInfo.id > 0) {
    if (deviceID != context->device()->uniqueID()) {
      LOGE("VideoSurface::attachToGLContext(): VideoSurface has already attached to a Context!");
      return false;
    }
    return true;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  tgfx::GLSampler sampler = {};
  sampler.target = GL_TEXTURE_EXTERNAL_OES;
  sampler.format = tgfx::PixelFormat::RGBA_8888;
  gl->genTextures(1, &sampler.id);
  if (sampler.id == 0) {
    return false;
  }
  auto result =
      env->CallBooleanMethod(videoSurface.get(), VideoSurface_attachToGLContext, sampler.id);
  if (!result) {
    gl->deleteTextures(1, &sampler.id);
    LOGE("VideoSurface::attachToGLContext(): failed to attached to a Surface!");
    return false;
  }
  glInfo = sampler;
  deviceID = context->device()->uniqueID();
  return true;
}

bool VideoSurface::updateTexImage(JNIEnv* env) {
  if (!hasPendingTextureImage) {
    return false;
  }
  auto result = env->CallBooleanMethod(videoSurface.get(), VideoSurface_updateTexImage);
  hasPendingTextureImage = false;
  return result;
}

}  // namespace pag
