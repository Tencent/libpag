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

#include "SequenceReader.h"
#include "rendering/caches/RenderCache.h"

namespace pag {
class SequenceTask : public Executor {
 public:
  static std::shared_ptr<Task> MakeAndRun(SequenceReader* reader, int64_t targetTime) {
    auto task = Task::Make(std::unique_ptr<SequenceTask>(new SequenceTask(reader, targetTime)));
    task->run();
    return task;
  }

 private:
  SequenceReader* reader = nullptr;
  int64_t targetTime = 0;

  SequenceTask(SequenceReader* reader, int64_t targetTime)
      : reader(reader), targetTime(targetTime) {
  }

  void execute() override {
    reader->decodeFrame(targetTime);
  }
};

SequenceReader::~SequenceReader() {
  // The subclass must cancel the last task in their destructor, otherwise, the task may access wild
  // pointers.
  DEBUG_ASSERT(lastTask == nullptr || !lastTask->isRunning());
}

void SequenceReader::prepare(int64_t targetFrame) {
  if (lastTask == nullptr && targetFrame > 0) {
    lastTask = SequenceTask::MakeAndRun(this, targetFrame);
  }
}

std::shared_ptr<tgfx::Texture> SequenceReader::readTexture(int64_t targetFrame,
                                                           RenderCache* cache) {
  if (lastFrame == targetFrame) {
    return lastTexture;
  }
  tgfx::Clock clock = {};
  // Setting the lastTask to nullptr triggers cancel().
  lastTask = nullptr;
  auto success = decodeFrame(targetFrame);
  auto decodingTime = clock.measure();
  recordPerformance(cache, decodingTime);
  // Release the last texture for immediately reusing.
  lastTexture = nullptr;
  lastFrame = -1;
  if (success) {
    clock.reset();
    lastTexture = makeTexture(cache->getContext());
    lastFrame = targetFrame;
    cache->textureUploadingTime += clock.measure();
    if (!staticContent()) {
      auto nextFrame = getNextFrameAt(targetFrame);
      if (nextFrame == -1 && pendingFirstFrame >= 0) {
        // Add preparation for the first frame when reach to the end.
        nextFrame = pendingFirstFrame;
        pendingFirstFrame = -1;
      }
      prepare(nextFrame);
    }
  }
  return lastTexture;
}
}  // namespace pag
