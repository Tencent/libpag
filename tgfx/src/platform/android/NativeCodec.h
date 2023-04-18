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

#include "JNIUtil.h"
#include "tgfx/core/ImageCodec.h"

namespace tgfx {
class NativeCodec : public ImageCodec {
 public:
  static void JNIInit(JNIEnv* env);

  bool readPixels(const ImageInfo& dstInfo, void* dstPixels) const override;

 protected:
  std::shared_ptr<ImageBuffer> onMakeBuffer(bool tryHardware) const override;

 private:
  std::string imagePath;
  std::shared_ptr<Data> imageBytes;
  Global<jobject> nativeImage;

  static std::shared_ptr<NativeCodec> Make(JNIEnv* env, jobject sizeObject, int origin);

  NativeCodec(int width, int height, EncodedOrigin origin) : ImageCodec(width, height, origin) {
  }

  jobject decodeBitmap(JNIEnv* env, ColorType colorType, AlphaType alphaType,
                       bool tryHardware) const;

  friend class ImageCodec;
};
}  // namespace tgfx
