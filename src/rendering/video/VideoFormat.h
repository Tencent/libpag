/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "pag/types.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/YUVColorSpace.h"

namespace pag {

class VideoDemuxer;

struct VideoFormat {
  std::string mimeType = "video/avc";
  std::vector<std::shared_ptr<tgfx::Data>> headers = {};
  tgfx::YUVColorSpace colorSpace = tgfx::YUVColorSpace::BT601_LIMITED;
  int width = 0;
  int height = 0;
  int64_t duration = 0;
  float frameRate = 0.0;
  int maxReorderSize = 4;
  // used by the video decoder on the web platform.
  VideoDemuxer* demuxer = nullptr;
};
}  // namespace pag
