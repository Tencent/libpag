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
#ifdef FFMPEG
#pragma once

#include "FFmpegDecoder.h"
#include "VideoBuffer.h"

namespace pag {
class NV12Buffer : public VideoBuffer {
 public:
  static std::shared_ptr<NV12Buffer> Make(int width, int height,
                                          tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange,
                                          std::shared_ptr<FFmpegFrame> frame) {
    auto buffer =
        new NV12Buffer(width, height, frame->frame()->data, frame->frame()->linesize,
                       colorSpace, colorRange);
    return std::shared_ptr<NV12Buffer>(buffer);
  }

  size_t planeCount() const override;

  std::shared_ptr<tgfx::Texture> makeTexture(tgfx::Context* context) const override;

 private:
  std::shared_ptr<FFmpegFrame> frame = nullptr;
  tgfx::YUVColorSpace colorSpace = tgfx::YUVColorSpace::Rec601;
  tgfx::YUVColorRange colorRange = tgfx::YUVColorRange::MPEG;
  uint8_t* pixelsPlane[2] = {};
  int rowBytesPlane[2] = {};

  NV12Buffer(int width, int height, uint8_t* data[3], const int lineSize[3],
             tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange);
};
}  // namespace pag
#endif