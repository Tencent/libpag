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

#ifdef FILE_MOVIE

#pragma once

#include "pag/pag.h"
#include "rendering/editing/FileMovie.h"
#include "video/FFmpegDemuxer.h"
#include "video/I420Buffer.h"
#include "video/VideoReader.h"
#include "video/VideoDecodingTask.h"

namespace pag {
class RenderCache;

class MovieReader {
 public:
  MovieReader(MovieInfo* movieInfo, DecodingPolicy policy);

  void prepareAsync(int64_t targetTime);

  std::shared_ptr<tgfx::Texture> readTexture(Frame targetFrame, RenderCache* cache);

 private:
  int64_t lastSampleTime = -1;
  int64_t pendingTime = -1;
  std::shared_ptr<VideoReader> reader = nullptr;
  std::shared_ptr<tgfx::Texture> lastTexture = nullptr;
  std::shared_ptr<Task> lastTask = nullptr;
};
}  // namespace pag

#endif
