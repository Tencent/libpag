/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "SequenceReaderFactory.h"
#include "base/utils/USE.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/core/Image.h"

#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/VideoSequenceReader.h"
#endif

namespace pag {

static tgfx::ISize GetBufferSize(Sequence* sequence) {
  if (sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    return tgfx::ISize::Make(videoSequence->getVideoWidth(), videoSequence->getVideoHeight());
  }
  return tgfx::ISize::Make(sequence->width, sequence->height);
}

std::shared_ptr<SequenceReaderFactory> SequenceReaderFactory::Make(Sequence* sequence) {
  if (sequence == nullptr) {
    return nullptr;
  }
  auto factory = std::shared_ptr<SequenceReaderFactory>(new SequenceReaderFactory(sequence));
  factory->weakThis = factory;
  return factory;
}

SequenceReaderFactory::SequenceReaderFactory(Sequence* sequence) : sequence(sequence) {
}

std::shared_ptr<SequenceReader> SequenceReaderFactory::makeReader(std::shared_ptr<File> file,
                                                                  PAGFile* pagFile) {
  if (sequence == nullptr || file == nullptr) {
    return nullptr;
  }
  std::shared_ptr<SequenceReader> reader = nullptr;
  auto composition = sequence->composition;
  if (composition->type() == CompositionType::Bitmap) {
    reader = std::make_shared<BitmapSequenceReader>(std::move(file),
                                                    static_cast<BitmapSequence*>(sequence));
  } else {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    auto demuxer = std::make_unique<VideoSequenceDemuxer>(std::move(file), videoSequence, pagFile);
    reader = std::make_shared<VideoReader>(std::move(demuxer));
  }
  return reader;
}

std::shared_ptr<tgfx::ImageGenerator> SequenceReaderFactory::makeGenerator(
    std::shared_ptr<File> file) {
  if (sequence == nullptr || file == nullptr || !staticContent()) {
    return nullptr;
  }
  return std::make_shared<StaticSequenceGenerator>(std::move(file), weakThis.lock(),
                                                   GetBufferSize(sequence));
}

std::shared_ptr<tgfx::ImageGenerator> SequenceReaderFactory::makeGenerator(
    std::shared_ptr<SequenceReader> reader, Frame targetFrame) {
  if (reader == nullptr || staticContent()) {
    return nullptr;
  }
  return std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame);
}

bool SequenceReaderFactory::staticContent() const {
  return sequence->composition->staticContent();
}

ID SequenceReaderFactory::uniqueID() const {
  return sequence->composition->uniqueID;
}

int SequenceReaderFactory::width() const {
  return sequence->width;
}

int SequenceReaderFactory::height() const {
  return sequence->height;
}

Frame SequenceReaderFactory::duration() const {
  return sequence->duration();
}

bool SequenceReaderFactory::isVideo() const {
  return sequence->composition->type() == CompositionType::Video;
}

RGBAAALayout SequenceReaderFactory::layout() const {
  if (sequence != nullptr && sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    return {sequence->width, sequence->height, videoSequence->alphaStartX,
            videoSequence->alphaStartY};
  }
  return {sequence->width, sequence->height, -1, -1};
}

Frame SequenceReaderFactory::firstVisibleFrame(const Layer* layer) const {
  if (sequence == nullptr || layer->type() != LayerType::PreCompose) {
    return 0;
  }
  auto preComposeLayer = static_cast<const PreComposeLayer*>(layer);
  auto composition = preComposeLayer->composition;
  if (composition->type() != CompositionType::Video &&
      composition->type() != CompositionType::Bitmap) {
    return 0;
  }
  auto timeScale = layer->containingComposition
                       ? (composition->frameRate / layer->containingComposition->frameRate)
                       : 1.0f;
  auto compositionFrame = static_cast<Frame>(roundf(
      static_cast<float>(layer->startTime - preComposeLayer->compositionStartTime) * timeScale));
  if (compositionFrame < 0) {
    compositionFrame = 0;
  }
  return sequence->toSequenceFrame(compositionFrame);
}

tgfx::Orientation SequenceReaderFactory::orientation() const {
  return tgfx::Orientation::TopLeft;
}

}  // namespace pag