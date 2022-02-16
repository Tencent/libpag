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

#include "VideoDecodingTask.h"
#include "base/utils/TimeUtil.h"
#include "rendering/readers/SequenceReader.h"
#include "video/VideoReader.h"

namespace pag {
class VideoSequenceReader : public SequenceReader {
 public:
  VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence, DecodingPolicy policy);

  void prepareAsync(Frame targetFrame) override;

  std::shared_ptr<tgfx::Texture> readTexture(Frame targetFrame, RenderCache* cache) override;

 private:
  Frame lastFrame = -1;
  int64_t pendingTime = -1;
  std::shared_ptr<VideoReader> reader = nullptr;
  std::shared_ptr<tgfx::Texture> lastTexture = nullptr;
  std::shared_ptr<Task> lastTask = nullptr;
};
}  // namespace pag
