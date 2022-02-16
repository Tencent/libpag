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

#include "video/VideoBuffer.h"

namespace pag {
class VideoImage : public VideoBuffer {
 public:
  static std::shared_ptr<VideoImage> MakeFrom(CVPixelBufferRef pixelBuffer,
                                              tgfx::YUVColorSpace colorSpace,
                                              tgfx::YUVColorRange colorRange);

  ~VideoImage() override;

  size_t planeCount() const override;

 protected:
  std::shared_ptr<tgfx::Texture> makeTexture(tgfx::Context* context) const override {
    return tgfx::YUVTexture::MakeFrom(context, _colorSpace, _colorRange, pixelBuffer);
  }

 private:
  CVPixelBufferRef pixelBuffer = nullptr;
  tgfx::YUVColorSpace _colorSpace = tgfx::YUVColorSpace::Rec601;
  tgfx::YUVColorRange _colorRange = tgfx::YUVColorRange::MPEG;

  VideoImage(CVPixelBufferRef pixelBuffer, tgfx::YUVColorSpace colorSpace,
             tgfx::YUVColorRange colorRange);
};
}  // namespace pag
