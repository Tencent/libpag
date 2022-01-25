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

namespace pag {
class I420Buffer : public VideoBuffer {
public:
    size_t planeCount() const override;

    std::shared_ptr<Texture> makeTexture(Context* context) const override;

protected:
    I420Buffer(int width, int height, uint8_t* data[3], const int lineSize[3],
               YUVColorSpace colorSpace, YUVColorRange colorRange);

private:
    YUVColorSpace colorSpace = YUVColorSpace::Rec601;
    YUVColorRange colorRange = YUVColorRange::MPEG;
    uint8_t* pixelsPlane[3] = {};
    int rowBytesPlane[3] = {};
};
}  // namespace pag