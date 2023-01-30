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

#include "SequenceGenerator.h"

namespace pag {
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

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    auto reader = SequenceReader::Make(file, sequence);
    return reader->makeBuffer(0);
  }

 private:
  std::shared_ptr<File> file = nullptr;
  Sequence* sequence = nullptr;
};

class SequenceFrameBuffer : public tgfx::ImageBuffer {
 public:
  SequenceFrameBuffer(std::shared_ptr<SequenceReader> reader, Frame targetFrame)
      : reader(std::move(reader)), targetFrame(targetFrame) {
  }

  int width() const override {
    return reader->width();
  }

  int height() const override {
    return reader->height();
  }

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context, bool) const override {
    return reader->makeTexture(context, targetFrame);
  }

 private:
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame targetFrame = 0;
};

class SequenceImageGenerator : public tgfx::ImageGenerator {
 public:
  SequenceImageGenerator(std::shared_ptr<SequenceReader> reader, Frame targetFrame)
      : tgfx::ImageGenerator(reader->width(), reader->height()), reader(std::move(reader)),
        targetFrame(targetFrame) {
  }

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    auto success = reader->decodeFrame(targetFrame);
    if (!success) {
      return nullptr;
    }
    return std::shared_ptr<SequenceFrameBuffer>(new SequenceFrameBuffer(reader, targetFrame));
  }

 private:
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame targetFrame = 0;
};

std::shared_ptr<tgfx::ImageGenerator> SequenceGenerator::MakeFromStatic(std::shared_ptr<File> file,
                                                                        Sequence* sequence) {
  if (file == nullptr || sequence == nullptr || !sequence->composition->staticContent()) {
    return nullptr;
  }
  return std::make_shared<StaticSequenceGenerator>(std::move(file), sequence);
}

std::shared_ptr<tgfx::ImageGenerator> SequenceGenerator::MakeFromReader(
    std::shared_ptr<SequenceReader> reader, Frame targetFrame) {
  if (reader == nullptr) {
    return nullptr;
  }
  return std::make_shared<SequenceImageGenerator>(std::move(reader), targetFrame);
}
}  // namespace pag
