/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "SurfaceBufferQueue.h"
#include "core/utils/Log.h"

namespace tgfx {
static jmethodID SurfaceTexture_updateTexImage;
static jmethodID SurfaceTexture_attachToGLContext;
static jmethodID SurfaceTexture_getTransformMatrix;

void SurfaceBufferQueue::JNIInit(JNIEnv* env) {
  Local<jclass> SurfaceTextureClass = {env, env->FindClass("android/graphics/SurfaceTexture")};
  SurfaceTexture_updateTexImage =
      env->GetMethodID(SurfaceTextureClass.get(), "updateTexImage", "()V");
  SurfaceTexture_attachToGLContext =
      env->GetMethodID(SurfaceTextureClass.get(), "attachToGLContext", "(I)V");
  SurfaceTexture_getTransformMatrix =
      env->GetMethodID(SurfaceTextureClass.get(), "getTransformMatrix", "([F)V");
}

std::shared_ptr<ImageBufferQueue> ImageBufferQueue::MakeFrom(jobject surfaceTexture, int width,
                                                             int height) {
  if (surfaceTexture == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return nullptr;
  }
  auto bufferQueue = std::make_shared<SurfaceBufferQueue>(env, surfaceTexture, width, height);
  bufferQueue->weakThis = bufferQueue;
  return bufferQueue;
}

class SurfaceBuffer : public ImageBuffer {
 public:
  explicit SurfaceBuffer(std::shared_ptr<SurfaceBufferQueue> bufferQueue)
      : bufferQueue(std::move(bufferQueue)) {
  }

  int width() const override {
    return bufferQueue->width;
  }

  int height() const override {
    return bufferQueue->height;
  }

  bool isAlphaOnly() const override {
    return false;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override {
    return bufferQueue->makeTexture(context, mipMapped);
  }

 private:
  std::shared_ptr<SurfaceBufferQueue> bufferQueue = nullptr;
};

SurfaceBufferQueue::SurfaceBufferQueue(JNIEnv* env, jobject surface, int width, int height)
    : width(width), height(height) {
  surfaceTexture.reset(env, surface);
}

std::shared_ptr<ImageBuffer> SurfaceBufferQueue::acquireNextBuffer() {
  std::lock_guard<std::mutex> autoLock(locker);
  hasNewPendingFrame = true;
  return std::make_shared<SurfaceBuffer>(weakThis.lock());
}

void SurfaceBufferQueue::notifyFrameAvailable() {
  std::lock_guard<std::mutex> autoLock(locker);
  frameAvailable = true;
  condition.notify_all();
}

static ISize ComputeTextureSize(float matrix[16], int width, int height) {
  Size size = {static_cast<float>(width), static_cast<float>(height)};
  auto scaleX = fabsf(matrix[0]);
  if (scaleX > 0) {
    size.width = size.width / (scaleX + matrix[12] * 2);
  }
  float scaleY = fabsf(matrix[5]);
  if (scaleY > 0) {
    size.height = size.height / (scaleY + (matrix[13] - scaleY) * 2);
  }
  return size.toRound();
}

std::shared_ptr<Texture> SurfaceBufferQueue::makeTexture(Context* context, bool) {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!hasNewPendingFrame) {
    return texture;
  }
  if (!frameAvailable) {
    condition.wait(autoLock);
  }
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return nullptr;
  }
  if (!attachToContext(env, context)) {
    return nullptr;
  }
  hasNewPendingFrame = false;
  frameAvailable = false;
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_updateTexImage);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("SurfaceBufferQueue::makeTexture(): failed to updateTexImage!");
    return nullptr;
  }
  Local<jfloatArray> floatArray = {env, env->NewFloatArray(16)};
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_getTransformMatrix, floatArray.get());
  auto matrix = env->GetFloatArrayElements(floatArray.get(), nullptr);
  auto textureSize = ComputeTextureSize(matrix, width, height);
  env->ReleaseFloatArrayElements(floatArray.get(), matrix, 0);
  texture->updateTextureSize(textureSize.width, textureSize.height);
  return texture;
}

bool SurfaceBufferQueue::attachToContext(JNIEnv* env, Context* context) {
  if (texture != nullptr) {
    if (texture->getContext() != context) {
      LOGE(
          "SurfaceBufferQueue::attachToGLContext(): "
          "SurfaceBufferQueue has already attached to a Context!");
      return false;
    }
    return true;
  }
  texture = GLExternalTexture::Make(context, width, height);
  if (texture == nullptr) {
    return false;
  }
  auto sampler = static_cast<const GLSampler*>(texture->getSampler());
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_attachToGLContext, sampler->id);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    texture = nullptr;
    LOGE("SurfaceBufferQueue::attachToGLContext(): failed to attached to a SurfaceTexture!");
    return false;
  }
  return true;
}
}  // namespace tgfx
