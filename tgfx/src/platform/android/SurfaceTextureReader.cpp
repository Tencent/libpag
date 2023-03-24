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

#include "tgfx/platform/android/SurfaceTextureReader.h"
#include <chrono>
#include "platform/android/SurfaceTexture.h"

namespace tgfx {
std::shared_ptr<SurfaceTextureReader> SurfaceTextureReader::Make(int width, int height,
                                                                 jobject listener) {
  if (listener == nullptr || width < 1 || height < 1) {
    return nullptr;
  }
  auto imageStream = SurfaceTexture::Make(width, height, listener);
  if (imageStream == nullptr) {
    return nullptr;
  }
  auto imageReader =
      std::shared_ptr<SurfaceTextureReader>(new SurfaceTextureReader(std::move(imageStream)));
  imageReader->weakThis = imageReader;
  return imageReader;
}

SurfaceTextureReader::SurfaceTextureReader(std::shared_ptr<ImageStream> stream)
    : ImageReader(std::move(stream)) {
}

jobject SurfaceTextureReader::getInputSurface() const {
  return std::static_pointer_cast<SurfaceTexture>(stream)->getInputSurface();
}

void SurfaceTextureReader::notifyFrameAvailable() {
  std::static_pointer_cast<SurfaceTexture>(stream)->notifyFrameAvailable();
}

std::shared_ptr<ImageBuffer> SurfaceTextureReader::acquireNextBuffer() {
  std::static_pointer_cast<SurfaceTexture>(stream)->notifyFrameChanged();
  return ImageReader::acquireNextBuffer();
}
}  // namespace tgfx
