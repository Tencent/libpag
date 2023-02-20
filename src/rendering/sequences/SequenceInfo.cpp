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

#include "SequenceInfo.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/core/Image.h"

namespace pag {
static tgfx::ISize GetBufferSize(Sequence* sequence) {
  if (sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    return tgfx::ISize::Make(videoSequence->getVideoWidth(), videoSequence->getVideoHeight());
  }
  return tgfx::ISize::Make(sequence->width, sequence->height);
}

static std::shared_ptr<tgfx::Image> MakeSequenceImage(
    std::shared_ptr<tgfx::ImageGenerator> generator, Sequence* sequence) {
  std::shared_ptr<tgfx::Image> image = nullptr;
  if (sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    return tgfx::Image::MakeRGBAAA(std::move(generator), sequence->width, sequence->height,
                                   videoSequence->alphaStartX, videoSequence->alphaStartY);
  }
  return tgfx::Image::MakeFromGenerator(std::move(generator));
}

std::shared_ptr<SequenceInfo> SequenceInfo::Make(Sequence* sequence) {
  if (sequence == nullptr) {
    return nullptr;
  }
  auto factory = std::shared_ptr<SequenceInfo>(new SequenceInfo(sequence));
  factory->weakThis = factory;
  return factory;
}

SequenceInfo::SequenceInfo(Sequence* sequence) : sequence(sequence) {
}

std::shared_ptr<SequenceReader> SequenceInfo::makeReader(std::shared_ptr<File> file,
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

std::shared_ptr<tgfx::Image> SequenceInfo::makeStatic(std::shared_ptr<File> file) {
  if (sequence == nullptr || file == nullptr || !staticContent()) {
    return nullptr;
  }
  auto generator = std::make_shared<StaticSequenceGenerator>(std::move(file), weakThis.lock(),
                                                             GetBufferSize(sequence));
  return MakeSequenceImage(std::move(generator), sequence);
}

std::shared_ptr<tgfx::Image> SequenceInfo::makeImage(std::shared_ptr<SequenceReader> reader,
                                                     Frame targetFrame) {
  if (reader == nullptr || sequence == nullptr) {
    return nullptr;
  }
  auto generator = std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame);
  return MakeSequenceImage(std::move(generator), sequence);
}

bool SequenceInfo::staticContent() const {
  return sequence->composition->staticContent();
}

ID SequenceInfo::uniqueID() const {
  return sequence->composition->uniqueID;
}

int SequenceInfo::width() const {
  return sequence->width;
}

int SequenceInfo::height() const {
  return sequence->height;
}

Frame SequenceInfo::duration() const {
  return sequence->duration();
}

bool SequenceInfo::isVideo() const {
  return sequence->composition->type() == CompositionType::Video;
}

Frame SequenceInfo::firstVisibleFrame(const Layer* layer) const {
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
}  // namespace pag