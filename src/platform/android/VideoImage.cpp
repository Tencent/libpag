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

#include "VideoImage.h"

namespace pag {
std::shared_ptr<VideoImage> VideoImage::MakeFrom(std::shared_ptr<VideoSurface> videoSurface,
                                                 int width, int height) {
  if (videoSurface == nullptr) {
    return nullptr;
  }
  auto videoImage =
      std::shared_ptr<VideoImage>(new VideoImage(std::move(videoSurface), width, height));
  return videoImage;
}

VideoImage::VideoImage(std::shared_ptr<VideoSurface> videoSurface, int width, int height)
    : VideoBuffer(width, height), videoSurface(std::move(videoSurface)) {
  this->videoSurface->markHasNewTextureImage();
}

std::shared_ptr<Texture> VideoImage::makeTexture(Context* context) const {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!videoSurface->attachToContext(context)) {
    return nullptr;
  }
  if (!videoSurface->updateTexImage()) {
    return nullptr;
  }
  return videoSurface->getTexture();
}
}  // namespace pag
