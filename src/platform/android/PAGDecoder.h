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
#include "JNIHelper.h"
#include "android/hardware_buffer.h"
#include "pag/pag.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLSampler.h"

namespace pag {
class PAGDecoder {
 public:
  static void initJNI(JNIEnv* env);
  static std::shared_ptr<PAGDecoder> Make(std::shared_ptr<PAGComposition> pagComposition, int width,
                                          int height);
  jobject decode(double value);
  ~PAGDecoder();

 private:
  void createSurface();
  void checkBitmap();
  std::shared_ptr<PAGSurface> pagSurface;
  std::shared_ptr<PAGPlayer> pagPlayer;
  std::shared_ptr<PAGComposition> pagComposition;
  Global<jobject> bitmap;
  tgfx::GLSampler textureInfo;
  int width = 0;
  int height = 0;
  std::shared_ptr<tgfx::GLDevice> device;
};
}  // namespace pag
