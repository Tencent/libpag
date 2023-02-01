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
#include "base/utils/USE.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "rendering/video/VideoDecoder.h"

#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/VideoSequenceReader.h"
#endif

namespace pag {
std::shared_ptr<SequenceReader> SequenceReader::Make(std::shared_ptr<File> file, Sequence* sequence,
                                                     PAGFile* pagFile) {
  std::shared_ptr<SequenceReader> reader = nullptr;
  auto composition = sequence->composition;
  if (composition->type() == CompositionType::Bitmap) {
    reader = std::make_shared<BitmapSequenceReader>(std::move(file),
                                                    static_cast<BitmapSequence*>(sequence));
  } else {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
#ifdef PAG_BUILD_FOR_WEB
    if (VideoDecoder::HasExternalSoftwareDecoder()) {
      auto demuxer = std::make_unique<VideoSequenceDemuxer>(std::move(file), videoSequence);
      reader = std::make_shared<VideoReader>(std::move(demuxer));
    } else {
      reader = std::make_shared<VideoSequenceReader>(std::move(file), videoSequence, pagFile);
    }
#else
    USE(pagFile);
    auto demuxer = std::make_unique<VideoSequenceDemuxer>(std::move(file), videoSequence);
    reader = std::make_shared<VideoReader>(std::move(demuxer));
#endif
  }
  return reader;
}

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
    preparedFrame = targetFrame;
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

std::shared_ptr<tgfx::ImageBuffer> SequenceReader::makeBuffer(Frame targetFrame) {
  auto success = decodeFrame(targetFrame);
  if (!success) {
    return nullptr;
  }
  return onMakeBuffer();
}

std::shared_ptr<tgfx::ImageBuffer> SequenceReader::onMakeBuffer() {
  return nullptr;
}

std::shared_ptr<tgfx::Texture> SequenceReader::makeTexture(tgfx::Context* context,
                                                           Frame targetFrame) {
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
  decodingTime += clock.measure();
  // Release the last texture for immediately reusing.
  lastTexture = nullptr;
  lastFrame = -1;
  if (success) {
    clock.reset();
    lastTexture = onMakeTexture(context);
    lastFrame = targetFrame;
    preparedFrame = targetFrame;
    textureUploadingTime += clock.measure();
    prepareNext(targetFrame);
  }
  return lastTexture;
}

void SequenceReader::recordPerformance(Performance* performance) {
  if (decodingTime > 0) {
    performance->imageDecodingTime += decodingTime;
    decodingTime = 0;
  }
  if (textureUploadingTime > 0) {
    performance->textureUploadingTime += textureUploadingTime;
    textureUploadingTime = 0;
  }
}
}  // namespace pag
