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

#include <atomic>
#include "VideoSurface.h"
#include "video/VideoBuffer.h"

namespace pag {
class VideoImage : public VideoBuffer {
public:
    static std::shared_ptr<VideoImage> MakeFrom(std::shared_ptr<VideoSurface> videoSurface, int width,
            int height);

    size_t planeCount() const override {
        return 1;
    }

protected:
    std::shared_ptr<Texture> makeTexture(Context* context) const override;

private:
    mutable std::mutex locker = {};
    std::shared_ptr<VideoSurface> videoSurface = nullptr;

    VideoImage(std::shared_ptr<VideoSurface> videoSurface, int width, int height);
};
}  // namespace pag
