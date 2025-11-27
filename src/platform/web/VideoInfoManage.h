/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#ifndef VIDEO_INFO_MANAGE_H
#define VIDEO_INFO_MANAGE_H
#include <emscripten.h>
#include <emscripten/val.h>
#include "codec/tags/VideoSequence.h"
#include "pag/pag.h"

namespace pag {

struct VideoInfo {
  std::shared_ptr<PAGFile> pagFile = nullptr;
  VideoComposition* videoComposition = nullptr;
  int32_t width = 0;
  int32_t height = 0;
  float frameRate = 30.0f;
  Frame targetFrame = -1;
  VideoSequence* videoSequence = nullptr;
  PreComposeLayer* preLayer = nullptr;
  std::shared_ptr<PAGComposition> pagComposition = nullptr;
  std::unique_ptr<ByteData> mp4Data = nullptr;
};

class VideoInfoManage {
 public:
  explicit VideoInfoManage(std::shared_ptr<PAGComposition> pagComposition);

  emscripten::val getMp4DataById(ID id);

  int32_t getWidthById(ID id);

  int32_t getHeightById(ID id);

  float getFrameRateById(ID id);

  emscripten::val getStaticTimeRangesById(ID id);

  int getTargetFrameById(ID id);

  float getPlaybackRateById(ID id);

  emscripten::val getVideoIds();

 private:
  std::shared_ptr<PAGComposition> pagComposition = nullptr;
  std::vector<std::shared_ptr<PAGFile>> pagFiles = {};
  std::vector<ID> videoIds = {};
  std::unordered_map<ID, VideoInfo> videoInfoMap = {};
};

}  // namespace pag
#endif  //VIDEO_INFO_MANAGE_H
