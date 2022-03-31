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
  static std::shared_ptr<Task> MakeAndRun(SequenceReader* reader, Frame targetFrame) {
    auto task = Task::Make(std::unique_ptr<SequenceTask>(new SequenceTask(reader, targetFrame)));
    task->run();
    return task;
  }

 private:
  SequenceReader* reader = nullptr;
  Frame targetFrame = 0;

  SequenceTask(SequenceReader* reader, Frame targetFrame)
      : reader(reader), targetFrame(targetFrame) {
  }

  void execute() override {
    reader->decodeFrame(targetFrame);
  }
};

SequenceReader::~SequenceReader() {
  // The subclass must cancel the last task in their destructor, otherwise, the task may access wild
  // pointers.
  DEBUG_ASSERT(lastTask == nullptr || !lastTask->isRunning());
}

void SequenceReader::prepare(Frame targetFrame) {
  if (staticContent) {
    targetFrame = 0;
  }
  if (lastTask == nullptr && targetFrame >= 0 && targetFrame < totalFrames) {
    lastTask = SequenceTask::MakeAndRun(this, targetFrame);
  }
}

void SequenceReader::prepareNext(Frame targetFrame) {
  if (!staticContent) {
    auto nextFrame = targetFrame + 1;
    if (nextFrame >= totalFrames && pendingFirstFrame >= 0) {
      // Add preparation for the first frame when reach to the end.
      nextFrame = pendingFirstFrame;
      pendingFirstFrame = -1;
    }
    prepare(nextFrame);
  }
}

std::shared_ptr<tgfx::Texture> SequenceReader::readTexture(Frame targetFrame, RenderCache* cache) {
  if (staticContent) {
    targetFrame = 0;
  }
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
    prepareNext(targetFrame);
  }
  return lastTexture;
}
}  // namespace pag
