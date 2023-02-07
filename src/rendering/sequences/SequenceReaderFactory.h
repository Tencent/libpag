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

#pragma once

#include "pag/file.h"
#include "rendering/sequences/SequenceReader.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/Orientation.h"

namespace pag {

/**
 * Defines the layout of a RGBAAA format image, which is half RGB, half AAA.
 */
struct RGBAAALayout {
  /**
   * The display width of the image.
   */
  int width;
  /**
   * The display height of the image.
   */
  int height;
  /**
   * The x position of where alpha area begins.
   */
  int alphaStartX;
  /**
   * The y position of where alpha area begins.
   */
  int alphaStartY;
};

class SequenceReaderFactory {
 public:
  explicit SequenceReaderFactory(Sequence* sequence);
  virtual ~SequenceReaderFactory() = default;

  virtual std::shared_ptr<SequenceReader> makeReader(std::shared_ptr<File> file,
                                                     PAGFile* pagFile = nullptr);
  virtual std::shared_ptr<tgfx::ImageGenerator> makeGenerator(std::shared_ptr<File> file);
  virtual std::shared_ptr<tgfx::ImageGenerator> makeGenerator(
      std::shared_ptr<SequenceReader> reader, Frame targetFrame);

  virtual bool staticContent() const;
  virtual ID uniqueID() const;
  virtual int width() const;
  virtual int height() const;
  virtual Frame duration() const;
  virtual bool isVideo() const;
  virtual RGBAAALayout layout() const;
  virtual Frame firstVisibleFrame(const Layer* layer) const;
  virtual tgfx::Orientation orientation() const;

 private:
  Sequence* sequence;
};

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

class StaticSequenceGenerator : public tgfx::ImageGenerator {
 public:
  StaticSequenceGenerator(std::shared_ptr<File> file, SequenceReaderFactory* factory,
                          tgfx::Size bufferSize)
      : tgfx::ImageGenerator(bufferSize), file(std::move(file)), factory(factory) {
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
    auto reader = factory->makeReader(file);
    auto buffer = reader->makeBuffer(0);
    if (buffer == nullptr) {
      buffer = std::shared_ptr<SequenceReaderBuffer>(new SequenceReaderBuffer(reader, 0));
    }
    return buffer;
  }

 private:
  std::shared_ptr<File> file = nullptr;
  SequenceReaderFactory* factory = nullptr;
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

}  // namespace pag
