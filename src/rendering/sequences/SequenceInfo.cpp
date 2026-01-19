/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "DiskSequenceReader.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/core/Image.h"
#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/WebVideoSequenceDemuxer.h"
#endif

namespace pag {
static std::shared_ptr<tgfx::Image> MakeSequenceImage(
    std::shared_ptr<tgfx::ImageGenerator> generator, Sequence* sequence, bool useDiskCache) {
  auto image = tgfx::Image::MakeFrom(std::move(generator));
  if (!useDiskCache && sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    image = image->makeRGBAAA(sequence->width, sequence->height, videoSequence->alphaStartX,
                              videoSequence->alphaStartY);
  }
  return image;
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
                                                         PAGFile* pagFile, bool useDiskCache) {
  if (sequence == nullptr || file == nullptr) {
    return nullptr;
  }
  std::shared_ptr<SequenceReader> reader = nullptr;
  auto composition = sequence->composition;
  if (useDiskCache) {
    reader = DiskSequenceReader::Make(std::move(file), sequence);
    if (reader) {
      return reader;
    }
  }
  if (composition->type() == CompositionType::Bitmap) {
    reader = std::make_shared<BitmapSequenceReader>(std::move(file),
                                                    static_cast<BitmapSequence*>(sequence));
  } else {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
#ifdef PAG_BUILD_FOR_WEB
    auto demuxer =
        std::make_unique<WebVideoSequenceDemuxer>(std::move(file), videoSequence, pagFile);
#else
    auto demuxer = std::make_unique<VideoSequenceDemuxer>(std::move(file), videoSequence, pagFile);
#endif
    reader = std::make_shared<VideoReader>(std::move(demuxer));
  }
  return reader;
}

std::shared_ptr<tgfx::Image> SequenceInfo::makeStaticImage(std::shared_ptr<File> file,
                                                           bool useDiskCache) {
  if (sequence == nullptr || file == nullptr || !staticContent()) {
    return nullptr;
  }
  int width = sequence->width;
  int height = sequence->height;
  if (sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    width = videoSequence->getVideoWidth();
    height = videoSequence->getVideoHeight();
  }
  auto generator = std::make_shared<StaticSequenceGenerator>(std::move(file), weakThis.lock(),
                                                             width, height, useDiskCache);
  return MakeSequenceImage(std::move(generator), sequence, useDiskCache);
}

std::shared_ptr<tgfx::Image> SequenceInfo::makeFrameImage(std::shared_ptr<SequenceReader> reader,
                                                          Frame targetFrame, bool useDiskCache) {
  if (reader == nullptr || sequence == nullptr) {
    return nullptr;
  }
  auto generator = std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame);
  return MakeSequenceImage(std::move(generator), sequence, useDiskCache);
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
