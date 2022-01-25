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

#include "VideoDecodingTask.h"

namespace pag {
std::shared_ptr<Task> VideoDecodingTask::MakeAndRun(VideoReader* reader, int64_t targetTime) {
  if (reader == nullptr) {
    return nullptr;
  }
  auto task =
      Task::Make(std::unique_ptr<VideoDecodingTask>(new VideoDecodingTask(reader, targetTime)));
  task->run();
  return task;
}

VideoDecodingTask::VideoDecodingTask(VideoReader* reader, int64_t targetTime)
    : reader(reader), targetTime(targetTime) {
}

void VideoDecodingTask::execute() {
  reader->readSample(targetTime);
}

}  // namespace pag