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

#include "SequenceImage.h"

namespace pag {
class SequenceReaderBuffer : public tgfx::ImageBuffer {
 public:
  SequenceReaderBuffer(std::shared_ptr<SequenceReader> reader, Frame targetFrame)
      : reader(std::move(reader)), targetFrame(targetFrame) {
  }

  int width() const override {
    return reader->width();
  }

  int height() const override {
    return reader->height();
  }

  bool isAlphaOnly() const override {
    return false;
  }

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context, bool) const override {
    return reader->makeTexture(context, targetFrame);
  }

 private:
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame targetFrame = 0;
};

static tgfx::Size GetBufferSize(Sequence* sequence) {
  if (sequence->composition->type() == CompositionType::Video) {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
    return tgfx::Size::Make(videoSequence->getVideoWidth(), videoSequence->getVideoHeight());
  }
  return tgfx::Size::Make(sequence->width, sequence->height);
}

class StaticSequenceGenerator : public tgfx::ImageGenerator {
 public:
  StaticSequenceGenerator(std::shared_ptr<File> file, Sequence* sequence)
      : tgfx::ImageGenerator(GetBufferSize(sequence)), file(std::move(file)), sequence(sequence) {
  }

  bool isAlphaOnly() const override {
    return false;
  }

#ifdef PAG_BUILD_FOR_WEB
  bool asyncSupport() const override {
    return true;
  }
#endif

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    auto reader = SequenceReader::Make(file, sequence);
    auto buffer = reader->makeBuffer(0);
    if (buffer == nullptr) {
      buffer = std::shared_ptr<SequenceReaderBuffer>(new SequenceReaderBuffer(reader, 0));
    }
    return buffer;
  }

 private:
  std::shared_ptr<File> file = nullptr;
  Sequence* sequence = nullptr;
};

class SequenceFrameGenerator : public tgfx::ImageGenerator {
 public:
  SequenceFrameGenerator(std::shared_ptr<SequenceReader> reader, Frame targetFrame)
      : tgfx::ImageGenerator(reader->width(), reader->height()), reader(std::move(reader)),
        targetFrame(targetFrame) {
  }

  bool isAlphaOnly() const override {
    return false;
  }

#ifdef PAG_BUILD_FOR_WEB
  bool asyncSupport() const override {
    return true;
  }
#endif

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    auto success = reader->decodeFrame(targetFrame);
    if (!success) {
      return nullptr;
    }
    return std::shared_ptr<SequenceReaderBuffer>(new SequenceReaderBuffer(reader, targetFrame));
  }

 private:
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame targetFrame = 0;
};

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

std::shared_ptr<tgfx::Image> SequenceImage::MakeStatic(std::shared_ptr<File> file,
                                                       Sequence* sequence) {
  if (sequence == nullptr || file == nullptr || !sequence->composition->staticContent()) {
    return nullptr;
  }
  auto generator = std::make_shared<StaticSequenceGenerator>(std::move(file), sequence);
  return MakeSequenceImage(std::move(generator), sequence);
}

std::shared_ptr<tgfx::Image> SequenceImage::MakeFrom(std::shared_ptr<SequenceReader> reader,
                                                     Sequence* sequence, Frame targetFrame) {
  if (reader == nullptr || sequence == nullptr) {
    return nullptr;
  }
  auto generator = std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame);
  return MakeSequenceImage(std::move(generator), sequence);
}
}  // namespace pag
