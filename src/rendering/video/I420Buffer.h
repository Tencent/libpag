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

#include "VideoBuffer.h"
#include "tgfx/core/YUVData.h"

namespace pag {
class I420Buffer : public VideoBuffer {
 public:
  I420Buffer(std::shared_ptr<tgfx::YUVData> yuvData, tgfx::YUVColorSpace colorSpace,
             tgfx::YUVColorRange colorRange);

  size_t planeCount() const override;

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context,
                                               bool mipMapped) const override;

 protected:
 private:
  std::shared_ptr<tgfx::YUVData> yuvData = nullptr;
  tgfx::YUVColorSpace colorSpace = tgfx::YUVColorSpace::Rec601;
  tgfx::YUVColorRange colorRange = tgfx::YUVColorRange::MPEG;
};
}  // namespace pag