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

#pragma once

#include "gpu/opengl/GLTexture.h"
#include "platform/android/Global.h"

namespace pag {
class OESTexture : public GLTexture {
 public:
  OESTexture(GLTextureInfo info, int width, int height, bool hasAlpha);

  Point getTextureCoord(float x, float y) const override;

  size_t memoryUsage() const override {
    return 0;
  }

 protected:
  void onRelease(Context* context) override;

 private:
  void setTextureSize(int width, int height);

  void computeTransform();

  int textureWidth = 0;
  int textureHeight = 0;
  bool hasAlpha = false;
  // 持有 Java 的 Surface，确保即使 GPUDecoder 提前释放也能正常被使用。
  Global<jobject> attachedSurface;
  float sx = 1.0f;
  float sy = 1.0f;
  float tx = 0.0f;
  float ty = 0.0f;

  friend class VideoSurface;
};

class VideoSurface {
 public:
  static void InitJNI(JNIEnv* env, const std::string& className);

  static std::shared_ptr<VideoSurface> Make(int width, int height, bool hasAlpha = false);

  ~VideoSurface();

  jobject getOutputSurface(JNIEnv* env) const;

  bool attachToContext(Context* context);

  bool updateTexImage();

  std::shared_ptr<OESTexture> getTexture();

  void markHasNewTextureImage();

 private:
  Global<jobject> videoSurface;
  int width = 0;
  int height = 0;
  bool hasAlpha = false;
  ID deviceID = 0;
  std::shared_ptr<OESTexture> oesTexture = nullptr;
  mutable std::atomic_bool hasPendingTextureImage = {false};

  VideoSurface(JNIEnv* env, jobject surface, int width, int height, bool hasAlpha);
};
}  // namespace pag
